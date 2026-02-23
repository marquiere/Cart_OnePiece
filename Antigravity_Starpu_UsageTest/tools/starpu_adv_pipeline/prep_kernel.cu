#include <starpu.h>
#include <cuda.h>
#include <cuda_runtime.h>

static __global__ void prep_cuda_kernel(float *tab, unsigned n)
{
    unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
    {
        tab[i] = tab[i] * 1.0001f + 0.01f;
    }
}

extern "C" void prep_cuda_func(void *buffers[], void *_args)
{
    float *tab = (float *)STARPU_VECTOR_GET_PTR(buffers[0]);
    unsigned n = STARPU_VECTOR_GET_NX(buffers[0]);

    unsigned threads = 256;
    unsigned blocks = (n + threads - 1) / threads;

    prep_cuda_kernel<<<blocks, threads, 0, starpu_cuda_get_local_stream()>>>(tab, n);
    
    // Check for errors? StarPU handles stream errors usually, 
    // but explicit check helps debugging.
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess)
    {
        fprintf(stderr, "CUDA Error: %s\n", cudaGetErrorString(err));
    }
}
