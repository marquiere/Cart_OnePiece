#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
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
#include <thread>

#include "frame_sync.hpp"
#include "postprocess.hpp"
#include "preprocess.hpp"
#include "semantic_decode.hpp"
#include "trt_runner.hpp"
#include "viewer_sdl2.hpp"
#include "vis.hpp"

using namespace std::chrono;
namespace fs = std::filesystem;

void SaveBin(const std::string &filepath, const uint8_t *data, size_t size) {
  std::ofstream f(filepath, std::ios::binary);
  if (f)
    f.write(reinterpret_cast<const char *>(data), size);
}

int main(int argc, char **argv) {
  std::string host = "127.0.0.1";
  uint16_t port = 2000;
  std::string map_name = "";
  int w = 800;
  int h = 600;
  float fps = 10.0f;
  int target_frames = 30;
  std::string engine_path = "";
  int out_w = 512;
  int out_h = 256;
  bool assume_bgra = true;
  float alpha = 0.5f;
  bool no_pred = false;
  bool save_raw_bin = true;
  bool save_png = true;
  int print_every = 5;
  bool display = false;

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
      fps = std::stof(argv[++i]);
    else if (arg == "--frames" && i + 1 < argc)
      target_frames = std::stoi(argv[++i]);
    else if (arg == "--engine" && i + 1 < argc)
      engine_path = argv[++i];
    else if (arg == "--out_w" && i + 1 < argc)
      out_w = std::stoi(argv[++i]);
    else if (arg == "--out_h" && i + 1 < argc)
      out_h = std::stoi(argv[++i]);
    else if (arg == "--assume_bgra" && i + 1 < argc)
      assume_bgra = std::stoi(argv[++i]);
    else if (arg == "--alpha" && i + 1 < argc)
      alpha = std::stof(argv[++i]);
    else if (arg == "--no_pred" && i + 1 < argc)
      no_pred = std::stoi(argv[++i]);
    else if (arg == "--save_raw_bin" && i + 1 < argc)
      save_raw_bin = std::stoi(argv[++i]);
    else if (arg == "--save_png" && i + 1 < argc)
      save_png = std::stoi(argv[++i]);
    else if (arg == "--print_every" && i + 1 < argc)
      print_every = std::stoi(argv[++i]);
    else if (arg == "--display" && i + 1 < argc)
      display = (std::stoi(argv[++i]) != 0);
  }

  if (!no_pred && engine_path.empty()) {
    std::cerr << "Error: --engine is required when prediction is enabled. Use "
                 "--no_pred 1 to disable.\n";
    return 1;
  }

  std::cout << "[Sanity Dataset] Starting... mode="
            << (no_pred ? "RGB+GT Only" : "Full Inference") << "\n";

  // Setup Engine if needed
  std::unique_ptr<TrtRunner> runner = nullptr;
  int out_c = 0;
  bool is_logits = false;
  if (!no_pred) {
    runner = std::make_unique<TrtRunner>();
    if (!runner->LoadEngine(engine_path) || !runner->Init()) {
      std::cerr << "Failed to init TRT\n";
      return 1;
    }

    if (!runner->SetInputShapeIfDynamic(out_w, out_h)) {
      std::cerr << "Failed to bind dynamic shape.\n";
      return 1;
    }

    size_t out_bytes = runner->GetOutputBytes();
    if (out_bytes == (size_t)(out_w * out_h * sizeof(int32_t))) {
      out_c = 1;
      is_logits = false;
    } else {
      out_c = out_bytes / (out_w * out_h * sizeof(float));
      if (out_c > 1) {
        is_logits = true;
      } else {
        std::cerr << "Unexpected engine output shape/type.\n";
        return 1;
      }
    }
  }

  // Setup runs foldermn
  auto now = system_clock::now();
  auto time_t = system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
  std::string timestamp = ss.str();

  std::string run_dir = "runs/" + timestamp + "/sanity_dataset";
  fs::create_directories(run_dir + "/rgb");
  fs::create_directories(run_dir + "/gt_raw");
  fs::create_directories(run_dir + "/gt_color");
  if (!no_pred) {
    fs::create_directories(run_dir + "/pred_raw");
    fs::create_directories(run_dir + "/pred_color");
    fs::create_directories(run_dir + "/overlay");
  }

  std::unique_ptr<SegmentationViewerSDL2> viewer = nullptr;
  if (display) {
    viewer = std::make_unique<SegmentationViewerSDL2>();
    if (!viewer->init(w, h, 2.0f)) {
      std::cerr << "[Sanity Dataset] Viewer init failed.\n";
      viewer.reset();
    }
  }

  // Connect
  auto client = carla::client::Client(host, port);
  client.SetTimeout(std::chrono::seconds(10));
  auto world = client.GetWorld();

  if (!map_name.empty()) {
    world = client.LoadWorld(map_name);
  }

  auto settings = world.GetSettings();
  bool original_sync = settings.synchronous_mode;
  settings.synchronous_mode = true;
  settings.fixed_delta_seconds = 1.0f / fps;
  world.ApplySettings(settings, std::chrono::seconds(2));

  auto tm = client.GetInstanceTM();
  tm.SetSynchronousMode(true);
  uint16_t tm_port = 8000;

  auto bp_lib = world.GetBlueprintLibrary();
  auto vehicle_bp = *bp_lib->Find("vehicle.mercedes.coupe_2020");
  auto spawn_points = world.GetMap()->GetRecommendedSpawnPoints();
  if (spawn_points.empty()) {
    std::cerr << "No spawn points found\n";
    return 1;
  }

  boost::shared_ptr<carla::client::Actor> vehicle = nullptr;
  for (auto sp : spawn_points) {
    sp.location.z += 1.0f;
    vehicle = world.TrySpawnActor(vehicle_bp, sp);
    if (vehicle)
      break;
  }

  if (!vehicle) {
    std::cerr << "Failed to spawn vehicle anywhere!\n";
    return 1;
  }
  vehicle->SetSimulatePhysics(true);
  auto p_vehicle = boost::static_pointer_cast<carla::client::Vehicle>(vehicle);
  p_vehicle->SetAutopilot(true, tm_port);

  auto rgb_bp = *bp_lib->Find("sensor.camera.rgb");
  rgb_bp.SetAttribute("image_size_x", std::to_string(w));
  rgb_bp.SetAttribute("image_size_y", std::to_string(h));
  rgb_bp.SetAttribute("sensor_tick", std::to_string(1.0f / fps));

  auto gt_bp = *bp_lib->Find("sensor.camera.semantic_segmentation");
  gt_bp.SetAttribute("image_size_x", std::to_string(w));
  gt_bp.SetAttribute("image_size_y", std::to_string(h));
  gt_bp.SetAttribute("sensor_tick", std::to_string(1.0f / fps));

  carla::geom::Transform cam_tf(carla::geom::Location(2.0f, 0.0f, 1.4f),
                                carla::geom::Rotation(0.0f, 0.0f, 0.0f));

  auto rgb_sensor = world.SpawnActor(rgb_bp, cam_tf, vehicle.get());
  auto gt_sensor = world.SpawnActor(gt_bp, cam_tf, vehicle.get());

  FrameSync sync(10);

  auto rgb_cam = boost::static_pointer_cast<carla::client::Sensor>(rgb_sensor);
  auto gt_cam = boost::static_pointer_cast<carla::client::Sensor>(gt_sensor);

  rgb_cam->Listen([&](auto data) {
    FrameIn f;
    f.frame_id = data->GetFrame();
    f.timestamp = data->GetTimestamp();
    f.w = w;
    f.h = h;
    f.carla_image =
        boost::static_pointer_cast<carla::sensor::data::Image>(data);
    sync.PushRgb(std::move(f));
  });

  gt_cam->Listen([&](auto data) {
    GtFrame f;
    f.frame_id = data->GetFrame();
    f.timestamp = data->GetTimestamp();
    f.w = w;
    f.h = h;
    f.carla_image =
        boost::static_pointer_cast<carla::sensor::data::Image>(data);
    sync.PushGt(std::move(f));
  });

  std::vector<uint64_t> saved_frames;

  // Arrays for image processing
  std::vector<uint8_t> rgb_bytes(w * h * 3);
  std::vector<uint8_t> gt_labels(w * h);
  std::vector<uint8_t> gt_color(w * h * 3);

  std::vector<float> trt_in;
  std::vector<uint8_t> pred_labels;
  std::vector<uint8_t> pred_upsampled;
  std::vector<uint8_t> pred_color;
  std::vector<uint8_t> overlay;

  if (!no_pred) {
    trt_in.resize(3 * out_w * out_h);
    pred_labels.resize(out_w * out_h);
    pred_upsampled.resize(w * h);
    pred_color.resize(w * h * 3);
    overlay.resize(w * h * 3);
  }

  int total_processed = 0;
  while (total_processed < target_frames) {
    if (viewer) {
      if (viewer->poll_events()) {
        break;
      }
    }

    world.Tick(std::chrono::seconds(2));

    MatchedPair pair;
    bool got_pair = false;
    // Try for some timeout to avoid locking up totally if sensors drop a frame
    for (int t = 0; t < 100; ++t) {
      if (sync.TryPopMatched(pair)) {
        got_pair = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (!got_pair) {
      continue;
    }

    uint64_t fid = pair.frame_id;

    // 1. Process RGB
    // Convert CARLA BGRA to RGB
    vis::BgraToRgb(
        reinterpret_cast<const uint8_t *>(pair.rgb.carla_image->data()), w, h,
        rgb_bytes.data());
    if (save_png) {
      vis::SavePng(run_dir + "/rgb/rgb_" + std::to_string(fid) + ".png", w, h,
                   3, rgb_bytes.data());
    }

    // 2. Process GT
    gt_labels = DecodeSemanticLabels(
        reinterpret_cast<const uint8_t *>(pair.gt.carla_image->data()), w, h,
        assume_bgra);
    if (save_raw_bin) {
      SaveBin(run_dir + "/gt_raw/gt_" + std::to_string(fid) + ".bin",
              gt_labels.data(), w * h);
    }
    if (save_png) {
      vis::ColorizeCityscapes(gt_labels.data(), w, h, gt_color.data());
      vis::SavePng(run_dir + "/gt_color/gt_" + std::to_string(fid) + ".png", w,
                   h, 3, gt_color.data());
    }

    // 3. Inference
    if (!no_pred) {
      PreprocessConfig pre_cfg;
      pre_cfg.out_w = out_w;
      pre_cfg.out_h = out_h;
      pre_cfg.resize = PreprocessConfig::ResizeMode::BILINEAR;
      pre_cfg.assume_bgra = assume_bgra;
      pre_cfg.to_rgb = true;
      pre_cfg.mean[0] = 0.485f;
      pre_cfg.mean[1] = 0.456f;
      pre_cfg.mean[2] = 0.406f;
      pre_cfg.std[0] = 0.229f;
      pre_cfg.std[1] = 0.224f;
      pre_cfg.std[2] = 0.225f;

      PreprocessBGRAtoNCHW_F32(
          reinterpret_cast<const uint8_t *>(pair.rgb.carla_image->data()), w, h,
          pre_cfg, trt_in.data());

      if (is_logits) {
        std::vector<float> trt_out(runner->GetOutputBytes() / sizeof(float));
        runner->Infer(trt_in.data(), runner->GetInputBytes(), trt_out.data(),
                      runner->GetOutputBytes());
        postprocess::ArgmaxLogitsToLabels(trt_out.data(), out_c, out_h, out_w,
                                          pred_labels);
      } else {
        std::vector<int32_t> trt_out(runner->GetOutputBytes() /
                                     sizeof(int32_t));
        runner->Infer(trt_in.data(), runner->GetInputBytes(), trt_out.data(),
                      runner->GetOutputBytes());
        postprocess::DirectLabelsToUint8(trt_out.data(), out_h, out_w,
                                         pred_labels);
      }

      if (save_raw_bin) {
        SaveBin(run_dir + "/pred_raw/pred_" + std::to_string(fid) + ".bin",
                pred_labels.data(), out_w * out_h);
      }

      // Upsample prediction to camera resolution for overlay mapping
      vis::UpsampleNearest(pred_labels.data(), out_w, out_h,
                           pred_upsampled.data(), w, h);

      if (save_png) {
        vis::ColorizeCityscapes(pred_upsampled.data(), w, h, pred_color.data());
        vis::SavePng(run_dir + "/pred_color/pred_" + std::to_string(fid) +
                         ".png",
                     w, h, 3, pred_color.data());

        vis::BlendOverlay(rgb_bytes.data(), pred_color.data(), w, h, alpha,
                          overlay.data());
        vis::SavePng(run_dir + "/overlay/overlay_" + std::to_string(fid) +
                         ".png",
                     w, h, 3, overlay.data());
      }
    }

    saved_frames.push_back(fid);
    total_processed++;

    if (viewer) {
      if (!no_pred) {
        viewer->submit_frame_rgb888(overlay.data(), w, h);
      } else {
        viewer->submit_frame_rgb888(rgb_bytes.data(), w, h);
      }
      viewer->render_latest();
    }

    if (total_processed % print_every == 0) {
      std::cout << "[Sanity Dataset] Saved frame " << total_processed << " / "
                << target_frames << " (ID: " << fid << ")\n";
    }
  }

  std::cout << "[Sanity Dataset] Finished writing " << target_frames
            << " frames.\n";

  // Write meta.json
  std::ofstream meta(run_dir + "/meta.json");
  meta << "{\n";
  meta << "  \"date_time\": \"" << timestamp << "\",\n";
  meta << "  \"map\": \"" << map_name << "\",\n";
  meta << "  \"w\": " << w << ",\n";
  meta << "  \"h\": " << h << ",\n";
  meta << "  \"fps\": " << fps << ",\n";
  meta << "  \"assume_bgra\": " << assume_bgra << ",\n";
  meta << "  \"no_pred\": " << no_pred << ",\n";
  if (!no_pred) {
    meta << "  \"engine_path\": \"" << engine_path << "\",\n";
    meta << "  \"out_w\": " << out_w << ",\n";
    meta << "  \"out_h\": " << out_h << ",\n";
    meta << "  \"alpha\": " << alpha << ",\n";
  }
  meta << "  \"frames\": [\n    ";
  for (size_t i = 0; i < saved_frames.size(); ++i) {
    meta << saved_frames[i] << (i < saved_frames.size() - 1 ? ", " : "");
    if ((i + 1) % 10 == 0 && i < saved_frames.size() - 1)
      meta << "\n    ";
  }
  meta << "\n  ]\n";
  meta << "}\n";

  if (gt_sensor)
    gt_sensor->Destroy();
  if (rgb_sensor)
    rgb_sensor->Destroy();
  if (vehicle)
    vehicle->Destroy();

  tm.SetSynchronousMode(false);
  settings.synchronous_mode = original_sync;
  world.ApplySettings(settings, std::chrono::seconds(2));

  return 0;
}
