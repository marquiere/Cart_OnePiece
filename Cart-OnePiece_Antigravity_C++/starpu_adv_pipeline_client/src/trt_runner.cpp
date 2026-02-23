#include "trt_runner.hpp"

#include <NvInfer.h>
#include <cuda_runtime_api.h>
#include <fstream>
#include <iostream>
#include <numeric>

#define CHECK_CUDA(call) CheckCudaError((call), __FILE__, __LINE__)

class TrtRunner::Logger : public nvinfer1::ILogger {
public:
  void log(Severity severity, const char *msg) noexcept override {
    if (severity <= Severity::kWARNING) {
      std::cerr << "[TRT " << (int)severity << "] " << msg << std::endl;
    }
  }
};

static size_t GetDataTypeSize(nvinfer1::DataType dtype) {
  switch (dtype) {
  case nvinfer1::DataType::kFLOAT:
    return 4;
  case nvinfer1::DataType::kHALF:
    return 2;
  case nvinfer1::DataType::kINT8:
    return 1;
  case nvinfer1::DataType::kINT32:
    return 4;
  case nvinfer1::DataType::kBOOL:
    return 1;
  default:
    return 0;
  }
}

static std::string GetDataTypeName(nvinfer1::DataType dtype) {
  switch (dtype) {
  case nvinfer1::DataType::kFLOAT:
    return "FP32";
  case nvinfer1::DataType::kHALF:
    return "FP16";
  case nvinfer1::DataType::kINT8:
    return "INT8";
  case nvinfer1::DataType::kINT32:
    return "INT32";
  case nvinfer1::DataType::kBOOL:
    return "BOOL";
  default:
    return "UNKNOWN";
  }
}

static size_t GetVolume(const nvinfer1::Dims &dims) {
  if (dims.nbDims == 0)
    return 0;
  size_t vol = 1;
  for (int i = 0; i < dims.nbDims; ++i) {
    if (dims.d[i] < 0)
      return 0;
    vol *= dims.d[i];
  }
  return vol;
}

TrtRunner::TrtRunner() : logger_(new Logger()) {}

TrtRunner::~TrtRunner() { Shutdown(); }

bool TrtRunner::CheckCudaError(int code, const char *file, int line) const {
  if (code != cudaSuccess) {
    std::cerr << "CUDA Error: " << cudaGetErrorString((cudaError_t)code)
              << " at " << file << ":" << line << std::endl;
    return false;
  }
  return true;
}

bool TrtRunner::LoadEngine(const std::string &engine_path) {
  std::ifstream file(engine_path, std::ios::binary);
  if (!file.good()) {
    std::cerr << "ERROR: Failed to open engine file: " << engine_path
              << std::endl;
    return false;
  }

  file.seekg(0, file.end);
  size_t size = file.tellg();
  file.seekg(0, file.beg);

  std::vector<char> trtModelStream(size);
  file.read(trtModelStream.data(), size);
  file.close();

  runtime_ = nvinfer1::createInferRuntime(*logger_);
  if (!runtime_) {
    std::cerr << "ERROR: Failed to create TRT runtime" << std::endl;
    return false;
  }

  engine_ = runtime_->deserializeCudaEngine(trtModelStream.data(), size);
  if (!engine_) {
    std::cerr << "ERROR: Failed to deserialize engine" << std::endl;
    return false;
  }

  return true;
}

bool TrtRunner::Init() {
  if (!engine_)
    return false;

  context_ = engine_->createExecutionContext();
  if (!context_)
    return false;

  if (!CHECK_CUDA(cudaStreamCreate(&stream_)))
    return false;

  int nbTensors = engine_->getNbIOTensors();

  std::cout << "--- TRT Init Parsing IOTensors ---" << std::endl;
  for (int i = 0; i < nbTensors; ++i) {
    const char *name = engine_->getIOTensorName(i);
    nvinfer1::TensorIOMode mode = engine_->getTensorIOMode(name);
    nvinfer1::Dims dims = engine_->getTensorShape(name);

    for (int j = 0; j < dims.nbDims; j++) {
      if (dims.d[j] == -1)
        is_dynamic_ = true;
    }

    if (mode == nvinfer1::TensorIOMode::kINPUT) {
      input_index_ = i;
      if (!is_dynamic_) {
        input_bytes_ =
            GetVolume(dims) * GetDataTypeSize(engine_->getTensorDataType(name));
        if (!CHECK_CUDA(cudaMalloc(&d_in_, input_bytes_)))
          return false;
      }
    } else if (mode == nvinfer1::TensorIOMode::kOUTPUT) {
      output_index_ = i;
      if (!is_dynamic_) {
        output_bytes_ =
            GetVolume(dims) * GetDataTypeSize(engine_->getTensorDataType(name));
        if (!CHECK_CUDA(cudaMalloc(&d_out_, output_bytes_)))
          return false;
      }
    }
  }

  if (input_index_ < 0 || output_index_ < 0) {
    std::cerr << "ERROR: Missing input or output tensor." << std::endl;
    return false;
  }

  return true;
}

void TrtRunner::PrintBindings() const {
  if (!engine_)
    return;
  int nbTensors = engine_->getNbIOTensors();
  std::cout << "=== TensorRT 10.x Engine Tensors (" << nbTensors
            << ") ===" << std::endl;

  for (int i = 0; i < nbTensors; ++i) {
    const char *name = engine_->getIOTensorName(i);
    nvinfer1::TensorIOMode mode = engine_->getTensorIOMode(name);
    nvinfer1::DataType dtype = engine_->getTensorDataType(name);
    nvinfer1::Dims dims = engine_->getTensorShape(name);

    size_t bytes = 0;
    if (context_) {
      nvinfer1::Dims cdims = context_->getTensorShape(name);
      bytes = GetVolume(cdims) * GetDataTypeSize(dtype);
    }

    std::cout << "[" << i << "] "
              << (mode == nvinfer1::TensorIOMode::kINPUT ? "IN " : "OUT")
              << " | Name: " << name << " | " << GetDataTypeName(dtype)
              << " | Dims: [";
    for (int j = 0; j < dims.nbDims; ++j) {
      std::cout << dims.d[j] << (j < dims.nbDims - 1 ? "x" : "");
    }
    std::cout << "] | Bytes: " << bytes << std::endl;
  }
}

bool TrtRunner::SetInputShapeIfDynamic(int w, int h) {
  if (!is_dynamic_)
    return true;
  if (!context_)
    return false;

  const char *in_name = engine_->getIOTensorName(input_index_);
  nvinfer1::Dims in_dims = engine_->getTensorShape(in_name);

  nvinfer1::Dims new_dims = in_dims;
  if (new_dims.nbDims == 4) {
    if (new_dims.d[0] == -1)
      new_dims.d[0] = 1;
    if (new_dims.d[2] == -1)
      new_dims.d[2] = h;
    if (new_dims.d[3] == -1)
      new_dims.d[3] = w;
  }

  if (!context_->setInputShape(in_name, new_dims)) {
    std::cerr << "ERROR: Failed to set input shape for " << in_name
              << std::endl;
    return false;
  }

  // Allocate
  input_bytes_ = GetVolume(context_->getTensorShape(in_name)) *
                 GetDataTypeSize(engine_->getTensorDataType(in_name));
  if (d_in_)
    cudaFree(d_in_);
  if (!CHECK_CUDA(cudaMalloc(&d_in_, input_bytes_)))
    return false;

  const char *out_name = engine_->getIOTensorName(output_index_);
  output_bytes_ = GetVolume(context_->getTensorShape(out_name)) *
                  GetDataTypeSize(engine_->getTensorDataType(out_name));
  if (d_out_)
    cudaFree(d_out_);
  if (!CHECK_CUDA(cudaMalloc(&d_out_, output_bytes_)))
    return false;

  return true;
}

bool TrtRunner::Infer(const void *host_input, size_t host_input_bytes,
                      void *host_output, size_t host_output_bytes) {
  if (!host_input || !host_output || !d_in_ || !d_out_)
    return false;
  if (host_input_bytes != input_bytes_ || host_output_bytes != output_bytes_)
    return false;

  if (!CHECK_CUDA(cudaMemcpyAsync(d_in_, host_input, input_bytes_,
                                  cudaMemcpyHostToDevice, stream_)))
    return false;

  const char *in_name = engine_->getIOTensorName(input_index_);
  const char *out_name = engine_->getIOTensorName(output_index_);

  if (!context_->setTensorAddress(in_name, d_in_))
    return false;
  if (!context_->setTensorAddress(out_name, d_out_))
    return false;

  if (!context_->enqueueV3(stream_)) {
    std::cerr << "ERROR: TensorRT enqueueV3 failed" << std::endl;
    return false;
  }

  if (!CHECK_CUDA(cudaMemcpyAsync(host_output, d_out_, output_bytes_,
                                  cudaMemcpyDeviceToHost, stream_)))
    return false;
  if (!CHECK_CUDA(cudaStreamSynchronize(stream_)))
    return false;

  return true;
}

void TrtRunner::Shutdown() {
  if (d_out_) {
    cudaFree(d_out_);
    d_out_ = nullptr;
  }
  if (d_in_) {
    cudaFree(d_in_);
    d_in_ = nullptr;
  }
  if (stream_) {
    cudaStreamDestroy(stream_);
    stream_ = nullptr;
  }

  // In TensorRT 10, delete is used instead of destroy()
  if (context_) {
    delete context_;
    context_ = nullptr;
  }
  if (engine_) {
    delete engine_;
    engine_ = nullptr;
  }
  if (runtime_) {
    delete runtime_;
    runtime_ = nullptr;
  }
}
