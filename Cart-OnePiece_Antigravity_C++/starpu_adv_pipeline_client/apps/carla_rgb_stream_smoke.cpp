#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
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
  int frames_to_capture = 200;
  float sensor_tick = -1.0f;
  int fov = 90;

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
    } else if (arg == "--sensor_tick") {
      if (i + 1 < argc)
        sensor_tick = std::stof(argv[++i]);
    } else if (arg == "--fov") {
      if (i + 1 < argc)
        fov = std::stoi(argv[++i]);
    }
  }

  if (sensor_tick < 0.0f) {
    sensor_tick = 1.0f / fps;
  }

  std::cout << "Connecting to CARLA at " << host << ":" << port << "..."
            << std::endl;

  boost::shared_ptr<cc::Actor> vehicle;
  boost::shared_ptr<cc::Sensor> camera;

  try {
    auto client = carla::client::Client(host, port);
    client.SetTimeout(std::chrono::seconds(10));

    if (!map_name.empty()) {
      std::cout << "Loading map: " << map_name << "..." << std::endl;
      client.LoadWorld(map_name);
    }

    auto world = client.GetWorld();
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
    spawn_pt.location.z += 1.0f; // Prevent spawn collision

    vehicle = world.TrySpawnActor(vehicle_bp, spawn_pt);
    if (!vehicle) {
      std::cerr << "ERROR: Failed to spawn vehicle!" << std::endl;
      return EXIT_FAILURE;
    }
    std::cout << "Spawned EGO vehicle ID: " << vehicle->GetId() << std::endl;

    // Spawn Camera
    auto cam_bp_ptr = blueprint_library->Find("sensor.camera.rgb");
    ca::ActorBlueprint cam_bp = *cam_bp_ptr;
    cam_bp.SetAttribute("image_size_x", std::to_string(w));
    cam_bp.SetAttribute("image_size_y", std::to_string(h));
    cam_bp.SetAttribute("fov", std::to_string(fov));
    cam_bp.SetAttribute("sensor_tick", std::to_string(sensor_tick));

    // Front camera transform
    carla::geom::Transform cam_transform(
        carla::geom::Location(1.5f, 0.0f, 1.4f),
        carla::geom::Rotation(0.0f, 0.0f, 0.0f));

    auto cam_actor = world.TrySpawnActor(cam_bp, cam_transform, vehicle.get());
    if (!cam_actor) {
      std::cerr << "ERROR: Failed to spawn RGB camera!" << std::endl;
      vehicle->Destroy();
      return EXIT_FAILURE;
    }
    std::cout << "Spawned RGB Camera ID: " << cam_actor->GetId() << std::endl;

    camera = boost::static_pointer_cast<cc::Sensor>(cam_actor);

    // State for tracking
    std::atomic<int> frame_count{0};
    std::atomic<bool> buffer_error{false};
    size_t expected_size = static_cast<size_t>(w * h * 4);

    int drop_estimate = 0;
    size_t last_frame_id = 0;
    auto last_time = std::chrono::steady_clock::now();
    double total_dt_ms = 0.0;
    int valid_dts = 0;

    // Listen
    camera->Listen([&](auto data) {
      if (g_quit)
        return;

      auto now = std::chrono::steady_clock::now();
      auto img = boost::static_pointer_cast<csd::Image>(data);

      // Raw buffer check
      size_t byte_size = img->size() * 4;
      if (byte_size != expected_size) {
        std::cerr << "ERROR: Buffer size mismatch! Expected " << expected_size
                  << ", got " << byte_size << std::endl;
        buffer_error = true;
        g_quit = true;
        return;
      }

      size_t frame_id = img->GetFrame();
      if (last_frame_id > 0 && frame_id > last_frame_id + 1) {
        drop_estimate += (frame_id - last_frame_id - 1);
      }
      last_frame_id = frame_id;

      double dt_ms =
          std::chrono::duration<double, std::milli>(now - last_time).count();
      if (valid_dts > 0 || drop_estimate > 0) { // skip very first dt
        total_dt_ms += dt_ms;
        valid_dts++;
      }
      last_time = now;

      frame_count++;
      if (frame_count % 20 == 0 || frame_count == frames_to_capture) {
        double avg_dt = valid_dts > 0 ? (total_dt_ms / valid_dts) : 0.0;
        std::cout << "[RGB Stream] Received: " << frame_count << "/"
                  << frames_to_capture << " | Buffer: " << byte_size << " bytes"
                  << " | Last dt: " << dt_ms << " ms"
                  << " | Avg dt: " << avg_dt << " ms"
                  << " | Drops approx: " << drop_estimate << std::endl;
      }

      if (frame_count >= frames_to_capture) {
        g_quit = true;
      }
    });

    // Wait loop
    std::cout << "Waiting for " << frames_to_capture << " frames at ~" << fps
              << " FPS..." << std::endl;
    while (!g_quit) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    camera->Stop();

    if (buffer_error) {
      throw std::runtime_error("Exited due to raw buffer size mismatch abort.");
    }

  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    if (camera)
      camera->Destroy();
    if (vehicle)
      vehicle->Destroy();
    return EXIT_FAILURE;
  }

  std::cout << "Cleaning up..." << std::endl;
  if (camera)
    camera->Destroy();
  if (vehicle)
    vehicle->Destroy();
  std::cout << "Clean exit. RGB Smoke test complete." << std::endl;

  return EXIT_SUCCESS;
}
