#include "preprocess.hpp"
#include "tensor_stats.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iostream>
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
#include <carla/geom/Transform.h>
#include <carla/sensor/data/Image.h>

namespace cc = carla::client;
namespace cg = carla::geom;
namespace csd = carla::sensor::data;
namespace ca = carla::actors;

std::atomic<bool> g_quit{false};

void signal_handler(int signum) {
  std::cout << "\nSIGINT received. Stopping gracefully..." << std::endl;
  g_quit = true;
}

// Utility to parse comma separated floats
void ParseFloats(const std::string &str, float out[3]) {
  std::stringstream ss(str);
  std::string item;
  int i = 0;
  while (std::getline(ss, item, ',') && i < 3) {
    out[i++] = std::stof(item);
  }
}

int main(int argc, char **argv) {
  std::signal(SIGINT, signal_handler);

  std::string host = "127.0.0.1";
  uint16_t port = 2000;
  std::string map_name = "";
  int w = 800; // Camera Capture Width
  int h = 600; // Camera Capture Height
  int fps = 20;
  int frames_to_capture = 1;

  PreprocessConfig cfg;
  cfg.out_w = 512;
  cfg.out_h = 256;
  cfg.assume_bgra = true;
  cfg.to_rgb = true;
  cfg.resize = PreprocessConfig::BILINEAR;

  // Default ImageNet Normalization
  cfg.mean[0] = 0.485f;
  cfg.mean[1] = 0.456f;
  cfg.mean[2] = 0.406f;
  cfg.std[0] = 0.229f;
  cfg.std[1] = 0.224f;
  cfg.std[2] = 0.225f;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--host") {
      if (i + 1 < argc)
        host = argv[++i];
    } else if (arg == "--port") {
      if (i + 1 < argc)
        port = std::stoi(argv[++i]);
    } else if (arg == "--map") {
      if (i + 1 < argc)
        map_name = argv[++i];
    } else if (arg == "--w") {
      if (i + 1 < argc)
        w = std::stoi(argv[++i]);
    } else if (arg == "--h") {
      if (i + 1 < argc)
        h = std::stoi(argv[++i]);
    } else if (arg == "--fps") {
      if (i + 1 < argc)
        fps = std::stoi(argv[++i]);
    } else if (arg == "--frames") {
      if (i + 1 < argc)
        frames_to_capture = std::stoi(argv[++i]);
    } else if (arg == "--out_w") {
      if (i + 1 < argc)
        cfg.out_w = std::stoi(argv[++i]);
    } else if (arg == "--out_h") {
      if (i + 1 < argc)
        cfg.out_h = std::stoi(argv[++i]);
    } else if (arg == "--assume_bgra") {
      if (i + 1 < argc)
        cfg.assume_bgra = (std::stoi(argv[++i]) != 0);
    } else if (arg == "--to_rgb") {
      if (i + 1 < argc)
        cfg.to_rgb = (std::stoi(argv[++i]) != 0);
    } else if (arg == "--resize") {
      if (i + 1 < argc) {
        std::string mode = argv[++i];
        if (mode == "nearest")
          cfg.resize = PreprocessConfig::NEAREST;
        else
          cfg.resize = PreprocessConfig::BILINEAR;
      }
    } else if (arg == "--mean") {
      if (i + 1 < argc)
        ParseFloats(argv[++i], cfg.mean);
    } else if (arg == "--std") {
      if (i + 1 < argc)
        ParseFloats(argv[++i], cfg.std);
    }
  }

  std::cout << "Connecting to CARLA at " << host << ":" << port << "..."
            << std::endl;
  std::cout << "Capture Dims: " << w << "x" << h << " @ " << fps << " FPS"
            << std::endl;
  std::cout << "Target Dims : " << cfg.out_w << "x" << cfg.out_h
            << " (to_rgb=" << cfg.to_rgb << ")" << std::endl;

  boost::shared_ptr<cc::Actor> vehicle;
  boost::shared_ptr<cc::Sensor> rgb_camera;

  try {
    auto client = carla::client::Client(host, port);
    client.SetTimeout(std::chrono::seconds(10));

    if (!map_name.empty()) {
      std::cout << "Loading map: " << map_name << "..." << std::endl;
      client.LoadWorld(map_name);
    }

    auto world = client.GetWorld();
    auto map = world.GetMap();

    auto blueprint_library = world.GetBlueprintLibrary();
    auto vehicle_bp_ptr = blueprint_library->Find("vehicle.tesla.model3");
    if (!vehicle_bp_ptr)
      vehicle_bp_ptr = &(*blueprint_library->Filter("vehicle.*")->begin());
    ca::ActorBlueprint vehicle_bp = *vehicle_bp_ptr;

    auto spawn_points = map->GetRecommendedSpawnPoints();
    if (spawn_points.empty()) {
      std::cerr << "ERROR: No spawn points available." << std::endl;
      return EXIT_FAILURE;
    }

    auto spawn_pt = spawn_points[0];
    spawn_pt.location.z += 1.0f;
    vehicle = world.TrySpawnActor(vehicle_bp, spawn_pt);
    if (!vehicle) {
      std::cerr << "ERROR: Failed to spawn ego vehicle." << std::endl;
      return EXIT_FAILURE;
    }

    carla::geom::Transform cam_transform(
        carla::geom::Location(1.5f, 0.0f, 1.4f),
        carla::geom::Rotation(0.0f, 0.0f, 0.0f));

    auto rgb_bp_ptr = blueprint_library->Find("sensor.camera.rgb");
    ca::ActorBlueprint rgb_bp = *rgb_bp_ptr;
    rgb_bp.SetAttribute("image_size_x", std::to_string(w));
    rgb_bp.SetAttribute("image_size_y", std::to_string(h));
    rgb_bp.SetAttribute("sensor_tick", std::to_string(1.0f / fps));

    auto rgb_actor = world.TrySpawnActor(rgb_bp, cam_transform, vehicle.get());
    if (!rgb_actor) {
      std::cerr << "ERROR: Failed to spawn camera." << std::endl;
      vehicle->Destroy();
      return EXIT_FAILURE;
    }
    rgb_camera = boost::static_pointer_cast<cc::Sensor>(rgb_actor);

    int frame_count = 0;

    // Allocate Host Preprocessing Buffer
    size_t nchw_size = 3 * cfg.out_w * cfg.out_h;
    std::vector<float> tensor_buffer(nchw_size, 0.0f);

    rgb_camera->Listen([&](auto data) {
      if (g_quit)
        return;
      auto img = boost::static_pointer_cast<csd::Image>(data);

      // Just drop frames if we are busy to avoid queue pileup blocking CARLA
      if (frame_count < frames_to_capture) {
        frame_count++;
        std::cout << "\n[Preprocess Smoke] Working on Frame " << img->GetFrame()
                  << "..." << std::endl;

        bool success = PreprocessBGRAtoNCHW_F32(
            reinterpret_cast<const uint8_t *>(img->data()), w, h, cfg,
            tensor_buffer.data());

        if (success) {
          auto stats = ComputeTensorStatsNCHW(tensor_buffer.data(), 3,
                                              cfg.out_w, cfg.out_h);
          PrintTensorStats(stats);

          std::cout << "First 10 tensor floats: [";
          for (int i = 0; i < std::min(10, (int)nchw_size); i++) {
            std::cout << std::fixed << std::setprecision(4) << tensor_buffer[i]
                      << (i < 9 ? ", " : "");
          }
          std::cout << "]" << std::endl;
        } else {
          std::cerr << "Preprocessing Execution Failed!" << std::endl;
        }

        if (frame_count >= frames_to_capture)
          g_quit = true;
      }
    });

    std::cout << "Waiting for " << frames_to_capture << " frames..."
              << std::endl;
    while (!g_quit) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    rgb_camera->Stop();

  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    if (rgb_camera)
      rgb_camera->Destroy();
    if (vehicle)
      vehicle->Destroy();
    return EXIT_FAILURE;
  }

  if (rgb_camera)
    rgb_camera->Destroy();
  if (vehicle)
    vehicle->Destroy();

  std::cout << "Preprocess smoke clean exit." << std::endl;
  return EXIT_SUCCESS;
}
