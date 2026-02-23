#include "frame_sync.hpp"
#include "semantic_decode.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <iomanip>
#include <iostream>
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
  std::cout << "\nSIGINT received (" << signum << "). Stopping gracefully..."
            << std::endl;
  g_quit = true;
}

int main(int argc, char **argv) {
  std::signal(SIGINT, signal_handler);

  std::string host = "127.0.0.1";
  uint16_t port = 2000;
  std::string map_name = "";
  int w = 800;
  int h = 600;
  int fps = 20;
  int frames_to_capture = 200; // mapped to --matches
  float sensor_tick = -1.0f;
  int window_size = 50;
  int print_every = 20;
  bool assume_bgra = true; // 1 means BGRA, 0 means RGBA
  bool gt_decode_stats = false;
  bool use_sync_mode = true;

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
    } else if (arg == "--matches") {
      if (i + 1 < argc)
        frames_to_capture = std::stoi(argv[++i]);
    } else if (arg == "--sensor_tick") {
      if (i + 1 < argc)
        sensor_tick = std::stof(argv[++i]);
    } else if (arg == "--window") {
      if (i + 1 < argc)
        window_size = std::stoi(argv[++i]);
    } else if (arg == "--print_every") {
      if (i + 1 < argc)
        print_every = std::stoi(argv[++i]);
    } else if (arg == "--assume_bgra") {
      if (i + 1 < argc)
        assume_bgra = (std::stoi(argv[++i]) != 0);
    } else if (arg == "--gt_decode_stats") {
      if (i + 1 < argc)
        gt_decode_stats = (std::stoi(argv[++i]) != 0);
    } else if (arg == "--sync") {
      if (i + 1 < argc)
        use_sync_mode = (std::stoi(argv[++i]) != 0);
    }
  }

  if (sensor_tick < 0.0f) {
    sensor_tick = 1.0f / fps;
  }

  std::cout << "Connecting to CARLA at " << host << ":" << port << "..."
            << std::endl;
  std::cout << "Sync Window Size: " << window_size
            << " | Matches Target: " << frames_to_capture << std::endl;
  std::cout << "Synchronous Mode: " << (use_sync_mode ? "ON" : "OFF")
            << std::endl;

  boost::shared_ptr<cc::Actor> vehicle;
  boost::shared_ptr<cc::Sensor> rgb_camera;
  boost::shared_ptr<cc::Sensor> gt_camera;

  try {
    auto client = carla::client::Client(host, port);
    client.SetTimeout(std::chrono::seconds(10));

    if (!map_name.empty()) {
      std::cout << "Loading map: " << map_name << "..." << std::endl;
      client.LoadWorld(map_name);
    }

    auto world = client.GetWorld();

    // Set synchronous mode if requested
    auto original_settings = world.GetSettings();
    if (use_sync_mode) {
      auto settings = original_settings;
      settings.synchronous_mode = true;
      settings.fixed_delta_seconds = 1.0 / fps; // Match the sensor tick exactly
      world.ApplySettings(settings, std::chrono::seconds(2));
    }

    auto map = world.GetMap();
    std::cout << "Connected to world: " << map->GetName() << std::endl;

    // Spawn Vehicle
    auto blueprint_library = world.GetBlueprintLibrary();
    auto vehicle_bp_ptr = blueprint_library->Find("vehicle.tesla.model3");
    if (!vehicle_bp_ptr)
      vehicle_bp_ptr = &(*blueprint_library->Filter("vehicle.*")->begin());
    ca::ActorBlueprint vehicle_bp = *vehicle_bp_ptr;

    auto spawn_points = map->GetRecommendedSpawnPoints();
    if (spawn_points.empty()) {
      std::cerr << "ERROR: No spawn points found in the map!" << std::endl;
      return EXIT_FAILURE;
    }

    auto spawn_pt = spawn_points[0];
    spawn_pt.location.z += 1.0f;

    vehicle = world.TrySpawnActor(vehicle_bp, spawn_pt);
    if (!vehicle) {
      std::cerr << "ERROR: Failed to spawn vehicle!" << std::endl;
      return EXIT_FAILURE;
    }
    std::cout << "Spawned EGO vehicle ID: " << vehicle->GetId() << std::endl;

    // Common transform for both cameras
    carla::geom::Transform cam_transform(
        carla::geom::Location(1.5f, 0.0f, 1.4f),
        carla::geom::Rotation(0.0f, 0.0f, 0.0f));

    // Spawn RGB Camera
    auto rgb_bp_ptr = blueprint_library->Find("sensor.camera.rgb");
    ca::ActorBlueprint rgb_bp = *rgb_bp_ptr;
    rgb_bp.SetAttribute("image_size_x", std::to_string(w));
    rgb_bp.SetAttribute("image_size_y", std::to_string(h));
    rgb_bp.SetAttribute("sensor_tick", std::to_string(sensor_tick));

    auto rgb_actor = world.TrySpawnActor(rgb_bp, cam_transform, vehicle.get());
    if (!rgb_actor) {
      std::cerr << "ERROR: Failed to spawn RGB camera!" << std::endl;
      vehicle->Destroy();
      return EXIT_FAILURE;
    }
    rgb_camera = boost::static_pointer_cast<cc::Sensor>(rgb_actor);
    std::cout << "Spawned RGB Camera ID: " << rgb_camera->GetId() << std::endl;

    // Spawn GT Camera
    auto gt_bp_ptr =
        blueprint_library->Find("sensor.camera.semantic_segmentation");
    ca::ActorBlueprint gt_bp = *gt_bp_ptr;
    gt_bp.SetAttribute("image_size_x", std::to_string(w));
    gt_bp.SetAttribute("image_size_y", std::to_string(h));
    gt_bp.SetAttribute("sensor_tick", std::to_string(sensor_tick));

    auto gt_actor = world.TrySpawnActor(gt_bp, cam_transform, vehicle.get());
    if (!gt_actor) {
      std::cerr << "ERROR: Failed to spawn GT camera!" << std::endl;
      rgb_camera->Destroy();
      vehicle->Destroy();
      return EXIT_FAILURE;
    }
    gt_camera = boost::static_pointer_cast<cc::Sensor>(gt_actor);
    std::cout << "Spawned GT Camera ID: " << gt_camera->GetId() << std::endl;

    // Initialize Synchronizer
    FrameSync sync(window_size);

    // Listeners
    rgb_camera->Listen([&](auto data) {
      if (g_quit)
        return;
      auto img = boost::static_pointer_cast<csd::Image>(data);
      FrameIn fin;
      fin.frame_id = img->GetFrame();
      fin.timestamp = img->GetTimestamp();
      fin.w = w;
      fin.h = h;
      fin.carla_image = img;
      sync.PushRgb(std::move(fin));
    });

    gt_camera->Listen([&](auto data) {
      if (g_quit)
        return;
      auto img = boost::static_pointer_cast<csd::Image>(data);
      GtFrame gtin;
      gtin.frame_id = img->GetFrame();
      gtin.timestamp = img->GetTimestamp();
      gtin.w = w;
      gtin.h = h;
      gtin.carla_image = img;
      sync.PushGt(std::move(gtin));
    });

    // Main Consumer Loop
    std::cout << "Waiting for " << frames_to_capture << " matched pairs..."
              << std::endl;
    int matched_count = 0;

    while (!g_quit) {
      if (use_sync_mode) {
        world.Tick(std::chrono::seconds(2));
      }

      MatchedPair pair;
      while (sync.TryPopMatched(pair)) {
        matched_count++;

        if (matched_count % print_every == 0 ||
            matched_count == frames_to_capture) {
          SyncStats stats = sync.GetStats();
          double dt_diff = pair.gt.timestamp - pair.rgb.timestamp;

          size_t min_pushed = std::min(stats.pushed_rgb, stats.pushed_gt);
          double match_rate =
              min_pushed > 0 ? static_cast<double>(stats.matched) / min_pushed
                             : 0.0;

          std::cout << "\n--- [Sync Stream] Match " << matched_count << "/"
                    << frames_to_capture << " ---" << std::endl;
          std::cout << "Frame ID  : " << pair.frame_id << std::endl;
          std::cout << "Timestamps: RGB=" << std::fixed << std::setprecision(4)
                    << pair.rgb.timestamp << " | GT=" << pair.gt.timestamp
                    << " | Delta=" << dt_diff << "s" << std::endl;
          std::cout << "Match Rate: " << std::fixed << std::setprecision(2)
                    << (match_rate * 100.0) << "% (" << stats.matched
                    << " matched / " << min_pushed << " min_pushed)"
                    << std::endl;
          std::cout << "Buffered  : RGB=" << stats.currently_buffered_rgb
                    << " | GT=" << stats.currently_buffered_gt << std::endl;
          std::cout << "Drops     : RGB(O=" << stats.dropped_rgb_overflow
                    << ",A=" << stats.dropped_rgb_age << ") "
                    << "| GT(O=" << stats.dropped_gt_overflow
                    << ",A=" << stats.dropped_gt_age << ")" << std::endl;

          if (gt_decode_stats) {
            auto labels = DecodeSemanticLabels(
                reinterpret_cast<const uint8_t *>(pair.gt.carla_image->data()),
                w, h, assume_bgra);
            auto sem_stats = ComputeSemanticStats(labels);
            std::cout << "GT Sanity : " << sem_stats.unique_labels.size()
                      << " unique labels found." << std::endl;
          }
        }

        if (matched_count >= frames_to_capture) {
          g_quit = true;
          break;
        }
      }

      if (!use_sync_mode) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }

    rgb_camera->Stop();
    gt_camera->Stop();

    // Restore settings
    if (use_sync_mode) {
      world.ApplySettings(original_settings, std::chrono::seconds(2));
    }

  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    if (rgb_camera)
      rgb_camera->Destroy();
    if (gt_camera)
      gt_camera->Destroy();
    if (vehicle)
      vehicle->Destroy();
    return EXIT_FAILURE;
  }

  std::cout << "Cleaning up..." << std::endl;
  if (rgb_camera)
    rgb_camera->Destroy();
  if (gt_camera)
    gt_camera->Destroy();
  if (vehicle)
    vehicle->Destroy();
  std::cout << "Clean exit. Sync Smoke test complete." << std::endl;

  return EXIT_SUCCESS;
}
