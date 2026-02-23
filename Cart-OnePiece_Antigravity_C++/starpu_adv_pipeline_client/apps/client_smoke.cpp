#include <exception>
#include <iostream>
#include <string>

// CARLA includes
#include <carla/client/Client.h>
#include <carla/client/Map.h>
#include <carla/client/World.h>

#define VERSION "1.0.0"

void print_help() {
  std::cout << "Usage: client_smoke [OPTIONS]\n"
            << "Options:\n"
            << "  --help     Show this help message and exit\n"
            << "  --version  Show version information and exit\n"
            << "  --host     CARLA server hostIP (default 127.0.0.1)\n"
            << "  --port     CARLA server port (default 2000)\n"
            << "\nThis program verifies connection to a local CARLA server.\n";
}

int main(int argc, char **argv) {
  std::string host = "127.0.0.1";
  uint16_t port = 2000;

  // Parse arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      print_help();
      return EXIT_SUCCESS;
    } else if (arg == "--version" || arg == "-v") {
      std::cout << "client_smoke version " << VERSION << std::endl;
      return EXIT_SUCCESS;
    } else if (arg == "--host") {
      if (i + 1 < argc)
        host = argv[++i];
    } else if (arg == "--port") {
      if (i + 1 < argc)
        port = std::stoi(argv[++i]);
    }
  }

  std::cout << "Attempting to connect to CARLA at " << host << ":" << port
            << "..." << std::endl;

  try {
    // Connect to the client
    auto client = carla::client::Client(host, port);
    client.SetTimeout(std::chrono::seconds(10));

    // Get server version
    std::string server_version = client.GetServerVersion();
    std::cout << "Server Version: " << server_version << std::endl;

    // Get the world and map to prove connection is fully active
    auto world = client.GetWorld();
    auto map = world.GetMap();
    std::string map_name = map->GetName();

    std::cout << "Map Name: " << map_name << std::endl;
    std::cout << "CONNECTED OK" << std::endl;

  } catch (const std::exception &e) {
    std::cerr
        << "\nERROR: Failed to connect or retrieve data from CARLA server."
        << std::endl;
    std::cerr << "Exception Output: " << e.what() << std::endl;
    std::cerr
        << "\n--- Troubleshooting Hints ---\n"
        << "1. Is the CARLA server running? Check 'tools/run_server.sh'.\n"
        << "2. Are you using the correct port? Default is 2000.\n"
        << "3. If the server just started, it might need a few more seconds to "
           "load the map.\n"
        << "4. Check the server log at runs/<timestamp>/server.log for "
           "crashes.\n"
        << "----------------------------\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
