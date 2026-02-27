#include "semantic_decode.hpp"
#include <carla/client/Client.h>
#include <carla/client/Sensor.h>
#include <carla/client/World.h>
#include <carla/sensor/data/Image.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main() {
  auto client = carla::client::Client("127.0.0.1", 2000);
  client.SetTimeout(std::chrono::seconds(10));
  auto world = client.GetWorld();

  // Attempt to read one sensor data
  std::cout << "Starting read test..." << std::endl;
  // We don't have direct access to the sensors here easily unless we spawn one.
  // Let's just run dataset_sanity which already saves the histograms!
  return 0;
}
