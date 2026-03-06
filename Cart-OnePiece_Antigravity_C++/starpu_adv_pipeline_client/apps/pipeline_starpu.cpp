#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <boost/shared_ptr.hpp>

#include <carla/actors/ActorBlueprint.h>
#include <carla/actors/BlueprintLibrary.h>
#include <carla/client/Actor.h>
#include <carla/client/Client.h>
#include <carla/client/Map.h>
#include <carla/client/Sensor.h>
#include <carla/client/Vehicle.h>
#include <carla/client/World.h>
#include <carla/geom/Location.h>
#include <carla/geom/Rotation.h>
#include <carla/geom/Transform.h>
#include <carla/sensor/data/Image.h>

#include "frame_sync.hpp"
#include "metrics.hpp"
#include "pipeline_types.hpp"
#include "postprocess.hpp"
#include "preprocess.hpp"
#include "semantic_decode.hpp"
#include "trt_runner.hpp"
#include "viewer_sdl2.hpp"
#include "vis.hpp"

#include "starpu_codelets.hpp"
#include "starpu_runtime.hpp"
#include <starpu.h>

namespace cc = carla::client;
namespace cg = carla::geom;
namespace cs = carla::sensor::data;
namespace ca = carla::actors;

using namespace std::chrono;

std::atomic<bool> g_stop_requested(false);
void signal_handler(int signum) {
  std::cout << "\n[!] Interrupt signal (" << signum
            << ") received. Shutting down...\n";
  g_stop_requested = true;
}

// ---------------------------------------------------------
// Types & Queues
// ---------------------------------------------------------

enum class CameraView { Front, Rear };

struct EvalPayload {
  CameraView view = CameraView::Front;
  uint64_t frame_id;
  std::vector<uint8_t> pred_labels; // HxW
  GtFrame gt_frame;                 // Contains raw BGRA semantic from CARLA
  FrameIn rgb_frame;

  // Metrics variables
  double total_ms = 0;
  int dropped_before_process = 0;

  double rgb_ts = 0;
  double gt_ts = 0;
};

template <typename T> class SafeQueue {
public:
  void push(T item) {
    std::lock_guard<std::mutex> lock(m_);
    q_.push(std::move(item));
    c_.notify_one();
  }

  bool pop(T &item) {
    std::unique_lock<std::mutex> lock(m_);
    c_.wait(lock, [&]() { return !q_.empty() || g_stop_requested.load(); });
    if (q_.empty() || g_stop_requested.load())
      return false;
    item = std::move(q_.front());
    q_.pop();
    return true;
  }

  size_t size() {
    std::lock_guard<std::mutex> lock(m_);
    return q_.size();
  }

private:
  std::queue<T> q_;
  std::mutex m_;
  std::condition_variable c_;
};

SafeQueue<EvalPayload> g_eval_queue;

// ---------------------------------------------------------
// Evaluation Thread
// ---------------------------------------------------------

struct EvalConfig {
  bool enabled = true;
  bool exclude_zero = true;
  int print_every = 50;
  std::vector<uint8_t> specific_classes;
  int out_w = 0;
  int out_h = 0;
  SegmentationViewerSDL2 *viewer = nullptr;
};

static void ResizeLabelsNearest(const uint8_t *src, int in_w, int in_h,
                                uint8_t *dst, int out_w, int out_h) {
  float scale_x = static_cast<float>(in_w) / out_w;
  float scale_y = static_cast<float>(in_h) / out_h;

  for (int y = 0; y < out_h; ++y) {
    int src_y = static_cast<int>(y * scale_y);
    if (src_y >= in_h)
      src_y = in_h - 1;
    for (int x = 0; x < out_w; ++x) {
      int src_x = static_cast<int>(x * scale_x);
      if (src_x >= in_w)
        src_x = in_w - 1;
      dst[y * out_w + x] = src[src_y * in_w + src_x];
    }
  }
}

// Global pipeline stat output
std::string g_pipeline_csv_path;
std::mutex g_csv_mu;
std::ofstream g_csv_file;

// Task profiling output
std::mutex g_task_csv_mu;
std::ofstream g_task_csv_file;

std::mutex g_all_tasks_mu;
std::vector<struct starpu_task *> g_all_tasks;

struct EpilogueArg {
  uint64_t frame_id;
  const char *stage;
};

void task_epilogue_callback(void *arg) {
  EpilogueArg *ea = static_cast<EpilogueArg *>(arg);
  struct starpu_task *task = starpu_task_get_current();

  if (task && task->profiling_info) {
    auto pinfo = task->profiling_info;
    uint64_t submit_ns =
        pinfo->submit_time.tv_sec * 1000000000ULL + pinfo->submit_time.tv_nsec;
    uint64_t start_ns =
        pinfo->start_time.tv_sec * 1000000000ULL + pinfo->start_time.tv_nsec;
    uint64_t end_ns =
        pinfo->end_time.tv_sec * 1000000000ULL + pinfo->end_time.tv_nsec;
    double duration_ms = (end_ns - start_ns) / 1e6;

    std::string worker_type = "CPU";
    if (starpu_worker_get_type(pinfo->workerid) == STARPU_CUDA_WORKER) {
      worker_type = "CUDA";
    }

    std::lock_guard<std::mutex> lck(g_task_csv_mu);
    if (g_task_csv_file.is_open()) {
      g_task_csv_file << ea->frame_id << "," << ea->stage << "," << submit_ns
                      << "," << start_ns << "," << end_ns << "," << worker_type
                      << "," << pinfo->workerid << "," << duration_ms << "\n";
    }
  }
}

void evaluate_thread(const EvalConfig &cfg) {
  int count_front = 0;
  double sum_pa_front = 0.0;
  double sum_miou_front = 0.0;

  int count_rear = 0;
  double sum_pa_rear = 0.0;
  double sum_miou_rear = 0.0;

  while (!g_stop_requested) {
    EvalPayload payload;
    if (!g_eval_queue.pop(payload)) {
      break;
    }

    std::vector<uint8_t> dec_gt = DecodeSemanticLabels(
        reinterpret_cast<const uint8_t *>(payload.gt_frame.carla_image->data()),
        payload.gt_frame.w, payload.gt_frame.h, true);

    std::vector<uint8_t> *target_gt = &dec_gt;
    std::vector<uint8_t> resized_gt;
    if (cfg.out_w != payload.gt_frame.w || cfg.out_h != payload.gt_frame.h) {
      resized_gt.resize(cfg.out_w * cfg.out_h);
      ResizeLabelsNearest(dec_gt.data(), payload.gt_frame.w, payload.gt_frame.h,
                          resized_gt.data(), cfg.out_w, cfg.out_h);
      target_gt = &resized_gt;
    }

    double pa = 0.0;
    double miou = 0.0;

    if (cfg.viewer) {
      int w = payload.rgb_frame.w;
      int h = payload.rgb_frame.h;
      std::vector<uint8_t> rgb_bytes(w * h * 3);
      vis::BgraToRgb(reinterpret_cast<const uint8_t *>(
                         payload.rgb_frame.carla_image->data()),
                     w, h, rgb_bytes.data());

      std::vector<uint8_t> pred_upsampled(w * h);
      vis::UpsampleNearest(payload.pred_labels.data(), cfg.out_w, cfg.out_h,
                           pred_upsampled.data(), w, h);

      std::vector<uint8_t> color_img(w * h * 3);
      vis::ColorizeCityscapes(pred_upsampled.data(), w, h, color_img.data());

      std::vector<uint8_t> overlay(w * h * 3);
      vis::BlendOverlay(rgb_bytes.data(), color_img.data(), w, h, 0.5f,
                        overlay.data());

      cfg.viewer->submit_frame_rgb888(overlay.data(), w, h);
    }

    if (cfg.enabled) {
      metrics::EvalResult res =
          metrics::EvaluateFrame(payload.pred_labels.data(), target_gt->data(),
                                 cfg.out_w, cfg.out_h, cfg.exclude_zero);
      pa = res.pixel_accuracy;
      miou = res.miou;

      std::string view_str;
      if (payload.view == CameraView::Front) {
        sum_pa_front += pa;
        sum_miou_front += miou;
        count_front++;
        view_str = "Front";
        if (count_front % cfg.print_every == 0) {
          std::cout << "[Eval] " << view_str
                    << " Frames evaluated: " << count_front
                    << " | Avg PA: " << (sum_pa_front / count_front)
                    << " | Avg mIoU: " << (sum_miou_front / count_front)
                    << "\n";
        }
      } else {
        sum_pa_rear += pa;
        sum_miou_rear += miou;
        count_rear++;
        view_str = "Rear ";
        if (count_rear % cfg.print_every == 0) {
          std::cout << "[Eval] " << view_str
                    << " Frames evaluated: " << count_rear
                    << " | Avg PA: " << (sum_pa_rear / count_rear)
                    << " | Avg mIoU: " << (sum_miou_rear / count_rear) << "\n";
        }
      }
    }

    // Write pipeline.csv
    {
      std::lock_guard<std::mutex> lck(g_csv_mu);
      if (g_csv_file.is_open()) {
        std::string view_prefix =
            (payload.view == CameraView::Front) ? "front" : "rear";
        g_csv_file << view_prefix << "," << payload.frame_id << ","
                   << payload.rgb_ts << "," << payload.gt_ts << ","
                   << payload.total_ms << "," << payload.dropped_before_process
                   << "," << pa << "," << miou << "\n";
      }
    }
  }
  std::cout << "[Eval] Thread finished.\n";
}

// ---------------------------------------------------------
// Pipeline StarPU Slots
// ---------------------------------------------------------

struct InflightSlot {
  CameraView view = CameraView::Front;
  int id;
  std::vector<uint8_t> in_bgra;
  std::vector<float> tensor_in;
  std::vector<uint8_t> raw_infer_out;
  std::vector<uint8_t> pred_out;

  starpu_data_handle_t handle_bgra;
  starpu_data_handle_t handle_in;
  starpu_data_handle_t handle_raw;
  starpu_data_handle_t handle_pred;

  EpilogueArg pre_epi;
  EpilogueArg inf_epi;
  EpilogueArg post_epi;

  PreprocArgs pre_args;
  InferArgs inf_args;
  PostArgs post_args;

  MatchedPair match;
  int dropped;
  time_point<high_resolution_clock> t_start;
};

class SlotManager {
public:
  SlotManager(int inflight_count, int out_w, int out_h, size_t raw_out_bytes) {
    slots.resize(inflight_count);
    for (int i = 0; i < inflight_count; ++i) {
      slots[i].id = i;
      slots[i].in_bgra.resize(out_w * 4 /* ratio */ * out_h * 4 /* ratio */ *
                              4); // Just big enough
      slots[i].tensor_in.resize(out_w * out_h * 3);
      slots[i].raw_infer_out.resize(raw_out_bytes);
      slots[i].pred_out.resize(out_w * out_h);

      starpu_memory_pin(slots[i].in_bgra.data(), slots[i].in_bgra.size());
      starpu_memory_pin(slots[i].tensor_in.data(),
                        slots[i].tensor_in.size() * sizeof(float));
      starpu_memory_pin(slots[i].raw_infer_out.data(),
                        slots[i].raw_infer_out.size());
      starpu_memory_pin(slots[i].pred_out.data(), slots[i].pred_out.size());

      starpu_vector_data_register(&slots[i].handle_bgra, STARPU_MAIN_RAM,
                                  (uintptr_t)slots[i].in_bgra.data(),
                                  slots[i].in_bgra.size(), sizeof(uint8_t));
      starpu_vector_data_register(&slots[i].handle_in, STARPU_MAIN_RAM,
                                  (uintptr_t)slots[i].tensor_in.data(),
                                  slots[i].tensor_in.size(), sizeof(float));
      starpu_vector_data_register(&slots[i].handle_raw, STARPU_MAIN_RAM,
                                  (uintptr_t)slots[i].raw_infer_out.data(),
                                  slots[i].raw_infer_out.size(), 1);
      starpu_vector_data_register(&slots[i].handle_pred, STARPU_MAIN_RAM,
                                  (uintptr_t)slots[i].pred_out.data(),
                                  slots[i].pred_out.size(), sizeof(uint8_t));

      free_slots.push(&slots[i]);
    }
  }

  ~SlotManager() {
    for (auto &s : slots) {
      // By-passing StarPU unregistration entirely.
      // Calling unregister triggers implicit tasks which segfault the FxT
      // tracker inside `_create_timer` when processing the deep job tracking
      // tree. The OS will recycle process memory seamlessly.

      starpu_memory_unpin(s.in_bgra.data(), s.in_bgra.size());
      starpu_memory_unpin(s.tensor_in.data(),
                          s.tensor_in.size() * sizeof(float));
      starpu_memory_unpin(s.raw_infer_out.data(), s.raw_infer_out.size());
      starpu_memory_unpin(s.pred_out.data(), s.pred_out.size());
    }
  }

  InflightSlot *acquire() {
    std::unique_lock<std::mutex> lock(m_);
    if (!c_.wait_for(lock, std::chrono::milliseconds(2000), [this] {
          return !free_slots.empty() || g_stop_requested;
        })) {
      return nullptr;
    }
    if (g_stop_requested)
      return nullptr;
    auto s = free_slots.front();
    free_slots.pop();
    return s;
  }

  void release(InflightSlot *s) {
    std::lock_guard<std::mutex> lock(m_);
    free_slots.push(s);
    c_.notify_one();
  }

private:
  std::vector<InflightSlot> slots;
  std::queue<InflightSlot *> free_slots;
  std::mutex m_;
  std::condition_variable c_;
};

SlotManager *g_slots = nullptr;

// Post task callback
void post_finish_callback(void *arg) {
  InflightSlot *s = static_cast<InflightSlot *>(arg);

  auto t_end = high_resolution_clock::now();
  double total_ms = duration<double, std::milli>(t_end - s->t_start).count();

  EvalPayload ep;
  ep.view = s->view;
  ep.frame_id = s->match.frame_id;
  ep.gt_frame = std::move(s->match.gt);
  ep.pred_labels = s->pred_out; // Copy
  ep.rgb_frame = std::move(s->match.rgb);
  ep.total_ms = total_ms;
  ep.dropped_before_process = s->dropped;
  ep.rgb_ts = s->match.rgb.timestamp;
  ep.gt_ts = s->match.gt.timestamp;

  g_eval_queue.push(std::move(ep));

  // Release slot back to pool
  g_slots->release(s);
}

// ---------------------------------------------------------
// Main
// ---------------------------------------------------------

int main(int argc, char **argv) {
  std::signal(SIGINT, signal_handler);

  std::string host = "127.0.0.1";
  uint16_t port = 2000;
  std::string map_name = "";
  int w = 800;
  int h = 600;
  int fps = 20;
  int max_frames = 200;
  int out_w = 512;
  int out_h = 256;
  bool assume_bgra = true;
  std::string resize_type = "bilinear";
  std::string engine_path = "models/dummy.engine";
  int print_every = 20;
  int eval_every = 50;
  int sync_window = 20;
  int inflight_pairs = 2;
  int cpu_workers = 4;
  bool do_eval = true;
  float mean_vals[3] = {0.485f, 0.456f, 0.406f};
  float std_vals[3] = {0.229f, 0.224f, 0.225f};
  bool display = false;
  bool print_stats = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--host" && i + 1 < argc)
      host = argv[++i];
    else if (arg == "--port" && i + 1 < argc)
      port = std::stoi(argv[++i]);
    else if (arg == "--map" && i + 1 < argc)
      map_name = argv[++i];
    else if (arg == "--w" && i + 1 < argc)
      w = std::stoi(argv[++i]);
    else if (arg == "--h" && i + 1 < argc)
      h = std::stoi(argv[++i]);
    else if (arg == "--fps" && i + 1 < argc)
      fps = std::stoi(argv[++i]);
    else if (arg == "--frames" && i + 1 < argc)
      max_frames = std::stoi(argv[++i]);
    else if (arg == "--engine" && i + 1 < argc)
      engine_path = argv[++i];
    else if (arg == "--out_w" && i + 1 < argc)
      out_w = std::stoi(argv[++i]);
    else if (arg == "--out_h" && i + 1 < argc)
      out_h = std::stoi(argv[++i]);
    else if (arg == "--assume_bgra" && i + 1 < argc)
      assume_bgra = (std::stoi(argv[++i]) != 0);
    else if (arg == "--resize" && i + 1 < argc)
      resize_type = argv[++i];
    else if (arg == "--print_every" && i + 1 < argc)
      print_every = std::stoi(argv[++i]);
    else if (arg == "--eval_every" && i + 1 < argc)
      eval_every = std::stoi(argv[++i]);
    else if (arg == "--sync_window" && i + 1 < argc)
      sync_window = std::stoi(argv[++i]);
    else if (arg == "--inflight" && i + 1 < argc)
      inflight_pairs = std::stoi(argv[++i]);
    else if (arg == "--cpu_workers" && i + 1 < argc)
      cpu_workers = std::stoi(argv[++i]);
    else if (arg == "--display" && i + 1 < argc)
      display = (std::stoi(argv[++i]) != 0);
    else if (arg == "--no_eval")
      do_eval = false;
    else if (arg == "--print_stats")
      print_stats = true;
  }

  std::cout << "[Pipeline] StarPU pipeline starting.\n";

  // Init StarPU
  std::unique_ptr<StarpuRuntime> spu_rt;
  try {
    spu_rt = std::make_unique<StarpuRuntime>(cpu_workers);
  } catch (const std::exception &e) {
    std::cerr << "Fatal StarPU Init Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  // Set up CSV
  const char *env_dir = std::getenv("RUN_DIR");
  std::string run_dir = env_dir ? env_dir : ".";
  g_pipeline_csv_path = run_dir + "/pipeline_starpu.csv";
  g_csv_file.open(g_pipeline_csv_path);
  if (g_csv_file.is_open()) {
    g_csv_file << "view,frame_id,rgb_ts,gt_ts,total_latency_ms,dropped_before_"
                  "process,pa,miou\n";
  }

  std::string task_csv_path = run_dir + "/starpu_tasks.csv";
  g_task_csv_file.open(task_csv_path);
  if (g_task_csv_file.is_open()) {
    g_task_csv_file << "frame_id,stage,submit_ns,start_ns,end_ns,worker_type,"
                       "worker_id,duration_ms\n";
  }

  // Load TRT
  TrtRunner runner;
  if (!runner.LoadEngine(engine_path)) {
    std::cerr << "Failed to load engine.\n";
    return EXIT_FAILURE;
  }
  if (!runner.Init()) {
    std::cerr << "TRT Init failed.\n";
    return EXIT_FAILURE;
  }
  runner.SetInputShapeIfDynamic(out_w, out_h);

  int out_c = 1;
  bool is_logits = true;
  size_t expected_label_bytes = out_w * out_h * sizeof(int32_t);
  if (runner.GetOutputBytes() == expected_label_bytes) {
    std::cout << "[Pipeline] Detected DIRECT label map output.\n";
    is_logits = false;
  } else {
    out_c = runner.GetOutputBytes() / (out_w * out_h * sizeof(float));
    std::cout << "[Pipeline] Detected logit output format. Channels=" << out_c
              << "\n";
  }

  // Init Slots
  g_slots =
      new SlotManager(inflight_pairs, out_w, out_h, runner.GetOutputBytes());

  std::unique_ptr<SegmentationViewerSDL2> viewer;
  if (display) {
    viewer = std::make_unique<SegmentationViewerSDL2>();
    if (!viewer->init(w, h, 1.0f)) {
      std::cerr << "[Pipeline] Viewer init failed, running headless.\n";
      viewer.reset();
    }
  }

  // Eval Thread
  EvalConfig ecfg;
  ecfg.enabled = do_eval;
  ecfg.specific_classes = {7, 8}; // roads and sidewalks
  ecfg.out_w = out_w;
  ecfg.out_h = out_h;
  ecfg.print_every = eval_every;
  ecfg.viewer = viewer.get();
  std::thread eval_t(evaluate_thread, ecfg);

  // CARLA setup
  auto client = cc::Client(host, port);
  client.SetTimeout(std::chrono::seconds(10));
  if (!map_name.empty())
    client.LoadWorld(map_name);
  auto world = client.GetWorld();
  auto map = world.GetMap();

  auto tm = client.GetInstanceTM();
  tm.SetSynchronousMode(true);
  uint16_t tm_port = 8000;

  auto blueprint_library = world.GetBlueprintLibrary();
  auto vehicle_bp = *(blueprint_library->Find("vehicle.tesla.model3"));
  auto spawn_points = map->GetRecommendedSpawnPoints();
  auto spawn_point = spawn_points[0];
  spawn_point.location.z += 1.0f;
  auto vehicle = world.SpawnActor(vehicle_bp, spawn_point);
  vehicle->SetSimulatePhysics(true);
  boost::static_pointer_cast<cc::Vehicle>(vehicle)->SetAutopilot(true, tm_port);

  cg::Transform front_tf(cg::Location(0.0f, 0.0f, 2.0f),
                         cg::Rotation(0.0f, 0.0f, 0.0f));
  cg::Transform rear_tf(cg::Location(-2.0f, 0.0f, 2.0f),
                        cg::Rotation(0.0f, 180.0f, 0.0f));

  auto rgb_bp = *(blueprint_library->Find("sensor.camera.rgb"));
  rgb_bp.SetAttribute("image_size_x", std::to_string(w));
  rgb_bp.SetAttribute("image_size_y", std::to_string(h));
  rgb_bp.SetAttribute("sensor_tick", std::to_string(1.0f / fps));

  auto front_rgb = world.SpawnActor(rgb_bp, front_tf, vehicle.get());
  auto rear_rgb = world.SpawnActor(rgb_bp, rear_tf, vehicle.get());

  auto gt_bp =
      *(blueprint_library->Find("sensor.camera.semantic_segmentation"));
  gt_bp.SetAttribute("image_size_x", std::to_string(w));
  gt_bp.SetAttribute("image_size_y", std::to_string(h));
  gt_bp.SetAttribute("sensor_tick", std::to_string(1.0f / fps));

  auto front_gt = world.SpawnActor(gt_bp, front_tf, vehicle.get());
  auto rear_gt = world.SpawnActor(gt_bp, rear_tf, vehicle.get());

  FrameSync front_sync(sync_window);
  FrameSync rear_sync(sync_window);

  boost::static_pointer_cast<cc::Sensor>(front_rgb)->Listen(
      [&front_sync, w, h](auto data) {
        if (g_stop_requested)
          return;
        auto image = boost::static_pointer_cast<cs::Image>(data);
        FrameIn f;
        f.frame_id = image->GetFrame();
        f.timestamp = image->GetTimestamp();
        f.w = image->GetWidth();
        f.h = image->GetHeight();
        f.carla_image = image;
        front_sync.PushRgb(std::move(f));
      });

  boost::static_pointer_cast<cc::Sensor>(rear_rgb)->Listen(
      [&rear_sync, w, h](auto data) {
        if (g_stop_requested)
          return;
        auto image = boost::static_pointer_cast<cs::Image>(data);
        FrameIn f;
        f.frame_id = image->GetFrame();
        f.timestamp = image->GetTimestamp();
        f.w = image->GetWidth();
        f.h = image->GetHeight();
        f.carla_image = image;
        rear_sync.PushRgb(std::move(f));
      });

  boost::static_pointer_cast<cc::Sensor>(front_gt)->Listen(
      [&front_sync, w, h](auto data) {
        if (g_stop_requested)
          return;
        auto image = boost::static_pointer_cast<cs::Image>(data);
        GtFrame g;
        g.frame_id = image->GetFrame();
        g.timestamp = image->GetTimestamp();
        g.w = image->GetWidth();
        g.h = image->GetHeight();
        g.carla_image = image;
        front_sync.PushGt(std::move(g));
      });

  boost::static_pointer_cast<cc::Sensor>(rear_gt)->Listen(
      [&rear_sync, w, h](auto data) {
        if (g_stop_requested)
          return;
        auto image = boost::static_pointer_cast<cs::Image>(data);
        GtFrame g;
        g.frame_id = image->GetFrame();
        g.timestamp = image->GetTimestamp();
        g.w = image->GetWidth();
        g.h = image->GetHeight();
        g.carla_image = image;
        rear_sync.PushGt(std::move(g));
      });

  auto settings = world.GetSettings();
  bool original_sync = settings.synchronous_mode;
  settings.synchronous_mode = true;
  settings.fixed_delta_seconds = 1.0f / fps;
  world.ApplySettings(settings, std::chrono::seconds(2));

  int frames_processed = 0;
  int dropped = 0;

  PreprocessConfig pre_cfg;
  pre_cfg.out_w = out_w;
  pre_cfg.out_h = out_h;
  pre_cfg.assume_bgra = assume_bgra;
  pre_cfg.to_rgb = true;
  pre_cfg.mean[0] = mean_vals[0];
  pre_cfg.mean[1] = mean_vals[1];
  pre_cfg.mean[2] = mean_vals[2];
  pre_cfg.std[0] = std_vals[0];
  pre_cfg.std[1] = std_vals[1];
  pre_cfg.std[2] = std_vals[2];
  pre_cfg.resize = (resize_type == "bilinear") ? PreprocessConfig::BILINEAR
                                               : PreprocessConfig::NEAREST;

  std::cout << "[Pipeline] Loop started. FPS: " << fps
            << " Frames: " << max_frames << "\n";

  while (!g_stop_requested && frames_processed < max_frames) {
    if (viewer) {
      if (viewer->poll_events()) {
        g_stop_requested = true;
        break;
      }
      viewer->render_latest();
    }

    world.Tick(std::chrono::seconds(2));

    auto process_camera = [&](FrameSync &sync_instance, CameraView view_tag,
                              int &dropped_counter, int &processed_counter) {
      MatchedPair pair;
      MatchedPair latest_pair;
      bool found = false;
      int popped = 0;

      for (int retry = 0; retry < 50; retry++) {
        while (sync_instance.TryPopMatched(pair)) {
          latest_pair = std::move(pair);
          found = true;
          popped++;
        }
        if (found)
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }

      if (found) {
        if (popped > inflight_pairs) {
          dropped_counter += (popped - 1);
        }
        auto t_start = high_resolution_clock::now();

        // Acquire slot
        InflightSlot *s = g_slots->acquire();
        if (!s) {
          // Stop requested or timeout
          return false;
        }
        s->view = view_tag;
        s->match = std::move(latest_pair);
        s->dropped = dropped_counter;
        s->t_start = t_start;
        dropped_counter = 0;

        // Register raw image buffer temporarily
        // Copy BGRA memory to statically registered slot buffer
        size_t img_bytes = s->match.rgb.w * s->match.rgb.h * 4;
        std::memcpy(s->in_bgra.data(), s->match.rgb.carla_image->data(),
                    img_bytes);

        s->pre_args = {s->match.frame_id, s->match.rgb.w, s->match.rgb.h,
                       pre_cfg};
        s->inf_args = {s->match.frame_id, out_c,  out_h, out_w,
                       is_logits,         &runner};
        s->post_args = {s->match.frame_id, out_c, out_h, out_w, is_logits};

        // TASK 1: Preproc
        struct starpu_task *t1 = starpu_task_create();
        t1->cl = &cl_preproc;
        t1->handles[0] = s->handle_bgra;
        t1->handles[1] = s->handle_in;
        t1->cl_arg = std::malloc(sizeof(PreprocArgs));
        std::memcpy(t1->cl_arg, &s->pre_args, sizeof(PreprocArgs));
        t1->cl_arg_size = sizeof(PreprocArgs);
        t1->cl_arg_free = 1;
        t1->destroy = 0; // PREVENT USE-AFTER-FREE IN TRACER
        starpu_task_submit(t1);

        // TASK 2: Infer
        struct starpu_task *t2 = starpu_task_create();
        t2->cl = &cl_infer_trt;
        t2->handles[0] = s->handle_in;
        t2->handles[1] = s->handle_raw;
        t2->cl_arg = std::malloc(sizeof(InferArgs));
        std::memcpy(t2->cl_arg, &s->inf_args, sizeof(InferArgs));
        t2->cl_arg_size = sizeof(InferArgs);
        t2->cl_arg_free = 1;
        t2->destroy = 0; // PREVENT USE-AFTER-FREE IN TRACER
        starpu_task_submit(t2);

        // TASK 3: Post
        struct starpu_task *t3 = starpu_task_create();
        t3->cl = &cl_post;
        t3->handles[0] = s->handle_raw;
        t3->handles[1] = s->handle_pred;
        t3->cl_arg = std::malloc(sizeof(PostArgs));
        std::memcpy(t3->cl_arg, &s->post_args, sizeof(PostArgs));
        t3->cl_arg_size = sizeof(PostArgs);
        t3->cl_arg_free = 1;
        t3->destroy = 0; // PREVENT USE-AFTER-FREE IN TRACER
        t3->callback_func = post_finish_callback;
        t3->callback_arg = s;
        t3->callback_arg_free = 0; // The slot manager manages this pointer
        starpu_task_submit(t3);

        {
          std::lock_guard<std::mutex> lk(g_all_tasks_mu);
          g_all_tasks.push_back(t1);
          g_all_tasks.push_back(t2);
          g_all_tasks.push_back(t3);
        }

        processed_counter++;
        if (processed_counter % print_every == 0) {
          std::string l = (view_tag == CameraView::Front) ? "front" : "rear";
          std::cout << "[Pipeline] Dispatched " << l << " Frame "
                    << processed_counter << "/" << max_frames << "\n";
        }
      }
      return true;
    };

    if (!process_camera(front_sync, CameraView::Front, dropped,
                        frames_processed))
      break;

    // We maintain frames_processed loosely around the front camera, but we
    // process both equally. If the front breaks early, we terminate.
    int dummy_rear_dropped = 0;
    int dummy_rear_processed = 0;
    process_camera(rear_sync, CameraView::Rear, dummy_rear_dropped,
                   dummy_rear_processed);
  }

  std::cout << "[Pipeline] Loop exit. Waiting for StarPU to drain...\n";
  starpu_task_wait_for_all();

  g_stop_requested = true;
  g_eval_queue.push(EvalPayload()); // Wake up eval thread
  eval_t.join();

  // Stop FxT trace BEFORE data unregistration to prevent FxT from analyzing
  // tear-down sync tasks
  starpu_fxt_stop_profiling();

  delete g_slots;

  // Now that all StarPU sync tasks related to unregistration have been
  // submitted and completed, we can safely destroy the tasks without corrupting
  // tracer memory trees!
  for (auto t : g_all_tasks) {
    starpu_task_destroy(t);
  }

  std::cout << "[Pipeline] Exiting cleanly.\n";

  // Stop and destroy CARLA sensors and vehicles to prevent ASIO faults
  // and zombie actors persisting between sequential profiling runs.
  if (front_rgb) {
    boost::static_pointer_cast<cc::Sensor>(front_rgb)->Stop();
    front_rgb->Destroy();
  }
  if (front_gt) {
    boost::static_pointer_cast<cc::Sensor>(front_gt)->Stop();
    front_gt->Destroy();
  }
  if (rear_rgb) {
    boost::static_pointer_cast<cc::Sensor>(rear_rgb)->Stop();
    rear_rgb->Destroy();
  }
  if (rear_gt) {
    boost::static_pointer_cast<cc::Sensor>(rear_gt)->Stop();
    rear_gt->Destroy();
  }
  if (vehicle) {
    vehicle->Destroy();
  }

  // Extract StarPU profile
  starpu_profiling_status_set(STARPU_PROFILING_DISABLE);

  if (print_stats) {
    fflush(stdout);
    printf("\n===STARPU_STATS_BEGIN===\n");
    fflush(stdout);
    starpu_profiling_bus_helper_display_summary();
    starpu_profiling_worker_helper_display_summary();
    starpu_data_display_memory_stats();
    fflush(stderr); // Ensure stats (stderr) are flushed
    printf("===STARPU_STATS_END===\n");
    fflush(stdout);
  }

  return EXIT_SUCCESS;
}
