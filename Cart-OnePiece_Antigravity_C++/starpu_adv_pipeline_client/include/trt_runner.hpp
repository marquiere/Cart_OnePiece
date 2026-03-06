#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declare TensorRT types to avoid polluting headers
namespace nvinfer1 {
class IRuntime;
class ICudaEngine;
class IExecutionContext;
} // namespace nvinfer1

// Opaque struct for CUDA stream forward declaration
typedef struct CUstream_st *cudaStream_t;

class TrtRunner {
public:
  TrtRunner();
  ~TrtRunner();

  // Disable copy/move semantics for safety
  TrtRunner(const TrtRunner &) = delete;
  TrtRunner &operator=(const TrtRunner &) = delete;
  TrtRunner(TrtRunner &&) = delete;
  TrtRunner &operator=(TrtRunner &&) = delete;

  // Load the serialized engine file from disk
  bool LoadEngine(const std::string &engine_path);

  // Initialize execution context and CUDA resources
  bool Init();

  // Support dynamic dimensions if the engine requires it
  bool SetInputShapeIfDynamic(int w, int h);

  // Run synchronous/asynchronous inference. If stream_override is provided,
  // it executes fully async on that stream without host blocking.
  bool Infer(const void *host_input, size_t host_input_bytes, void *host_output,
             size_t host_output_bytes, cudaStream_t stream_override = nullptr);

  // Print out the loaded engine's binding metadata (Inputs, Outputs, DTypes,
  // Dims)
  void PrintBindings() const;

  // Explicitly release GPU assets
  void Shutdown();

  size_t GetInputBytes() const { return input_bytes_; }
  size_t GetOutputBytes() const { return output_bytes_; }

private:
  class Logger; // Internal TRT logger

  std::unique_ptr<Logger> logger_;
  nvinfer1::IRuntime *runtime_ = nullptr;
  nvinfer1::ICudaEngine *engine_ = nullptr;
  nvinfer1::IExecutionContext *context_ = nullptr;

  cudaStream_t stream_ = nullptr;

  void *d_in_ = nullptr;
  void *d_out_ = nullptr;

  int input_index_ = -1;
  int output_index_ = -1;

  size_t input_bytes_ = 0;
  size_t output_bytes_ = 0;

  bool is_dynamic_ = false;

  std::mutex infer_mutex_;

  // Internal helper for CUDA errors
  bool CheckCudaError(int code, const char *file, int line) const;
};
