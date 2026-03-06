#include <iostream>
#include <fstream>
#include <vector>
#include <NvInfer.h>
#include <cuda_runtime_api.h>

using namespace nvinfer1;

class Logger : public ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity != Severity::kINFO) { std::cout << msg << std::endl; }
    }
} gLogger;

int main(int argc, char** argv) {
    if(argc != 2) return 1;
    std::ifstream file(argv[1], std::ios::binary);
    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0, file.beg);
    std::vector<char> engineData(size);
    file.read(engineData.data(), size);
    
    IRuntime* runtime = createInferRuntime(gLogger);
    ICudaEngine* engine = runtime->deserializeCudaEngine(engineData.data(), size);
    
    int nbBindings = engine->getNbIOTensors();
    std::cout << "Bindings: " << nbBindings << std::endl;
    for (int i = 0; i < nbBindings; i++) {
        std::string name = engine->getIOTensorName(i);
        TensorIOMode mode = engine->getTensorIOMode(name.c_str());
        Dims d = engine->getTensorShape(name.c_str());
        std::cout << "Tensor " << i << ": " << name << " -> ";
        for(int j=0; j<d.nbDims; j++) std::cout << d.d[j] << "x";
        std::cout << " Mode: " << (mode == TensorIOMode::kINPUT ? "IN" : "OUT") << std::endl;
    }
    return 0;
}
