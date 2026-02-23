#include <carla/client/Client.h>
#include <carla/client/World.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char **argv) {
  std::string host = "127.0.0.1";
  uint16_t port = 2000;
  int duration = 30; // seconds

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--host") {
      if (i + 1 < argc)
        host = argv[++i];
    } else if (arg == "--port") {
      if (i + 1 < argc)
        port = std::stoi(argv[++i]);
    } else if (arg == "--duration") {
      if (i + 1 < argc)
        duration = std::stoi(argv[++i]);
    }
  }

  try {
    std::cout << "Profile Client Stub connecting to " << host << ":" << port
              << "..." << std::endl;
    auto client = carla::client::Client(host, port);
    client.SetTimeout(std::chrono::seconds(10));
    auto world = client.GetWorld();
    std::cout << "Connected to CARLA. Map: " << world.GetMap()->GetName()
              << std::endl;
    std::cout << "Simulating workload for " << duration << " seconds..."
              << std::endl;

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - start)
               .count() < duration) {
      // Do some "work" (e.g. just querying the world frequently to generate
      // some traffic/CPU load)
      world.GetSnapshot();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Workload complete." << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
