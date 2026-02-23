/**
 * CARLA C++ Client Example: Spawn a Mercedes Coupe (Smart IO & Safe Driving)
 *
 * Requirements:
 * - Connect to 127.0.0.1:2000
 * - Prefer "vehicle.mercedes.coupe"
 * - Retry spawn on failure
 * - Autopilot ON (Speed reduced by 50%)
 * - Async IO for dataset recording
 * - RAII Cleanup
 */

#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <mutex>
#include <set>
#include <queue>
#include <condition_variable>
#include <functional>

// CARLA headers
#include <carla/client/Client.h>
#include <carla/client/Actor.h>
#include <carla/actors/BlueprintLibrary.h>
#include <carla/actors/ActorBlueprint.h>
#include <carla/client/Map.h>
#include <carla/client/World.h>
#include <carla/geom/Transform.h>
#include <carla/client/Sensor.h>
#include <carla/sensor/data/Image.h>
#include <carla/sensor/data/DVSEventArray.h>
#include <carla/image/CityScapesPalette.h>
#include <carla/rpc/ActorId.h>
#include <cmath>

namespace cc = carla::client;
namespace cg = carla::geom;
namespace csd = carla::sensor::data;
namespace cr = carla::rpc;
namespace ca = carla::actors;

using namespace std::chrono_literals;

// Async Writer to avoid blocking the game thread during IO (Producer-Consumer)
// Async Writer with Thread Pool (4 threads)
class AsyncWriter {
public:
    AsyncWriter() : running_(true) {
        // Create 4 worker threads
        for (int i = 0; i < 4; ++i) {
            workers_.emplace_back(&AsyncWriter::Loop, this);
        }
    }

    ~AsyncWriter() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_all(); // Wake up all workers
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void Push(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

private:
    void Loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]{ return !tasks_.empty() || !running_; });
                
                if (!running_ && tasks_.empty()) {
                    return;
                }
                
                if (tasks_.empty()) continue;

                task = std::move(tasks_.front());
                tasks_.pop();
            }
            if (task) task();
        }
    }

    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool running_;
    std::vector<std::thread> workers_;
};

// Helper to clean up actor on exit
class ActorDestroyer {
public:
    explicit ActorDestroyer(boost::shared_ptr<cc::Actor> actor) : actor_(std::move(actor)) {}
    
    ~ActorDestroyer() {
        if (actor_) {
            try {
                actor_->Destroy();
                std::cout << "Actor " << actor_->GetDisplayId() << " (ID: " << actor_->GetId() << ") destroyed." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error destroying actor: " << e.what() << std::endl;
            }
        }
    }

    ActorDestroyer(const ActorDestroyer&) = delete;
    ActorDestroyer& operator=(const ActorDestroyer&) = delete;

private:
    boost::shared_ptr<cc::Actor> actor_;
};

int main(int argc, char* argv[]) {
    // 1. Initialize AsyncWriter FIRST so it is destroyed LAST
    AsyncWriter writer;

    try {
        std::string host = "127.0.0.1";
        uint16_t port = 2000;

        if (argc > 1) host = argv[1];
        if (argc > 2) port = static_cast<uint16_t>(std::stoi(argv[2]));

        std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;

        cc::Client client(host, port);
        client.SetTimeout(10s); 

        std::cout << "Connected!" << std::endl;
        std::cout << "Client API version: " << client.GetClientVersion() << std::endl;
        std::cout << "Server API version: " << client.GetServerVersion() << std::endl;

        auto world = client.GetWorld();
        auto library = world.GetBlueprintLibrary();

        // 2. Select Blueprint
        auto blueprint = library->Find("vehicle.mercedes.coupe");
        
        if (!blueprint) {
            std::cout << "Warning: 'vehicle.mercedes.coupe' not found. Selecting a random vehicle..." << std::endl;
            auto vehicles = library->Filter("vehicle");
            if (vehicles->empty()) {
                throw std::runtime_error("No vehicle blueprints found!");
            }
            std::mt19937_64 rng((std::random_device())());
            std::uniform_int_distribution<size_t> dist{0u, vehicles->size() - 1u};
            blueprint = &vehicles->at(dist(rng));
        }

        ca::ActorBlueprint mutable_blueprint = *blueprint;
        if (mutable_blueprint.ContainsAttribute("color")) {
            mutable_blueprint.SetAttribute("color", "255,255,0");
        }
        if (mutable_blueprint.ContainsAttribute("role_name")) {
            mutable_blueprint.SetAttribute("role_name", "autopilot");
        }

        std::cout << "Selected Blueprint: " << mutable_blueprint.GetId() << std::endl;

        // 3. Spawn Vehicle
        auto map = world.GetMap();
        auto spawn_points = map->GetRecommendedSpawnPoints();
        if (spawn_points.empty()) throw std::runtime_error("No spawn points available!");

        boost::shared_ptr<cc::Actor> actor;
        cg::Transform spawn_transform;
        
        // Shuffle spawn points
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(spawn_points.begin(), spawn_points.end(), g);
        
        for (const auto& transform : spawn_points) {
            actor = world.TrySpawnActor(mutable_blueprint, transform);
            if (actor) {
                spawn_transform = transform;
                break;
            }
        }

        if (!actor) throw std::runtime_error("Failed to spawn actor at any recommended spawn point.");

        ActorDestroyer destroyer(actor);

        std::cout << "Successfully Spawned Actor: " << actor->GetId() << " at " << spawn_transform.location.x << ", " << spawn_transform.location.y << std::endl;

        // 4. Configure Autopilot & Traffic Manager
        auto vehicle = boost::static_pointer_cast<cc::Vehicle>(actor);
        if (vehicle) {
             vehicle->SetAutopilot(true, 8000);
             
             // Get TrafficManager and configure speed
             auto tm = client.GetInstanceTM(8000);
             
             // Reduce speed by 70% to prevent "drunk" driving (oscillation/overshoot)
             tm.SetPercentageSpeedDifference(vehicle, 70.0); 
             tm.SetGlobalPercentageSpeedDifference(70.0); // Apply globally for safety

             std::cout << "  - Autopilot: ON (Port 8000)" << std::endl;
             std::cout << "  - Speed Reduction: 70%" << std::endl;
        }

        // 5. Setup Dataset Recording (Async)
        const std::string base_dir = "/media/eric/INTEL_32G/Cart-OnePiece_Datasets/antigravity_cpp_yellow/";
        
        if (system(("mkdir -p " + base_dir).c_str()) != 0) {
            throw std::runtime_error("Failed to create/access dataset root: " + base_dir);
        }

        const std::vector<std::string> sensors = {"rgb", "depth", "semantic", "instance", "optflow", "dvs"};
        for (const auto& s : sensors) system(("mkdir -p " + base_dir + s).c_str());

        // Shared Manifest State (Thread-Safe)
        struct ManifestState {
            struct FrameEntry { std::map<std::string, std::string> paths; };
            std::map<uint64_t, FrameEntry> buffer;
            std::ofstream file;
            std::mutex mutex; // Add mutex for thread safety
            ManifestState(std::string path) : file(path, std::ios::app) {}
        };
        auto manifest = std::make_shared<ManifestState>(base_dir + "manifest.csv");
        
        // Sensor Spawn Helper
        auto spawn_sensor = [&](const std::string& type_id, const std::string& name, auto callback) {
            auto bp_ptr = library->Find(type_id);
            if (!bp_ptr) throw std::runtime_error("Blueprint not found: " + type_id);
            
            ca::ActorBlueprint bp = *bp_ptr;
             if (bp.ContainsAttribute("image_size_x")) {
                 bp.SetAttribute("image_size_x", "800");
                 bp.SetAttribute("image_size_y", "600");
             }
             if (bp.ContainsAttribute("sensor_tick")) {
                 bp.SetAttribute("sensor_tick", "0.1"); // 10Hz
             }

            cg::Transform tf(cg::Location(1.5f, 0.0f, 1.2f));
            auto sensor_actor = world.SpawnActor(bp, tf, actor.get());
            auto sensor = boost::static_pointer_cast<cc::Sensor>(sensor_actor);
            
            // Listen with AsyncWriter push
            sensor->Listen([&writer, name, base_dir, manifest, callback, sensors_list=sensors](auto data) {
                if (!data) return;
                
                // Capture data (shared_ptr) and metadata by value
                writer.Push([=]() {
                     // Executed in worker thread
                    std::stringstream ss_id;
                    ss_id << std::setw(6) << std::setfill('0') << data->GetFrame();
                    
                    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                    std::stringstream ss_time;
                    ss_time << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");
                    
                    std::string base_id = ss_id.str() + "_" + ss_time.str();
                    std::string filename = name + "_" + base_id;
                    std::string full_path = base_dir + name + "/" + filename;

                    // Execute I/O callback
                    full_path = callback(data, full_path);

                    // Update Manifest (Thread-safe with mutex)
                    {
                        std::lock_guard<std::mutex> lock(manifest->mutex);
                        auto& entry = manifest->buffer[data->GetFrame()];
                        entry.paths[name] = full_path;

                        if (entry.paths.size() == 6) { // Hardcoded 6 sensors
                            manifest->file << base_id;
                            for (const auto& s : sensors_list) manifest->file << "," << entry.paths[s];
                            manifest->file << "\n" << std::flush;
                            manifest->buffer.erase(data->GetFrame());
                        }
                    }
                });
            });
            
            return sensor; 
        };

        std::vector<boost::shared_ptr<cc::Sensor>> active_sensors;
        std::vector<boost::shared_ptr<cc::Actor>> actors_to_destroy;

        // 1. RGB
        active_sensors.push_back(spawn_sensor("sensor.camera.rgb", "rgb", [](auto data, auto path) {
            auto image = boost::static_pointer_cast<csd::Image>(data);
            path += ".ppm";
            std::ofstream file(path, std::ios::binary);
            file << "P6\n" << image->GetWidth() << " " << image->GetHeight() << "\n255\n";
            auto buffer = image->begin();
            // Use write for speed
             for (auto& pixel : *image) {
                char rgb[3] = { (char)pixel.r, (char)pixel.g, (char)pixel.b };
                file.write(rgb, 3);
            }
            return path;
        }));

        // 2. Depth
        active_sensors.push_back(spawn_sensor("sensor.camera.depth", "depth", [](auto data, auto path) {
            auto image = boost::static_pointer_cast<csd::Image>(data);
            path += ".ppm";
            std::ofstream file(path, std::ios::binary);
            file << "P6\n" << image->GetWidth() << " " << image->GetHeight() << "\n255\n";
             for (auto& pixel : *image) {
                char rgb[3] = { (char)pixel.r, (char)pixel.g, (char)pixel.b };
                file.write(rgb, 3);
            }
            return path;
        }));

        // 3. Semantic
        active_sensors.push_back(spawn_sensor("sensor.camera.semantic_segmentation", "semantic", [](auto data, auto path) {
            auto image = boost::static_pointer_cast<csd::Image>(data);
            path += ".ppm";
            std::ofstream file(path, std::ios::binary);
            file << "P6\n" << image->GetWidth() << " " << image->GetHeight() << "\n255\n";
             for (auto& pixel : *image) {
                auto color = carla::image::CityScapesPalette::GetColor(pixel.r);
                char rgb[3] = { (char)color[0], (char)color[1], (char)color[2] };
                file.write(rgb, 3);
            }
            return path;
        }));

        // 4. Instance
        active_sensors.push_back(spawn_sensor("sensor.camera.instance_segmentation", "instance", [](auto data, auto path) {
            auto image = boost::static_pointer_cast<csd::Image>(data);
            path += ".ppm";
            std::ofstream file(path, std::ios::binary);
            file << "P6\n" << image->GetWidth() << " " << image->GetHeight() << "\n255\n";
             for (auto& pixel : *image) {
                char rgb[3] = { (char)pixel.r, (char)pixel.g, (char)pixel.b };
                file.write(rgb, 3);
            }
            return path;
        }));

        // 5. Optical Flow
        active_sensors.push_back(spawn_sensor("sensor.camera.optical_flow", "optflow", [](auto data, auto path) {
            auto image = boost::static_pointer_cast<csd::OpticalFlowImage>(data);
            path += ".ppm";
            std::ofstream file(path, std::ios::binary);
            file << "P6\n" << image->GetWidth() << " " << image->GetHeight() << "\n255\n";
            for (auto& pixel : *image) {
                float vx = pixel.x; float vy = pixel.y; 
                float magnitude = std::sqrt(vx*vx + vy*vy);
                float angle = std::atan2(vy, vx); 
                float hue = (angle + M_PI) * 180.0f / M_PI;
                float v = std::min(1.0f, std::log(1.0f + magnitude * 10.0f)); 
                float s = 1.0f;
                float c = v * s;
                float x = c * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
                float m = v - c;
                float r_prime = 0, g_prime = 0, b_prime = 0;
                if (0 <= hue && hue < 60) { r_prime = c; g_prime = x; b_prime = 0; }
                else if (60 <= hue && hue < 120) { r_prime = x; g_prime = c; b_prime = 0; }
                else if (120 <= hue && hue < 180) { r_prime = 0; g_prime = c; b_prime = x; }
                else if (180 <= hue && hue < 240) { r_prime = 0; g_prime = x; b_prime = c; }
                else if (240 <= hue && hue < 300) { r_prime = x; g_prime = 0; b_prime = c; }
                else if (300 <= hue && hue < 360) { r_prime = c; g_prime = 0; b_prime = x; }
                char rgb[3] = { (char)((r_prime + m) * 255), (char)((g_prime + m) * 255), (char)((b_prime + m) * 255) };
                file.write(rgb, 3);
            }
            return path;
        }));

        // 6. DVS
        active_sensors.push_back(spawn_sensor("sensor.camera.dvs", "dvs", [](auto data, auto path) {
            auto events = boost::static_pointer_cast<csd::DVSEventArray>(data);
            path += ".jsonl";
            std::ofstream file(path);
            for (const auto& e : *events) {
                file << "{\"t\":" << e.t << ",\"x\":" << e.x << ",\"y\":" << e.y << ",\"pol\":" << e.pol << "}\n";
            }
            return path;
        }));

        std::cout << "  - Dataset Recording: ON (6 Modalities, Async) -> " << base_dir << std::endl;

        for (auto& s : active_sensors) {
            actors_to_destroy.push_back(boost::static_pointer_cast<cc::Actor>(s));
        }

        class ListDestroyer {
            std::vector<boost::shared_ptr<cc::Actor>> list;
        public:
            ListDestroyer(std::vector<boost::shared_ptr<cc::Actor>> l) : list(l) {}
            ~ListDestroyer() { for(auto& a : list) if(a) a->Destroy(); }
        } sensor_destroyer(actors_to_destroy);

        auto spectator = world.GetSpectator();
        auto rgb_camera = active_sensors[0];
        auto callback_id = world.OnTick([spectator, rgb_camera](const auto&) {
            if (spectator && rgb_camera) spectator->SetTransform(rgb_camera->GetTransform());
        });

        std::cout << "\nPress ENTER to destroy the actor and exit..." << std::endl;
        std::cin.get();

        world.RemoveOnTick(callback_id);

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    // writer destroyed here, flushing queue
    std::cout << "Flushing write queue and exiting..." << std::endl;
    return 0;
}
