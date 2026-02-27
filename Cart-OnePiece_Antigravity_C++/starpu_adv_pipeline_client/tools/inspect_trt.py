import torch
import tensorrt as trt
import pycuda.driver as cuda
import pycuda.autoinit
import numpy as np

logger = trt.Logger(trt.Logger.WARNING)
with open("models/dummy.engine", "rb") as f, trt.Runtime(logger) as runtime:
    engine = runtime.deserialize_cuda_engine(f.read())
    context = engine.create_execution_context()
    
    # Check shapes
    print("Binding 0 (input):", engine.get_binding_shape(0))
    print("Binding 1 (output):", engine.get_binding_shape(1))
    
    # We will pass a dummy smooth image
    inputs = np.ones((1, 3, 256, 512), dtype=np.float32)
    output = np.empty((1, 19, 256, 512), dtype=np.float32)
    
    d_input = cuda.mem_alloc(inputs.nbytes)
    d_output = cuda.mem_alloc(output.nbytes)
    
    cuda.memcpy_htod(d_input, inputs)
    context.execute_v2(bindings=[int(d_input), int(d_output)])
    cuda.memcpy_dtoh(output, d_output)

    # Do proper argmax
    argmax = np.argmax(output[0], axis=0) # Shape: 256, 512
    print("Argmax unique:", np.unique(argmax))
    
    print("First row of output[0, 0, 0, :10] = ", output[0, 0, 0, :10])
