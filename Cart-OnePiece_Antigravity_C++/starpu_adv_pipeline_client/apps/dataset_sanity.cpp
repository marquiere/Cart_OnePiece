#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
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
  std::set<int> active_cameras = {0};

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
    else if (arg == "--cameras" && i + 1 < argc) {
      active_cameras.clear();
      std::stringstream ss(argv[++i]);
      std::string item;
      while (std::getline(ss, item, ',')) {
        active_cameras.insert(std::stoi(item));
      }
    }
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

  // Setup runs folder respecting RUN_DIR
  const char *env_dir = std::getenv("RUN_DIR");
  std::string run_dir;
  if (env_dir) {
    run_dir = std::string(env_dir) + "/sanity_dataset";
  } else {
    auto now = system_clock::now();
    auto time_t = system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    run_dir = "runs/" + ss.str() + "/sanity_dataset";
  }
  if (active_cameras.count(0)) {
    fs::create_directories(run_dir + "/rgb");
    fs::create_directories(run_dir + "/gt_raw");
    fs::create_directories(run_dir + "/gt_color");
  }
  if (active_cameras.count(1)) {
    fs::create_directories(run_dir + "/rear_rgb");
    fs::create_directories(run_dir + "/rear_gt_raw");
    fs::create_directories(run_dir + "/rear_gt_color");
  }
  if (active_cameras.count(2)) {
    fs::create_directories(run_dir + "/left_rgb");
    fs::create_directories(run_dir + "/left_gt_raw");
    fs::create_directories(run_dir + "/left_gt_color");
  }
  if (active_cameras.count(3)) {
    fs::create_directories(run_dir + "/right_rgb");
    fs::create_directories(run_dir + "/right_gt_raw");
    fs::create_directories(run_dir + "/right_gt_color");
  }
  if (active_cameras.count(4)) {
    fs::create_directories(run_dir + "/front_left_rgb");
    fs::create_directories(run_dir + "/front_left_gt_raw");
    fs::create_directories(run_dir + "/front_left_gt_color");
  }
  if (active_cameras.count(5)) {
    fs::create_directories(run_dir + "/front_right_rgb");
    fs::create_directories(run_dir + "/front_right_gt_raw");
    fs::create_directories(run_dir + "/front_right_gt_color");
  }
  if (active_cameras.count(6)) {
    fs::create_directories(run_dir + "/rear_left_rgb");
    fs::create_directories(run_dir + "/rear_left_gt_raw");
    fs::create_directories(run_dir + "/rear_left_gt_color");
  }
  if (active_cameras.count(7)) {
    fs::create_directories(run_dir + "/rear_right_rgb");
    fs::create_directories(run_dir + "/rear_right_gt_raw");
    fs::create_directories(run_dir + "/rear_right_gt_color");
  }

  if (!no_pred) {
    if (active_cameras.count(0)) {
      fs::create_directories(run_dir + "/pred_raw");
      fs::create_directories(run_dir + "/pred_color");
      fs::create_directories(run_dir + "/overlay");
    }
    if (active_cameras.count(1)) {
      fs::create_directories(run_dir + "/rear_pred_raw");
      fs::create_directories(run_dir + "/rear_pred_color");
      fs::create_directories(run_dir + "/rear_overlay");
    }
    if (active_cameras.count(2)) {
      fs::create_directories(run_dir + "/left_pred_raw");
      fs::create_directories(run_dir + "/left_pred_color");
      fs::create_directories(run_dir + "/left_overlay");
    }
    if (active_cameras.count(3)) {
      fs::create_directories(run_dir + "/right_pred_raw");
      fs::create_directories(run_dir + "/right_pred_color");
      fs::create_directories(run_dir + "/right_overlay");
    }
    if (active_cameras.count(4)) {
      fs::create_directories(run_dir + "/front_left_pred_raw");
      fs::create_directories(run_dir + "/front_left_pred_color");
      fs::create_directories(run_dir + "/front_left_overlay");
    }
    if (active_cameras.count(5)) {
      fs::create_directories(run_dir + "/front_right_pred_raw");
      fs::create_directories(run_dir + "/front_right_pred_color");
      fs::create_directories(run_dir + "/front_right_overlay");
    }
    if (active_cameras.count(6)) {
      fs::create_directories(run_dir + "/rear_left_pred_raw");
      fs::create_directories(run_dir + "/rear_left_pred_color");
      fs::create_directories(run_dir + "/rear_left_overlay");
    }
    if (active_cameras.count(7)) {
      fs::create_directories(run_dir + "/rear_right_pred_raw");
      fs::create_directories(run_dir + "/rear_right_pred_color");
      fs::create_directories(run_dir + "/rear_right_overlay");
    }
  }

  std::unique_ptr<SegmentationViewerSDL2> viewer = nullptr;
  if (display) {
    viewer = std::make_unique<SegmentationViewerSDL2>();
    int num_cams = active_cameras.size();
    if (num_cams == 0)
      num_cams = 1;
    int cols = 1, rows = 1;
    if (num_cams == 2) {
      cols = 2;
      rows = 1;
    } else if (num_cams == 3 || num_cams == 4) {
      cols = 2;
      rows = 2;
    } else if (num_cams == 5 || num_cams == 6) {
      cols = 3;
      rows = 2;
    } else if (num_cams >= 7) {
      cols = 4;
      rows = 2;
    }

    if (!viewer->init(w * cols, h * rows, 0.5f)) {
      std::cerr << "[Sanity Dataset] Viewer init failed.\n";
      viewer.reset();
    }
  }

  // Connect
  auto client = carla::client::Client(host, port);
  client.SetTimeout(std::chrono::seconds(60));

  std::optional<carla::client::World> world_opt;
  while (!world_opt) {
    try {
      world_opt = client.GetWorld();
    } catch (const std::exception &e) {
      std::cerr << "Timeout waiting for CARLA world. Retrying...\n";
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  }
  auto world = *world_opt;

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

  auto rgb_sensor = (active_cameras.count(0))
                        ? world.SpawnActor(rgb_bp, cam_tf, vehicle.get())
                        : nullptr;
  auto gt_sensor = (active_cameras.count(0))
                       ? world.SpawnActor(gt_bp, cam_tf, vehicle.get())
                       : nullptr;

  carla::geom::Transform rear_tf(carla::geom::Location(-2.0f, 0.0f, 1.4f),
                                 carla::geom::Rotation(0.0f, 180.0f, 0.0f));
  auto rear_rgb_sensor = (active_cameras.count(1))
                             ? world.SpawnActor(rgb_bp, rear_tf, vehicle.get())
                             : nullptr;
  auto rear_gt_sensor = (active_cameras.count(1))
                            ? world.SpawnActor(gt_bp, rear_tf, vehicle.get())
                            : nullptr;

  carla::geom::Transform left_tf(carla::geom::Location(0.0f, -1.0f, 1.4f),
                                 carla::geom::Rotation(0.0f, -90.0f, 0.0f));
  auto left_rgb_sensor = (active_cameras.count(2))
                             ? world.SpawnActor(rgb_bp, left_tf, vehicle.get())
                             : nullptr;
  auto left_gt_sensor = (active_cameras.count(2))
                            ? world.SpawnActor(gt_bp, left_tf, vehicle.get())
                            : nullptr;

  carla::geom::Transform right_tf(carla::geom::Location(0.0f, 1.0f, 1.4f),
                                  carla::geom::Rotation(0.0f, 90.0f, 0.0f));
  auto right_rgb_sensor =
      (active_cameras.count(3))
          ? world.SpawnActor(rgb_bp, right_tf, vehicle.get())
          : nullptr;
  auto right_gt_sensor = (active_cameras.count(3))
                             ? world.SpawnActor(gt_bp, right_tf, vehicle.get())
                             : nullptr;

  // 8-Camera Additions
  carla::geom::Transform fl_tf(carla::geom::Location(1.4f, -1.0f, 1.4f),
                               carla::geom::Rotation(0.0f, -45.0f, 0.0f));
  auto fl_rgb_sensor = (active_cameras.count(4))
                           ? world.SpawnActor(rgb_bp, fl_tf, vehicle.get())
                           : nullptr;
  auto fl_gt_sensor = (active_cameras.count(4))
                          ? world.SpawnActor(gt_bp, fl_tf, vehicle.get())
                          : nullptr;

  carla::geom::Transform fr_tf(carla::geom::Location(1.4f, 1.0f, 1.4f),
                               carla::geom::Rotation(0.0f, 45.0f, 0.0f));
  auto fr_rgb_sensor = (active_cameras.count(5))
                           ? world.SpawnActor(rgb_bp, fr_tf, vehicle.get())
                           : nullptr;
  auto fr_gt_sensor = (active_cameras.count(5))
                          ? world.SpawnActor(gt_bp, fr_tf, vehicle.get())
                          : nullptr;

  carla::geom::Transform rl_tf(carla::geom::Location(-1.4f, -1.0f, 1.4f),
                               carla::geom::Rotation(0.0f, -135.0f, 0.0f));
  auto rl_rgb_sensor = (active_cameras.count(6))
                           ? world.SpawnActor(rgb_bp, rl_tf, vehicle.get())
                           : nullptr;
  auto rl_gt_sensor = (active_cameras.count(6))
                          ? world.SpawnActor(gt_bp, rl_tf, vehicle.get())
                          : nullptr;

  carla::geom::Transform rr_tf(carla::geom::Location(-1.4f, 1.0f, 1.4f),
                               carla::geom::Rotation(0.0f, 135.0f, 0.0f));
  auto rr_rgb_sensor = (active_cameras.count(7))
                           ? world.SpawnActor(rgb_bp, rr_tf, vehicle.get())
                           : nullptr;
  auto rr_gt_sensor = (active_cameras.count(7))
                          ? world.SpawnActor(gt_bp, rr_tf, vehicle.get())
                          : nullptr;

  FrameSync sync(10);
  FrameSync rear_sync(10);
  FrameSync left_sync(10);
  FrameSync right_sync(10);
  FrameSync fl_sync(10);
  FrameSync fr_sync(10);
  FrameSync rl_sync(10);
  FrameSync rr_sync(10);

  auto rgb_cam = boost::static_pointer_cast<carla::client::Sensor>(rgb_sensor);
  auto gt_cam = boost::static_pointer_cast<carla::client::Sensor>(gt_sensor);
  auto rear_rgb_cam =
      boost::static_pointer_cast<carla::client::Sensor>(rear_rgb_sensor);
  auto rear_gt_cam =
      boost::static_pointer_cast<carla::client::Sensor>(rear_gt_sensor);
  auto left_rgb_cam =
      boost::static_pointer_cast<carla::client::Sensor>(left_rgb_sensor);
  auto left_gt_cam =
      boost::static_pointer_cast<carla::client::Sensor>(left_gt_sensor);
  auto right_rgb_cam =
      boost::static_pointer_cast<carla::client::Sensor>(right_rgb_sensor);
  auto right_gt_cam =
      boost::static_pointer_cast<carla::client::Sensor>(right_gt_sensor);
  auto fl_rgb_cam =
      boost::static_pointer_cast<carla::client::Sensor>(fl_rgb_sensor);
  auto fl_gt_cam =
      boost::static_pointer_cast<carla::client::Sensor>(fl_gt_sensor);
  auto fr_rgb_cam =
      boost::static_pointer_cast<carla::client::Sensor>(fr_rgb_sensor);
  auto fr_gt_cam =
      boost::static_pointer_cast<carla::client::Sensor>(fr_gt_sensor);
  auto rl_rgb_cam =
      boost::static_pointer_cast<carla::client::Sensor>(rl_rgb_sensor);
  auto rl_gt_cam =
      boost::static_pointer_cast<carla::client::Sensor>(rl_gt_sensor);
  auto rr_rgb_cam =
      boost::static_pointer_cast<carla::client::Sensor>(rr_rgb_sensor);
  auto rr_gt_cam =
      boost::static_pointer_cast<carla::client::Sensor>(rr_gt_sensor);

  if (rgb_cam)
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

  if (gt_cam)
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

  if (rear_rgb_cam)
    rear_rgb_cam->Listen([&](auto data) {
      FrameIn f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      rear_sync.PushRgb(std::move(f));
    });

  if (rear_gt_cam)
    rear_gt_cam->Listen([&](auto data) {
      GtFrame f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      rear_sync.PushGt(std::move(f));
    });

  if (left_rgb_cam)
    left_rgb_cam->Listen([&](auto data) {
      FrameIn f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      left_sync.PushRgb(std::move(f));
    });

  if (left_gt_cam)
    left_gt_cam->Listen([&](auto data) {
      GtFrame f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      left_sync.PushGt(std::move(f));
    });

  if (right_rgb_cam)
    right_rgb_cam->Listen([&](auto data) {
      FrameIn f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      right_sync.PushRgb(std::move(f));
    });

  if (right_gt_cam)
    right_gt_cam->Listen([&](auto data) {
      GtFrame f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      right_sync.PushGt(std::move(f));
    });

  // 8-Camera Additions
  if (fl_rgb_cam)
    fl_rgb_cam->Listen([&](auto data) {
      FrameIn f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      fl_sync.PushRgb(std::move(f));
    });
  if (fl_gt_cam)
    fl_gt_cam->Listen([&](auto data) {
      GtFrame f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      fl_sync.PushGt(std::move(f));
    });

  if (fr_rgb_cam)
    fr_rgb_cam->Listen([&](auto data) {
      FrameIn f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      fr_sync.PushRgb(std::move(f));
    });
  if (fr_gt_cam)
    fr_gt_cam->Listen([&](auto data) {
      GtFrame f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      fr_sync.PushGt(std::move(f));
    });

  if (rl_rgb_cam)
    rl_rgb_cam->Listen([&](auto data) {
      FrameIn f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      rl_sync.PushRgb(std::move(f));
    });
  if (rl_gt_cam)
    rl_gt_cam->Listen([&](auto data) {
      GtFrame f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      rl_sync.PushGt(std::move(f));
    });

  if (rr_rgb_cam)
    rr_rgb_cam->Listen([&](auto data) {
      FrameIn f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      rr_sync.PushRgb(std::move(f));
    });
  if (rr_gt_cam)
    rr_gt_cam->Listen([&](auto data) {
      GtFrame f;
      f.frame_id = data->GetFrame();
      f.timestamp = data->GetTimestamp();
      f.w = w;
      f.h = h;
      f.carla_image =
          boost::static_pointer_cast<carla::sensor::data::Image>(data);
      rr_sync.PushGt(std::move(f));
    });

  std::vector<uint64_t> saved_frames;
  std::vector<uint64_t> rear_saved_frames;
  std::vector<uint64_t> left_saved_frames;
  std::vector<uint64_t> right_saved_frames;
  std::vector<uint64_t> fl_saved_frames;
  std::vector<uint64_t> fr_saved_frames;
  std::vector<uint64_t> rl_saved_frames;
  std::vector<uint64_t> rr_saved_frames;

  // Arrays for image processing
  std::vector<uint8_t> rgb_bytes(w * h * 3);
  std::vector<uint8_t> gt_labels(w * h);
  std::vector<uint8_t> gt_color(w * h * 3);
  std::vector<uint8_t> rear_rgb_bytes(w * h * 3);
  std::vector<uint8_t> rear_gt_labels(w * h);
  std::vector<uint8_t> rear_gt_color(w * h * 3);
  std::vector<uint8_t> left_rgb_bytes(w * h * 3);
  std::vector<uint8_t> left_gt_labels(w * h);
  std::vector<uint8_t> left_gt_color(w * h * 3);
  std::vector<uint8_t> right_rgb_bytes(w * h * 3);
  std::vector<uint8_t> right_gt_labels(w * h);
  std::vector<uint8_t> right_gt_color(w * h * 3);
  std::vector<uint8_t> fl_rgb_bytes(w * h * 3);
  std::vector<uint8_t> fl_gt_labels(w * h);
  std::vector<uint8_t> fl_gt_color(w * h * 3);
  std::vector<uint8_t> fr_rgb_bytes(w * h * 3);
  std::vector<uint8_t> fr_gt_labels(w * h);
  std::vector<uint8_t> fr_gt_color(w * h * 3);
  std::vector<uint8_t> rl_rgb_bytes(w * h * 3);
  std::vector<uint8_t> rl_gt_labels(w * h);
  std::vector<uint8_t> rl_gt_color(w * h * 3);
  std::vector<uint8_t> rr_rgb_bytes(w * h * 3);
  std::vector<uint8_t> rr_gt_labels(w * h);
  std::vector<uint8_t> rr_gt_color(w * h * 3);

  std::vector<float> trt_in;
  std::vector<uint8_t> pred_labels;
  std::vector<uint8_t> pred_upsampled;
  std::vector<uint8_t> pred_color;
  std::vector<uint8_t> overlay;
  std::vector<float> rear_trt_in;
  std::vector<uint8_t> rear_pred_labels;
  std::vector<uint8_t> rear_pred_upsampled;
  std::vector<uint8_t> rear_pred_color;
  std::vector<uint8_t> rear_overlay;
  std::vector<float> left_trt_in;
  std::vector<uint8_t> left_pred_labels;
  std::vector<uint8_t> left_pred_upsampled;
  std::vector<uint8_t> left_pred_color;
  std::vector<uint8_t> left_overlay;
  std::vector<float> right_trt_in;
  std::vector<uint8_t> right_pred_labels;
  std::vector<uint8_t> right_pred_upsampled;
  std::vector<uint8_t> right_pred_color;
  std::vector<uint8_t> right_overlay;
  std::vector<float> fl_trt_in;
  std::vector<uint8_t> fl_pred_labels;
  std::vector<uint8_t> fl_pred_upsampled;
  std::vector<uint8_t> fl_pred_color;
  std::vector<uint8_t> fl_overlay;
  std::vector<float> fr_trt_in;
  std::vector<uint8_t> fr_pred_labels;
  std::vector<uint8_t> fr_pred_upsampled;
  std::vector<uint8_t> fr_pred_color;
  std::vector<uint8_t> fr_overlay;
  std::vector<float> rl_trt_in;
  std::vector<uint8_t> rl_pred_labels;
  std::vector<uint8_t> rl_pred_upsampled;
  std::vector<uint8_t> rl_pred_color;
  std::vector<uint8_t> rl_overlay;
  std::vector<float> rr_trt_in;
  std::vector<uint8_t> rr_pred_labels;
  std::vector<uint8_t> rr_pred_upsampled;
  std::vector<uint8_t> rr_pred_color;
  std::vector<uint8_t> rr_overlay;

  int num_cams = active_cameras.size();
  if (num_cams == 0)
    num_cams = 1;
  int mosaic_cols = 1, mosaic_rows = 1;
  if (num_cams == 2) {
    mosaic_cols = 2;
    mosaic_rows = 1;
  } else if (num_cams == 3 || num_cams == 4) {
    mosaic_cols = 2;
    mosaic_rows = 2;
  } else if (num_cams == 5 || num_cams == 6) {
    mosaic_cols = 3;
    mosaic_rows = 2;
  } else if (num_cams >= 7) {
    mosaic_cols = 4;
    mosaic_rows = 2;
  }

  std::vector<uint8_t> mosaic(w * mosaic_cols * h * mosaic_rows * 3, 0);

  if (!no_pred) {
    trt_in.resize(3 * out_w * out_h);
    pred_labels.resize(out_w * out_h);
    pred_upsampled.resize(w * h);
    pred_color.resize(w * h * 3);
    overlay.resize(w * h * 3);
    rear_trt_in.resize(3 * out_w * out_h);
    rear_pred_labels.resize(out_w * out_h);
    rear_pred_upsampled.resize(w * h);
    rear_pred_color.resize(w * h * 3);
    rear_overlay.resize(w * h * 3);
    left_trt_in.resize(3 * out_w * out_h);
    left_pred_labels.resize(out_w * out_h);
    left_pred_upsampled.resize(w * h);
    left_pred_color.resize(w * h * 3);
    left_overlay.resize(w * h * 3);
    right_trt_in.resize(3 * out_w * out_h);
    right_pred_labels.resize(out_w * out_h);
    right_pred_upsampled.resize(w * h);
    right_pred_color.resize(w * h * 3);
    right_overlay.resize(w * h * 3);
    fl_trt_in.resize(3 * out_w * out_h);
    fl_pred_labels.resize(out_w * out_h);
    fl_pred_upsampled.resize(w * h);
    fl_pred_color.resize(w * h * 3);
    fl_overlay.resize(w * h * 3);
    fr_trt_in.resize(3 * out_w * out_h);
    fr_pred_labels.resize(out_w * out_h);
    fr_pred_upsampled.resize(w * h);
    fr_pred_color.resize(w * h * 3);
    fr_overlay.resize(w * h * 3);
    rl_trt_in.resize(3 * out_w * out_h);
    rl_pred_labels.resize(out_w * out_h);
    rl_pred_upsampled.resize(w * h);
    rl_pred_color.resize(w * h * 3);
    rl_overlay.resize(w * h * 3);
    rr_trt_in.resize(3 * out_w * out_h);
    rr_pred_labels.resize(out_w * out_h);
    rr_pred_upsampled.resize(w * h);
    rr_pred_color.resize(w * h * 3);
    rr_overlay.resize(w * h * 3);
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
    MatchedPair rear_pair;
    bool got_rear_pair = false;
    MatchedPair left_pair;
    bool got_left_pair = false;
    MatchedPair right_pair;
    bool got_right_pair = false;
    MatchedPair fl_pair;
    bool got_fl_pair = false;
    MatchedPair fr_pair;
    bool got_fr_pair = false;
    MatchedPair rl_pair;
    bool got_rl_pair = false;
    MatchedPair rr_pair;
    bool got_rr_pair = false;

    // Try for some timeout to avoid locking up totally if sensors drop a frame
    for (int t = 0; t < 100; ++t) {
      if (!got_pair && sync.TryPopMatched(pair))
        got_pair = true;
      if (!got_rear_pair && rear_sync.TryPopMatched(rear_pair))
        got_rear_pair = true;
      if (!got_left_pair && left_sync.TryPopMatched(left_pair))
        got_left_pair = true;
      if (!got_right_pair && right_sync.TryPopMatched(right_pair))
        got_right_pair = true;
      if (!got_fl_pair && fl_sync.TryPopMatched(fl_pair))
        got_fl_pair = true;
      if (!got_fr_pair && fr_sync.TryPopMatched(fr_pair))
        got_fr_pair = true;
      if (!got_rl_pair && rl_sync.TryPopMatched(rl_pair))
        got_rl_pair = true;
      if (!got_rr_pair && rr_sync.TryPopMatched(rr_pair))
        got_rr_pair = true;

      if (got_pair && got_rear_pair && got_left_pair && got_right_pair &&
          got_fl_pair && got_fr_pair && got_rl_pair && got_rr_pair) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (!got_pair && !got_rear_pair && !got_left_pair && !got_right_pair &&
        !got_fl_pair && !got_fr_pair && !got_rl_pair && !got_rr_pair) {
      continue;
    }

    if (got_pair) {
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
        vis::SavePng(run_dir + "/gt_color/gt_" + std::to_string(fid) + ".png",
                     w, h, 3, gt_color.data());
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
            reinterpret_cast<const uint8_t *>(pair.rgb.carla_image->data()), w,
            h, pre_cfg, trt_in.data());

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
          vis::ColorizeCityscapes(pred_upsampled.data(), w, h,
                                  pred_color.data());
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
    }

    if (got_rear_pair) {
      uint64_t r_fid = rear_pair.frame_id;

      vis::BgraToRgb(
          reinterpret_cast<const uint8_t *>(rear_pair.rgb.carla_image->data()),
          w, h, rear_rgb_bytes.data());
      if (save_png) {
        vis::SavePng(run_dir + "/rear_rgb/rgb_" + std::to_string(r_fid) +
                         ".png",
                     w, h, 3, rear_rgb_bytes.data());
      }

      rear_gt_labels = DecodeSemanticLabels(
          reinterpret_cast<const uint8_t *>(rear_pair.gt.carla_image->data()),
          w, h, assume_bgra);
      if (save_raw_bin) {
        SaveBin(run_dir + "/rear_gt_raw/gt_" + std::to_string(r_fid) + ".bin",
                rear_gt_labels.data(), w * h);
      }
      if (save_png) {
        vis::ColorizeCityscapes(rear_gt_labels.data(), w, h,
                                rear_gt_color.data());
        vis::SavePng(run_dir + "/rear_gt_color/gt_" + std::to_string(r_fid) +
                         ".png",
                     w, h, 3, rear_gt_color.data());
      }

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

        PreprocessBGRAtoNCHW_F32(reinterpret_cast<const uint8_t *>(
                                     rear_pair.rgb.carla_image->data()),
                                 w, h, pre_cfg, rear_trt_in.data());

        if (is_logits) {
          std::vector<float> trt_out(runner->GetOutputBytes() / sizeof(float));
          runner->Infer(rear_trt_in.data(), runner->GetInputBytes(),
                        trt_out.data(), runner->GetOutputBytes());
          postprocess::ArgmaxLogitsToLabels(trt_out.data(), out_c, out_h, out_w,
                                            rear_pred_labels);
        } else {
          std::vector<int32_t> trt_out(runner->GetOutputBytes() /
                                       sizeof(int32_t));
          runner->Infer(rear_trt_in.data(), runner->GetInputBytes(),
                        trt_out.data(), runner->GetOutputBytes());
          postprocess::DirectLabelsToUint8(trt_out.data(), out_h, out_w,
                                           rear_pred_labels);
        }

        if (save_raw_bin) {
          SaveBin(run_dir + "/rear_pred_raw/pred_" + std::to_string(r_fid) +
                      ".bin",
                  rear_pred_labels.data(), out_w * out_h);
        }

        vis::UpsampleNearest(rear_pred_labels.data(), out_w, out_h,
                             rear_pred_upsampled.data(), w, h);

        if (save_png) {
          vis::ColorizeCityscapes(rear_pred_upsampled.data(), w, h,
                                  rear_pred_color.data());
          vis::SavePng(run_dir + "/rear_pred_color/pred_" +
                           std::to_string(r_fid) + ".png",
                       w, h, 3, rear_pred_color.data());

          vis::BlendOverlay(rear_rgb_bytes.data(), rear_pred_color.data(), w, h,
                            alpha, rear_overlay.data());
          vis::SavePng(run_dir + "/rear_overlay/overlay_" +
                           std::to_string(r_fid) + ".png",
                       w, h, 3, rear_overlay.data());
        }
      }

      rear_saved_frames.push_back(r_fid);
    }

    if (got_left_pair) {
      uint64_t l_fid = left_pair.frame_id;
      vis::BgraToRgb(
          reinterpret_cast<const uint8_t *>(left_pair.rgb.carla_image->data()),
          w, h, left_rgb_bytes.data());
      if (save_png)
        vis::SavePng(run_dir + "/left_rgb/rgb_" + std::to_string(l_fid) +
                         ".png",
                     w, h, 3, left_rgb_bytes.data());

      left_gt_labels = DecodeSemanticLabels(
          reinterpret_cast<const uint8_t *>(left_pair.gt.carla_image->data()),
          w, h, assume_bgra);
      if (save_raw_bin)
        SaveBin(run_dir + "/left_gt_raw/gt_" + std::to_string(l_fid) + ".bin",
                left_gt_labels.data(), w * h);
      if (save_png) {
        vis::ColorizeCityscapes(left_gt_labels.data(), w, h,
                                left_gt_color.data());
        vis::SavePng(run_dir + "/left_gt_color/gt_" + std::to_string(l_fid) +
                         ".png",
                     w, h, 3, left_gt_color.data());
      }
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
        PreprocessBGRAtoNCHW_F32(reinterpret_cast<const uint8_t *>(
                                     left_pair.rgb.carla_image->data()),
                                 w, h, pre_cfg, left_trt_in.data());
        if (is_logits) {
          std::vector<float> trt_out(runner->GetOutputBytes() / sizeof(float));
          runner->Infer(left_trt_in.data(), runner->GetInputBytes(),
                        trt_out.data(), runner->GetOutputBytes());
          postprocess::ArgmaxLogitsToLabels(trt_out.data(), out_c, out_h, out_w,
                                            left_pred_labels);
        } else {
          std::vector<int32_t> trt_out(runner->GetOutputBytes() /
                                       sizeof(int32_t));
          runner->Infer(left_trt_in.data(), runner->GetInputBytes(),
                        trt_out.data(), runner->GetOutputBytes());
          postprocess::DirectLabelsToUint8(trt_out.data(), out_h, out_w,
                                           left_pred_labels);
        }
        if (save_raw_bin)
          SaveBin(run_dir + "/left_pred_raw/pred_" + std::to_string(l_fid) +
                      ".bin",
                  left_pred_labels.data(), out_w * out_h);
        vis::UpsampleNearest(left_pred_labels.data(), out_w, out_h,
                             left_pred_upsampled.data(), w, h);
        if (save_png) {
          vis::ColorizeCityscapes(left_pred_upsampled.data(), w, h,
                                  left_pred_color.data());
          vis::SavePng(run_dir + "/left_pred_color/pred_" +
                           std::to_string(l_fid) + ".png",
                       w, h, 3, left_pred_color.data());
          vis::BlendOverlay(left_rgb_bytes.data(), left_pred_color.data(), w, h,
                            alpha, left_overlay.data());
          vis::SavePng(run_dir + "/left_overlay/overlay_" +
                           std::to_string(l_fid) + ".png",
                       w, h, 3, left_overlay.data());
        }
      }
      left_saved_frames.push_back(l_fid);
    }

    if (got_right_pair) {
      uint64_t ri_fid = right_pair.frame_id;
      vis::BgraToRgb(
          reinterpret_cast<const uint8_t *>(right_pair.rgb.carla_image->data()),
          w, h, right_rgb_bytes.data());
      if (save_png)
        vis::SavePng(run_dir + "/right_rgb/rgb_" + std::to_string(ri_fid) +
                         ".png",
                     w, h, 3, right_rgb_bytes.data());

      right_gt_labels = DecodeSemanticLabels(
          reinterpret_cast<const uint8_t *>(right_pair.gt.carla_image->data()),
          w, h, assume_bgra);
      if (save_raw_bin)
        SaveBin(run_dir + "/right_gt_raw/gt_" + std::to_string(ri_fid) + ".bin",
                right_gt_labels.data(), w * h);
      if (save_png) {
        vis::ColorizeCityscapes(right_gt_labels.data(), w, h,
                                right_gt_color.data());
        vis::SavePng(run_dir + "/right_gt_color/gt_" + std::to_string(ri_fid) +
                         ".png",
                     w, h, 3, right_gt_color.data());
      }
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
        PreprocessBGRAtoNCHW_F32(reinterpret_cast<const uint8_t *>(
                                     right_pair.rgb.carla_image->data()),
                                 w, h, pre_cfg, right_trt_in.data());
        if (is_logits) {
          std::vector<float> trt_out(runner->GetOutputBytes() / sizeof(float));
          runner->Infer(right_trt_in.data(), runner->GetInputBytes(),
                        trt_out.data(), runner->GetOutputBytes());
          postprocess::ArgmaxLogitsToLabels(trt_out.data(), out_c, out_h, out_w,
                                            right_pred_labels);
        } else {
          std::vector<int32_t> trt_out(runner->GetOutputBytes() /
                                       sizeof(int32_t));
          runner->Infer(right_trt_in.data(), runner->GetInputBytes(),
                        trt_out.data(), runner->GetOutputBytes());
          postprocess::DirectLabelsToUint8(trt_out.data(), out_h, out_w,
                                           right_pred_labels);
        }
        if (save_raw_bin)
          SaveBin(run_dir + "/right_pred_raw/pred_" + std::to_string(ri_fid) +
                      ".bin",
                  right_pred_labels.data(), out_w * out_h);
        vis::UpsampleNearest(right_pred_labels.data(), out_w, out_h,
                             right_pred_upsampled.data(), w, h);
        if (save_png) {
          vis::ColorizeCityscapes(right_pred_upsampled.data(), w, h,
                                  right_pred_color.data());
          vis::SavePng(run_dir + "/right_pred_color/pred_" +
                           std::to_string(ri_fid) + ".png",
                       w, h, 3, right_pred_color.data());
          vis::BlendOverlay(right_rgb_bytes.data(), right_pred_color.data(), w,
                            h, alpha, right_overlay.data());
          vis::SavePng(run_dir + "/right_overlay/overlay_" +
                           std::to_string(ri_fid) + ".png",
                       w, h, 3, right_overlay.data());
        }
      }
      right_saved_frames.push_back(ri_fid);
    }

    if (got_fl_pair) {
      uint64_t fid = fl_pair.frame_id;
      vis::BgraToRgb(
          reinterpret_cast<const uint8_t *>(fl_pair.rgb.carla_image->data()), w,
          h, fl_rgb_bytes.data());
      if (save_png)
        vis::SavePng(run_dir + "/front_left_rgb/rgb_" + std::to_string(fid) +
                         ".png",
                     w, h, 3, fl_rgb_bytes.data());
      fl_gt_labels = DecodeSemanticLabels(
          reinterpret_cast<const uint8_t *>(fl_pair.gt.carla_image->data()), w,
          h, assume_bgra);
      if (save_raw_bin)
        SaveBin(run_dir + "/front_left_gt_raw/gt_" + std::to_string(fid) +
                    ".bin",
                fl_gt_labels.data(), w * h);
      if (save_png) {
        vis::ColorizeCityscapes(fl_gt_labels.data(), w, h, fl_gt_color.data());
        vis::SavePng(run_dir + "/front_left_gt_color/gt_" +
                         std::to_string(fid) + ".png",
                     w, h, 3, fl_gt_color.data());
      }
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
            reinterpret_cast<const uint8_t *>(fl_pair.rgb.carla_image->data()),
            w, h, pre_cfg, fl_trt_in.data());
        if (is_logits) {
          std::vector<float> out(runner->GetOutputBytes() / sizeof(float));
          runner->Infer(fl_trt_in.data(), runner->GetInputBytes(), out.data(),
                        runner->GetOutputBytes());
          postprocess::ArgmaxLogitsToLabels(out.data(), out_c, out_h, out_w,
                                            fl_pred_labels);
        } else {
          std::vector<int32_t> out(runner->GetOutputBytes() / sizeof(int32_t));
          runner->Infer(fl_trt_in.data(), runner->GetInputBytes(), out.data(),
                        runner->GetOutputBytes());
          postprocess::DirectLabelsToUint8(out.data(), out_h, out_w,
                                           fl_pred_labels);
        }
        if (save_raw_bin)
          SaveBin(run_dir + "/front_left_pred_raw/pred_" + std::to_string(fid) +
                      ".bin",
                  fl_pred_labels.data(), out_w * out_h);
        vis::UpsampleNearest(fl_pred_labels.data(), out_w, out_h,
                             fl_pred_upsampled.data(), w, h);
        if (save_png) {
          vis::ColorizeCityscapes(fl_pred_upsampled.data(), w, h,
                                  fl_pred_color.data());
          vis::SavePng(run_dir + "/front_left_pred_color/pred_" +
                           std::to_string(fid) + ".png",
                       w, h, 3, fl_pred_color.data());
          vis::BlendOverlay(fl_rgb_bytes.data(), fl_pred_color.data(), w, h,
                            alpha, fl_overlay.data());
          vis::SavePng(run_dir + "/front_left_overlay/overlay_" +
                           std::to_string(fid) + ".png",
                       w, h, 3, fl_overlay.data());
        }
      }
      fl_saved_frames.push_back(fid);
    }

    if (got_fr_pair) {
      uint64_t fid = fr_pair.frame_id;
      vis::BgraToRgb(
          reinterpret_cast<const uint8_t *>(fr_pair.rgb.carla_image->data()), w,
          h, fr_rgb_bytes.data());
      if (save_png)
        vis::SavePng(run_dir + "/front_right_rgb/rgb_" + std::to_string(fid) +
                         ".png",
                     w, h, 3, fr_rgb_bytes.data());
      fr_gt_labels = DecodeSemanticLabels(
          reinterpret_cast<const uint8_t *>(fr_pair.gt.carla_image->data()), w,
          h, assume_bgra);
      if (save_raw_bin)
        SaveBin(run_dir + "/front_right_gt_raw/gt_" + std::to_string(fid) +
                    ".bin",
                fr_gt_labels.data(), w * h);
      if (save_png) {
        vis::ColorizeCityscapes(fr_gt_labels.data(), w, h, fr_gt_color.data());
        vis::SavePng(run_dir + "/front_right_gt_color/gt_" +
                         std::to_string(fid) + ".png",
                     w, h, 3, fr_gt_color.data());
      }
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
            reinterpret_cast<const uint8_t *>(fr_pair.rgb.carla_image->data()),
            w, h, pre_cfg, fr_trt_in.data());
        if (is_logits) {
          std::vector<float> out(runner->GetOutputBytes() / sizeof(float));
          runner->Infer(fr_trt_in.data(), runner->GetInputBytes(), out.data(),
                        runner->GetOutputBytes());
          postprocess::ArgmaxLogitsToLabels(out.data(), out_c, out_h, out_w,
                                            fr_pred_labels);
        } else {
          std::vector<int32_t> out(runner->GetOutputBytes() / sizeof(int32_t));
          runner->Infer(fr_trt_in.data(), runner->GetInputBytes(), out.data(),
                        runner->GetOutputBytes());
          postprocess::DirectLabelsToUint8(out.data(), out_h, out_w,
                                           fr_pred_labels);
        }
        if (save_raw_bin)
          SaveBin(run_dir + "/front_right_pred_raw/pred_" +
                      std::to_string(fid) + ".bin",
                  fr_pred_labels.data(), out_w * out_h);
        vis::UpsampleNearest(fr_pred_labels.data(), out_w, out_h,
                             fr_pred_upsampled.data(), w, h);
        if (save_png) {
          vis::ColorizeCityscapes(fr_pred_upsampled.data(), w, h,
                                  fr_pred_color.data());
          vis::SavePng(run_dir + "/front_right_pred_color/pred_" +
                           std::to_string(fid) + ".png",
                       w, h, 3, fr_pred_color.data());
          vis::BlendOverlay(fr_rgb_bytes.data(), fr_pred_color.data(), w, h,
                            alpha, fr_overlay.data());
          vis::SavePng(run_dir + "/front_right_overlay/overlay_" +
                           std::to_string(fid) + ".png",
                       w, h, 3, fr_overlay.data());
        }
      }
      fr_saved_frames.push_back(fid);
    }

    if (got_rl_pair) {
      uint64_t fid = rl_pair.frame_id;
      vis::BgraToRgb(
          reinterpret_cast<const uint8_t *>(rl_pair.rgb.carla_image->data()), w,
          h, rl_rgb_bytes.data());
      if (save_png)
        vis::SavePng(run_dir + "/rear_left_rgb/rgb_" + std::to_string(fid) +
                         ".png",
                     w, h, 3, rl_rgb_bytes.data());
      rl_gt_labels = DecodeSemanticLabels(
          reinterpret_cast<const uint8_t *>(rl_pair.gt.carla_image->data()), w,
          h, assume_bgra);
      if (save_raw_bin)
        SaveBin(run_dir + "/rear_left_gt_raw/gt_" + std::to_string(fid) +
                    ".bin",
                rl_gt_labels.data(), w * h);
      if (save_png) {
        vis::ColorizeCityscapes(rl_gt_labels.data(), w, h, rl_gt_color.data());
        vis::SavePng(run_dir + "/rear_left_gt_color/gt_" + std::to_string(fid) +
                         ".png",
                     w, h, 3, rl_gt_color.data());
      }
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
            reinterpret_cast<const uint8_t *>(rl_pair.rgb.carla_image->data()),
            w, h, pre_cfg, rl_trt_in.data());
        if (is_logits) {
          std::vector<float> out(runner->GetOutputBytes() / sizeof(float));
          runner->Infer(rl_trt_in.data(), runner->GetInputBytes(), out.data(),
                        runner->GetOutputBytes());
          postprocess::ArgmaxLogitsToLabels(out.data(), out_c, out_h, out_w,
                                            rl_pred_labels);
        } else {
          std::vector<int32_t> out(runner->GetOutputBytes() / sizeof(int32_t));
          runner->Infer(rl_trt_in.data(), runner->GetInputBytes(), out.data(),
                        runner->GetOutputBytes());
          postprocess::DirectLabelsToUint8(out.data(), out_h, out_w,
                                           rl_pred_labels);
        }
        if (save_raw_bin)
          SaveBin(run_dir + "/rear_left_pred_raw/pred_" + std::to_string(fid) +
                      ".bin",
                  rl_pred_labels.data(), out_w * out_h);
        vis::UpsampleNearest(rl_pred_labels.data(), out_w, out_h,
                             rl_pred_upsampled.data(), w, h);
        if (save_png) {
          vis::ColorizeCityscapes(rl_pred_upsampled.data(), w, h,
                                  rl_pred_color.data());
          vis::SavePng(run_dir + "/rear_left_pred_color/pred_" +
                           std::to_string(fid) + ".png",
                       w, h, 3, rl_pred_color.data());
          vis::BlendOverlay(rl_rgb_bytes.data(), rl_pred_color.data(), w, h,
                            alpha, rl_overlay.data());
          vis::SavePng(run_dir + "/rear_left_overlay/overlay_" +
                           std::to_string(fid) + ".png",
                       w, h, 3, rl_overlay.data());
        }
      }
      rl_saved_frames.push_back(fid);
    }

    if (got_rr_pair) {
      uint64_t fid = rr_pair.frame_id;
      vis::BgraToRgb(
          reinterpret_cast<const uint8_t *>(rr_pair.rgb.carla_image->data()), w,
          h, rr_rgb_bytes.data());
      if (save_png)
        vis::SavePng(run_dir + "/rear_right_rgb/rgb_" + std::to_string(fid) +
                         ".png",
                     w, h, 3, rr_rgb_bytes.data());
      rr_gt_labels = DecodeSemanticLabels(
          reinterpret_cast<const uint8_t *>(rr_pair.gt.carla_image->data()), w,
          h, assume_bgra);
      if (save_raw_bin)
        SaveBin(run_dir + "/rear_right_gt_raw/gt_" + std::to_string(fid) +
                    ".bin",
                rr_gt_labels.data(), w * h);
      if (save_png) {
        vis::ColorizeCityscapes(rr_gt_labels.data(), w, h, rr_gt_color.data());
        vis::SavePng(run_dir + "/rear_right_gt_color/gt_" +
                         std::to_string(fid) + ".png",
                     w, h, 3, rr_gt_color.data());
      }
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
            reinterpret_cast<const uint8_t *>(rr_pair.rgb.carla_image->data()),
            w, h, pre_cfg, rr_trt_in.data());
        if (is_logits) {
          std::vector<float> out(runner->GetOutputBytes() / sizeof(float));
          runner->Infer(rr_trt_in.data(), runner->GetInputBytes(), out.data(),
                        runner->GetOutputBytes());
          postprocess::ArgmaxLogitsToLabels(out.data(), out_c, out_h, out_w,
                                            rr_pred_labels);
        } else {
          std::vector<int32_t> out(runner->GetOutputBytes() / sizeof(int32_t));
          runner->Infer(rr_trt_in.data(), runner->GetInputBytes(), out.data(),
                        runner->GetOutputBytes());
          postprocess::DirectLabelsToUint8(out.data(), out_h, out_w,
                                           rr_pred_labels);
        }
        if (save_raw_bin)
          SaveBin(run_dir + "/rear_right_pred_raw/pred_" + std::to_string(fid) +
                      ".bin",
                  rr_pred_labels.data(), out_w * out_h);
        vis::UpsampleNearest(rr_pred_labels.data(), out_w, out_h,
                             rr_pred_upsampled.data(), w, h);
        if (save_png) {
          vis::ColorizeCityscapes(rr_pred_upsampled.data(), w, h,
                                  rr_pred_color.data());
          vis::SavePng(run_dir + "/rear_right_pred_color/pred_" +
                           std::to_string(fid) + ".png",
                       w, h, 3, rr_pred_color.data());
          vis::BlendOverlay(rr_rgb_bytes.data(), rr_pred_color.data(), w, h,
                            alpha, rr_overlay.data());
          vis::SavePng(run_dir + "/rear_right_overlay/overlay_" +
                           std::to_string(fid) + ".png",
                       w, h, 3, rr_overlay.data());
        }
      }
      rr_saved_frames.push_back(fid);
    }

    total_processed++;

    if (viewer) {
      bool all_ready = true;
      if (active_cameras.count(0) && !got_pair)
        all_ready = false;
      if (active_cameras.count(1) && !got_rear_pair)
        all_ready = false;
      if (active_cameras.count(2) && !got_left_pair)
        all_ready = false;
      if (active_cameras.count(3) && !got_right_pair)
        all_ready = false;
      if (active_cameras.count(4) && !got_fl_pair)
        all_ready = false;
      if (active_cameras.count(5) && !got_fr_pair)
        all_ready = false;
      if (active_cameras.count(6) && !got_rl_pair)
        all_ready = false;
      if (active_cameras.count(7) && !got_rr_pair)
        all_ready = false;

      if (all_ready) {
        const uint8_t *f_ptr = no_pred ? rgb_bytes.data() : overlay.data();
        const uint8_t *r_ptr =
            no_pred ? rear_rgb_bytes.data() : rear_overlay.data();
        const uint8_t *l_ptr =
            no_pred ? left_rgb_bytes.data() : left_overlay.data();
        const uint8_t *ri_ptr =
            no_pred ? right_rgb_bytes.data() : right_overlay.data();
        const uint8_t *fl_ptr =
            no_pred ? fl_rgb_bytes.data() : fl_overlay.data();
        const uint8_t *fr_ptr =
            no_pred ? fr_rgb_bytes.data() : fr_overlay.data();
        const uint8_t *rl_ptr =
            no_pred ? rl_rgb_bytes.data() : rl_overlay.data();
        const uint8_t *rr_ptr =
            no_pred ? rr_rgb_bytes.data() : rr_overlay.data();

        std::vector<const uint8_t *> frames;
        if (active_cameras.count(0))
          frames.push_back(f_ptr);
        if (active_cameras.count(1))
          frames.push_back(r_ptr);
        if (active_cameras.count(2))
          frames.push_back(l_ptr);
        if (active_cameras.count(3))
          frames.push_back(ri_ptr);
        if (active_cameras.count(4))
          frames.push_back(fl_ptr);
        if (active_cameras.count(5))
          frames.push_back(fr_ptr);
        if (active_cameras.count(6))
          frames.push_back(rl_ptr);
        if (active_cameras.count(7))
          frames.push_back(rr_ptr);

        vis::CreateDynamicMosaic(frames, mosaic_cols, mosaic_rows, w, h,
                                 mosaic.data());

        viewer->submit_frame_rgb888(mosaic.data(), w * mosaic_cols,
                                    h * mosaic_rows);
        viewer->render_latest();
      }
    }

    if (total_processed % print_every == 0) {
      uint64_t print_fid = got_pair ? pair.frame_id : rear_pair.frame_id;
      std::cout << "[Sanity Dataset] Saved frame " << total_processed << " / "
                << target_frames << " (ID: " << print_fid << ")\n";
    }
  }

  std::cout << "[Sanity Dataset] Finished writing " << target_frames
            << " frames.\n";

  // Write meta.json
  std::ofstream meta(run_dir + "/meta.json");
  meta << "{\n";
  meta << "  \"date_time\": \"" << run_dir << "\",\n";
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
  meta << "\n  ],\n";
  meta << "  \"rear_frames\": [\n    ";
  for (size_t i = 0; i < rear_saved_frames.size(); ++i) {
    meta << rear_saved_frames[i]
         << (i < rear_saved_frames.size() - 1 ? ", " : "");
    if ((i + 1) % 10 == 0 && i < rear_saved_frames.size() - 1)
      meta << "\n    ";
  }
  meta << "\n  ],\n";
  meta << "  \"left_frames\": [\n    ";
  for (size_t i = 0; i < left_saved_frames.size(); ++i) {
    meta << left_saved_frames[i]
         << (i < left_saved_frames.size() - 1 ? ", " : "");
    if ((i + 1) % 10 == 0 && i < left_saved_frames.size() - 1)
      meta << "\n    ";
  }
  meta << "\n  ],\n";
  meta << "  \"right_frames\": [\n    ";
  for (size_t i = 0; i < right_saved_frames.size(); ++i) {
    meta << right_saved_frames[i]
         << (i < right_saved_frames.size() - 1 ? ", " : "");
    if ((i + 1) % 10 == 0 && i < right_saved_frames.size() - 1)
      meta << "\n    ";
  }
  meta << "\n  ],\n";
  meta << "  \"fl_frames\": [\n    ";
  for (size_t i = 0; i < fl_saved_frames.size(); ++i) {
    meta << fl_saved_frames[i] << (i < fl_saved_frames.size() - 1 ? ", " : "");
    if ((i + 1) % 10 == 0 && i < fl_saved_frames.size() - 1)
      meta << "\n    ";
  }
  meta << "\n  ],\n";
  meta << "  \"fr_frames\": [\n    ";
  for (size_t i = 0; i < fr_saved_frames.size(); ++i) {
    meta << fr_saved_frames[i] << (i < fr_saved_frames.size() - 1 ? ", " : "");
    if ((i + 1) % 10 == 0 && i < fr_saved_frames.size() - 1)
      meta << "\n    ";
  }
  meta << "\n  ],\n";
  meta << "  \"rl_frames\": [\n    ";
  for (size_t i = 0; i < rl_saved_frames.size(); ++i) {
    meta << rl_saved_frames[i] << (i < rl_saved_frames.size() - 1 ? ", " : "");
    if ((i + 1) % 10 == 0 && i < rl_saved_frames.size() - 1)
      meta << "\n    ";
  }
  meta << "\n  ],\n";
  meta << "  \"rr_frames\": [\n    ";
  for (size_t i = 0; i < rr_saved_frames.size(); ++i) {
    meta << rr_saved_frames[i] << (i < rr_saved_frames.size() - 1 ? ", " : "");
    if ((i + 1) % 10 == 0 && i < rr_saved_frames.size() - 1)
      meta << "\n    ";
  }
  meta << "\n  ]\n";
  meta << "}\n";

  if (gt_sensor)
    gt_sensor->Destroy();
  if (rgb_sensor)
    rgb_sensor->Destroy();
  if (rear_gt_sensor)
    rear_gt_sensor->Destroy();
  if (rear_rgb_sensor)
    rear_rgb_sensor->Destroy();
  if (left_gt_sensor)
    left_gt_sensor->Destroy();
  if (left_rgb_sensor)
    left_rgb_sensor->Destroy();
  if (right_gt_sensor)
    right_gt_sensor->Destroy();
  if (right_rgb_sensor)
    right_rgb_sensor->Destroy();
  if (fl_gt_sensor)
    fl_gt_sensor->Destroy();
  if (fl_rgb_sensor)
    fl_rgb_sensor->Destroy();
  if (fr_gt_sensor)
    fr_gt_sensor->Destroy();
  if (fr_rgb_sensor)
    fr_rgb_sensor->Destroy();
  if (rl_gt_sensor)
    rl_gt_sensor->Destroy();
  if (rl_rgb_sensor)
    rl_rgb_sensor->Destroy();
  if (rr_gt_sensor)
    rr_gt_sensor->Destroy();
  if (rr_rgb_sensor)
    rr_rgb_sensor->Destroy();
  if (vehicle)
    vehicle->Destroy();

  tm.SetSynchronousMode(false);
  settings.synchronous_mode = original_sync;
  world.ApplySettings(settings, std::chrono::seconds(2));

  return 0;
}
