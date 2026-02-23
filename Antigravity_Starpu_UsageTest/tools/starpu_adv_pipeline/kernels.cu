#include <starpu.h>

// Trivial kernel
__global__ void empty_kernel(void)
{
}

// Host wrapper
extern "C" void launch_empty_kernel(void)
{
    empty_kernel<<<1, 1>>>();
}

// ------------------------------------------------------------------
// Infer Kernel
// ------------------------------------------------------------------
__global__ void infer_kernel(float *valB, float *valC, unsigned n, unsigned iters)
{
    unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
    {
        float val = valB[i];
        for (unsigned k = 0; k < iters; k++)
        {
            // Simple dummy compute
            val = val * 1.0001f + 0.001f;
        }
        valC[i] = val;
    }
}

extern "C" void infer_cuda_func(void *buffers[], void *cl_arg)
{
    float *valB = (float *)STARPU_VECTOR_GET_PTR(buffers[0]);
    float *valC = (float *)STARPU_VECTOR_GET_PTR(buffers[1]);
    unsigned n = STARPU_VECTOR_GET_NX(buffers[0]);
    unsigned iters = 0;
    
    starpu_codelet_unpack_args(cl_arg, &iters);

    unsigned threads = 256;
    unsigned blocks = (n + threads - 1) / threads;

    infer_kernel<<<blocks, threads, 0, starpu_cuda_get_local_stream()>>>(valB, valC, n, iters);
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) fprintf(stderr, "CUDA Error: %s\n", cudaGetErrorString(err));
}
