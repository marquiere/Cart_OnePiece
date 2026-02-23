#include "trt_runner.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  std::string engine_path = "";
  int iters = 100;
  int warmup = 10;
  std::string input_fill = "random";
  int dump_bindings = 1;

  // Dynamic shapes
  int in_w = -1;
  int in_h = -1;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--engine") {
      if (i + 1 < argc)
        engine_path = argv[++i];
    } else if (arg == "--iters") {
      if (i + 1 < argc)
        iters = std::stoi(argv[++i]);
    } else if (arg == "--warmup") {
      if (i + 1 < argc)
        warmup = std::stoi(argv[++i]);
    } else if (arg == "--input_fill") {
      if (i + 1 < argc)
        input_fill = argv[++i];
    } else if (arg == "--dump_bindings") {
      if (i + 1 < argc)
        dump_bindings = std::stoi(argv[++i]);
    } else if (arg == "--in_w") {
      if (i + 1 < argc)
        in_w = std::stoi(argv[++i]);
    } else if (arg == "--in_h") {
      if (i + 1 < argc)
        in_h = std::stoi(argv[++i]);
    }
  }

  if (engine_path.empty()) {
    std::cerr << "ERROR: --engine <path> is strictly required." << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Loading TensorRT Engine: " << engine_path << std::endl;

  TrtRunner runner;
  if (!runner.LoadEngine(engine_path))
    return EXIT_FAILURE;
  if (!runner.Init())
    return EXIT_FAILURE;

  if (dump_bindings) {
    runner.PrintBindings();
  }

  if (in_w > 0 && in_h > 0) {
    if (!runner.SetInputShapeIfDynamic(in_w, in_h)) {
      std::cerr << "ERROR: Failed to establish dynamic shapes" << std::endl;
      return EXIT_FAILURE;
    }
  }

  size_t in_bytes = runner.GetInputBytes();
  size_t out_bytes = runner.GetOutputBytes();
  if (in_bytes == 0 || out_bytes == 0) {
    std::cerr
        << "ERROR: Invalid buffer sizes. Likely missed dynamic shape bindings."
        << std::endl;
    return EXIT_FAILURE;
  }

  std::vector<uint8_t> h_in(in_bytes, 0);
  std::vector<uint8_t> h_out(out_bytes, 0);

  // Provide generic float parsing input fill
  if (input_fill == "random") {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    float *f_ptr = reinterpret_cast<float *>(h_in.data());
    size_t f_count = in_bytes / sizeof(float);
    for (size_t i = 0; i < f_count; ++i) {
      f_ptr[i] = dis(gen);
    }
  } // else zeros remain zeros.

  std::cout << "--- Executing " << warmup << " Warmups ---" << std::endl;
  for (int i = 0; i < warmup; ++i) {
    if (!runner.Infer(h_in.data(), in_bytes, h_out.data(), out_bytes)) {
      std::cerr << "ERROR: Inference failed during warmup" << std::endl;
      return EXIT_FAILURE;
    }
  }

  std::cout << "--- Executing " << iters << " Inferences ---" << std::endl;
  std::vector<double> latencies_ms;
  latencies_ms.reserve(iters);

  auto t_start_total = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iters; ++i) {
    auto t_start = std::chrono::high_resolution_clock::now();
    if (!runner.Infer(h_in.data(), in_bytes, h_out.data(), out_bytes)) {
      std::cerr << "ERROR: Inference failed during iterations" << std::endl;
      return EXIT_FAILURE;
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    double ms =
        std::chrono::duration<double, std::milli>(t_end - t_start).count();
    latencies_ms.push_back(ms);
  }
  auto t_end_total = std::chrono::high_resolution_clock::now();
  double total_ms =
      std::chrono::duration<double, std::milli>(t_end_total - t_start_total)
          .count();

  std::sort(latencies_ms.begin(), latencies_ms.end());
  double min_ms = latencies_ms.front();
  double p95_ms = latencies_ms[static_cast<size_t>(iters * 0.95)];
  double avg_ms =
      std::accumulate(latencies_ms.begin(), latencies_ms.end(), 0.0) / iters;

  std::cout << "\n===== TRT Profiling Results =====" << std::endl;
  std::cout << "Min Latency : " << min_ms << " ms" << std::endl;
  std::cout << "Avg Latency : " << avg_ms << " ms" << std::endl;
  std::cout << "p95 Latency : " << p95_ms << " ms" << std::endl;
  std::cout << "Total Time  : " << total_ms << " ms" << std::endl;

  // Checksum
  double checksum = 0.0;
  float *f_out = reinterpret_cast<float *>(h_out.data());
  size_t f_out_count = out_bytes / sizeof(float);
  size_t view_limit = std::min((size_t)1024, f_out_count);

  for (size_t i = 0; i < view_limit; ++i) {
    checksum += static_cast<double>(f_out[i]);
  }
  std::cout << "\nChecksum (first " << view_limit << " floats): " << checksum
            << std::endl;

  // Optional Checksum sanity bound proof (must be somewhat non-trivial unless
  // model puts out all zeros)
  if (std::isnan(checksum) || std::isinf(checksum)) {
    std::cerr << "WARNING: Checksum is NaN or Infinity!" << std::endl;
  }

  std::cout << "Clean Exit." << std::endl;
  return EXIT_SUCCESS;
}
