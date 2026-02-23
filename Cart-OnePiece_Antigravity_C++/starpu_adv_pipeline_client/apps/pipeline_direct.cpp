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

#include <carla/actors/ActorBlueprint.h>
#include <carla/actors/BlueprintLibrary.h>
#include <carla/client/Actor.h>
#include <carla/client/Client.h>
#include <carla/client/Map.h>
#include <carla/client/Sensor.h>
#include <carla/client/Vehicle.h>
#include <carla/client/World.h>
#include <carla/geom/Transform.h>
#include <carla/sensor/data/Image.h>

#include <boost/shared_ptr.hpp>

#include "frame_sync.hpp"
#include "metrics.hpp"
#include "pipeline_types.hpp"
#include "postprocess.hpp"
#include "preprocess.hpp"
#include "semantic_decode.hpp"
#include "trt_runner.hpp"

namespace cc = carla::client;
namespace cg = carla::geom;
namespace cs = carla::sensor::data;
namespace ca = carla::actors;

std::atomic<bool> g_stop_requested(false);
void signal_handler(int signum) {
  std::cout << "\n[!] Interrupt signal (" << signum
            << ") received. Shutting down...\n";
  g_stop_requested = true;
}

// ---------------------------------------------------------
// Types & Queues
// ---------------------------------------------------------

struct EvalPayload {
  uint64_t frame_id;
  std::vector<uint8_t> pred_labels; // HxW
  GtFrame gt_frame;                 // Contains raw BGRA semantic from CARLA

  // Metrics variables
  double preproc_ms = 0;
  double infer_ms = 0;
  double post_ms = 0;
  double total_ms = 0;
  int dropped_before_process = 0;
};

// ---------------------------------------------------------
// Safe Thread Queue
// ---------------------------------------------------------

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
// Metric Thread (Thread C)
// ---------------------------------------------------------

struct EvalConfig {
  bool enabled = true;
  bool exclude_zero = true;
  int print_every = 50;
  std::string csv_path;
  std::vector<uint8_t> specific_classes; // If empty, compute auto dynamically
  int out_w = 0;
  int out_h = 0;
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

void evaluate_thread(const EvalConfig &cfg) {
  int count = 0;
  double sum_pa = 0.0;
  double sum_miou = 0.0;

  std::ofstream csv;
  if (!cfg.csv_path.empty()) {
    csv.open(cfg.csv_path);
    if (csv.is_open()) {
      csv << "frame_id,t_capture_rgb,t_capture_gt,preproc_ms,infer_ms,post_ms,"
             "total_ms,dropped_before_process,pa,miou\n";
    }
  }

  while (!g_stop_requested) {
    EvalPayload payload;
    if (!g_eval_queue.pop(payload)) {
      break; // Stop requested
    }

    // Decode GT
    std::vector<uint8_t> dec_gt = DecodeSemanticLabels(
        reinterpret_cast<const uint8_t *>(payload.gt_frame.carla_image->data()),
        payload.gt_frame.w, payload.gt_frame.h,
        true // assume bgra
    );

    // If engine output resolution != GT resolution, we'd resize here,
    // For Step 8 simplicity, we expect the engine output to be evaluated over
    // the scaled dimensions, so to perform eval, we need the GT at network
    // shape, OR predictions at sensor shape. Assuming for Step 8 that we
    // evaluate at network shape (which allows smaller GT for quick
    // comparisons): (If out_w != gt_w, we'll resize GT via nearest neighbor to
    // align with PredOut logic)

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

    if (cfg.enabled) {
      if (cfg.specific_classes.empty()) {
        metrics::EvalResult res = metrics::EvaluateFrame(
            payload.pred_labels.data(), target_gt->data(), cfg.out_w, cfg.out_h,
            cfg.exclude_zero);
        pa = res.pixel_accuracy;
        miou = res.miou;
      } else {
        metrics::EvalResult res;
        miou =
            metrics::MeanIoU(payload.pred_labels.data(), target_gt->data(),
                             cfg.out_w, cfg.out_h, cfg.specific_classes, &res);
        pa = res.pixel_accuracy;
      }
      sum_pa += pa;
      sum_miou += miou;
      count++;

      if (count % cfg.print_every == 0) {
        std::cout << "[Eval] Frames evaluated: " << count
                  << " | Avg PA: " << (sum_pa / count)
                  << " | Avg mIoU: " << (sum_miou / count) << std::endl;
      }
    }

    if (csv.is_open()) {
      csv << payload.frame_id << "," << 0.0
          << "," // Not tracking full timestamps deep inside EvalPayload for
                 // now, simplificiation
          << payload.gt_frame.timestamp << "," << payload.preproc_ms << ","
          << payload.infer_ms << "," << payload.post_ms << ","
          << payload.total_ms << "," << payload.dropped_before_process << ","
          << pa << "," << miou << "\n";
    }
  }

  if (csv.is_open())
    csv.close();
  std::cout << "[Eval] Thread finished." << std::endl;
}

// ---------------------------------------------------------
// Main Pipeline (Thread B logic)
// ---------------------------------------------------------

int main(int argc, char **argv) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Default params
  std::string host = "127.0.0.1";
  uint16_t port = 2000;
  int w = 800;
  int h = 600;
  int fps = 20;
  int max_frames = 300;
  std::string engine_path = "models/seg.engine";
  int out_w = 512;
  int out_h = 256;
  bool assume_bgra = true;
  std::string resize_type = "bilinear";
  int print_every = 20;
  int eval_every = 50;
  int sync_window = 50;
  int inflight_pairs = 2;
  bool disable_eval = false;
  std::string classes_arg = "all";
  std::vector<float> mean_vals = {0.485f, 0.456f, 0.406f};
  std::vector<float> std_vals = {0.229f, 0.224f, 0.225f};

  // Very basic arg parsing
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--host" && i + 1 < argc)
      host = argv[++i];
    else if (arg == "--port" && i + 1 < argc)
      port = std::stoi(argv[++i]);
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
    else if (arg == "--inflight_pairs" && i + 1 < argc)
      inflight_pairs = std::stoi(argv[++i]);
    else if (arg == "--classes" && i + 1 < argc)
      classes_arg = argv[++i];
    else if (arg == "--no_eval")
      disable_eval = true;
  }

  std::cout << "[Pipeline] End-to-End Direct started." << std::endl;

  // --- Init TRT ---
  TrtRunner runner;
  if (!runner.LoadEngine(engine_path))
    return EXIT_FAILURE;
  if (!runner.Init())
    return EXIT_FAILURE;
  if (!runner.SetInputShapeIfDynamic(out_w, out_h))
    return EXIT_FAILURE;

  // Output parsing detection (Logits vs Labels):
  // For Step 8, we expect either [1, C, H, W] (Logits) or [1, H, W] (Labels) or
  // [H,W] (Labels)
  int out_c = 0;
  bool is_logits = false;
  // HACK: Use total elements to infer format if strict dimension counting fails
  size_t engine_output_elements = runner.GetOutputBytes() / 4; // float or int32
  if (engine_output_elements > static_cast<size_t>(out_w) * out_h) {
    out_c = engine_output_elements / (out_w * out_h);
    is_logits = true;
    std::cout << "[Pipeline] Detected logit output format. Channels=" << out_c
              << std::endl;
  } else {
    is_logits = false;
    std::cout << "[Pipeline] Detected direct label output format." << std::endl;
  }

  // --- Init Eval Thread ---
  EvalConfig eval_cfg;
  eval_cfg.enabled = !disable_eval;
  eval_cfg.print_every = eval_every;
  eval_cfg.out_w = out_w;
  eval_cfg.out_h = out_h;

  // Setup CSV run dir
  auto now = std::chrono::system_clock::now();
  uint64_t timestamp =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
          .count();
  std::string cmd = "mkdir -p runs/" + std::to_string(timestamp);
  system(cmd.c_str());
  eval_cfg.csv_path =
      "runs/" + std::to_string(timestamp) + "/pipeline_direct.csv";

  std::thread eval_th(evaluate_thread, eval_cfg);

  // --- Init Carla ---
  cc::Client client(host, port);
  client.SetTimeout(std::chrono::seconds(10));
  auto world = client.GetWorld();
  auto blueprint_library = world.GetBlueprintLibrary();

  // Spawn vehicle
  auto vehicle_bp = *blueprint_library->Find("vehicle.mercedes.coupe_2020");
  auto spawn_points = world.GetMap()->GetRecommendedSpawnPoints();
  if (spawn_points.empty())
    return EXIT_FAILURE;
  auto spawn_point = spawn_points[0];
  auto vehicle = world.SpawnActor(vehicle_bp, spawn_point);

  // Turn on autopilot
  boost::static_pointer_cast<cc::Vehicle>(vehicle)->SetAutopilot(true);

  // Common transform for both sensors
  cg::Transform camera_tf(cg::Location(0.0f, 0.0f, 2.0f),
                          cg::Rotation(0.0f, 0.0f, 0.0f));

  // RGB Camera
  auto rgb_bp = *blueprint_library->Find("sensor.camera.rgb");
  rgb_bp.SetAttribute("image_size_x", std::to_string(w));
  rgb_bp.SetAttribute("image_size_y", std::to_string(h));
  rgb_bp.SetAttribute("sensor_tick", std::to_string(1.0f / fps));
  auto rgb_sensor = world.SpawnActor(rgb_bp, camera_tf, vehicle.get());

  // GT Camera
  auto gt_bp = *blueprint_library->Find("sensor.camera.semantic_segmentation");
  gt_bp.SetAttribute("image_size_x", std::to_string(w));
  gt_bp.SetAttribute("image_size_y", std::to_string(h));
  gt_bp.SetAttribute("sensor_tick", std::to_string(1.0f / fps));
  auto gt_sensor = world.SpawnActor(gt_bp, camera_tf, vehicle.get());

  // --- Callbacks (Thread A logic via CARLA API) ---
  FrameSync sync(sync_window);

  boost::static_pointer_cast<cc::Sensor>(rgb_sensor)
      ->Listen([&sync](auto data) {
        if (g_stop_requested)
          return;
        auto image = boost::static_pointer_cast<cs::Image>(data);
        FrameIn f;
        f.frame_id = image->GetFrame();
        f.timestamp = image->GetTimestamp();
        f.w = image->GetWidth();
        f.h = image->GetHeight();
        f.carla_image = image;
        sync.PushRgb(std::move(f));
      });

  boost::static_pointer_cast<cc::Sensor>(gt_sensor)->Listen([&sync](auto data) {
    if (g_stop_requested)
      return;
    auto image = boost::static_pointer_cast<cs::Image>(data);
    GtFrame g;
    g.frame_id = image->GetFrame();
    g.timestamp = image->GetTimestamp();
    g.w = image->GetWidth();
    g.h = image->GetHeight();
    g.carla_image = image;
    sync.PushGt(std::move(g));
  });

  // --- Synced mode (strongly recommended) ---
  auto settings = world.GetSettings();
  bool original_sync = settings.synchronous_mode;
  settings.synchronous_mode = true;
  settings.fixed_delta_seconds = 1.0f / fps;
  world.ApplySettings(settings, std::chrono::seconds(2));

  // Buffers allocated once
  std::vector<float> input_tensor(out_w * out_h * 3, 0.0f);
  std::vector<uint8_t> raw_engine_output(runner.GetOutputBytes(), 0);

  // --- Main Loop (Thread B logic) ---
  int frames_processed = 0;
  int dropped = 0;

  using namespace std::chrono;

  std::cout << "[Pipeline] Running loop expecting " << max_frames
            << " frames..." << std::endl;

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
  if (resize_type == "bilinear") {
    pre_cfg.resize = PreprocessConfig::BILINEAR;
  } else {
    pre_cfg.resize = PreprocessConfig::NEAREST;
  }

  while (!g_stop_requested && frames_processed < max_frames) {
    world.Tick(std::chrono::seconds(2));

    MatchedPair pair;
    MatchedPair latest_pair;
    bool found = false;
    int popped = 0;

    // Drain matches to get the latest + implement drop-old
    for (int retry = 0; retry < 50; retry++) {
      while (sync.TryPopMatched(pair)) {
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
        dropped += (popped - 1);
      }
      pair = std::move(latest_pair);

      auto t_start = high_resolution_clock::now();

      // 1) PREPROCESS
      PreprocessBGRAtoNCHW_F32(
          reinterpret_cast<const uint8_t *>(pair.rgb.carla_image->data()),
          pair.rgb.w, pair.rgb.h, pre_cfg, input_tensor.data());
      auto t_pre = high_resolution_clock::now();

      // 2) INFER
      runner.Infer(input_tensor.data(), input_tensor.size() * sizeof(float),
                   raw_engine_output.data(), raw_engine_output.size());
      auto t_inf = high_resolution_clock::now();

      // 3) POSTPROCESS
      EvalPayload ep;
      ep.frame_id = pair.rgb.frame_id;
      // NOTE: raw_semantic_bytes needs moving to EvalPayload so GT is saved for
      // Async checking
      ep.gt_frame = std::move(pair.gt);

      if (is_logits) {
        // out_c, out_h, out_w is the shape of logits
        const float *logits_ptr =
            reinterpret_cast<const float *>(raw_engine_output.data());
        postprocess::ArgmaxLogitsToLabels(logits_ptr, out_c, out_h, out_w,
                                          ep.pred_labels);
      } else {
        const int32_t *labels_ptr =
            reinterpret_cast<const int32_t *>(raw_engine_output.data());
        postprocess::DirectLabelsToUint8(labels_ptr, out_h, out_w,
                                         ep.pred_labels);
      }
      auto t_post = high_resolution_clock::now();

      // Store timings
      ep.preproc_ms = duration<double, std::milli>(t_pre - t_start).count();
      ep.infer_ms = duration<double, std::milli>(t_inf - t_pre).count();
      ep.post_ms = duration<double, std::milli>(t_post - t_inf).count();
      ep.total_ms = duration<double, std::milli>(t_post - t_start).count();
      ep.dropped_before_process = dropped;
      dropped = 0; // reset

      // Send to eval payload
      g_eval_queue.push(std::move(ep));

      frames_processed++;

      if (frames_processed % print_every == 0) {
        std::cout << "[Pipeline] Frame " << frames_processed << "/"
                  << max_frames << " | Drops: " << ep.dropped_before_process
                  << " | Pre: " << ep.preproc_ms << "ms"
                  << " | Inf: " << ep.infer_ms << "ms"
                  << " | Post: " << ep.post_ms << "ms"
                  << " | Total: " << ep.total_ms << "ms" << std::endl;
      }

    } else {
      // No match found in time window
    }
  }

  std::cout
      << "\n[Pipeline] Exiting. Waiting for eval thread to secure output..."
      << std::endl;
  g_stop_requested = true;
  eval_th.join();
  runner.Shutdown();

  // Destroy sensors and restore settings
  if (rgb_sensor)
    rgb_sensor->Destroy();
  if (gt_sensor)
    gt_sensor->Destroy();
  if (vehicle)
    vehicle->Destroy();

  settings.synchronous_mode = original_sync;
  world.ApplySettings(settings, std::chrono::seconds(2));

  return EXIT_SUCCESS;
}
