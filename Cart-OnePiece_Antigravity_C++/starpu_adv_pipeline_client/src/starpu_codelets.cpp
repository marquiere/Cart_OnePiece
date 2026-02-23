#include "starpu_codelets.hpp"
#include "postprocess.hpp"
#include "preprocess.hpp"
#include "trt_runner.hpp"
#include <iostream>

// --- Preprocess Codelet ---

void cpu_preproc_func(void *buffers[], void *cl_arg) {
  auto args = static_cast<PreprocArgs *>(cl_arg);

  if (!args || args->cfg.out_w <= 0 || args->cfg.out_h <= 0 ||
      args->in_w <= 0 || args->in_h <= 0) {
    std::cerr << "Preproc codelet received invalid args! in_w:"
              << (args ? args->in_w : -1)
              << " in_h:" << (args ? args->in_h : -1) << std::endl;
    return;
  }

  uint8_t *in_bgra = (uint8_t *)STARPU_VECTOR_GET_PTR(buffers[0]);
  float *out_nchw = (float *)STARPU_VECTOR_GET_PTR(buffers[1]);

  PreprocessBGRAtoNCHW_F32(in_bgra, args->in_w, args->in_h, args->cfg,
                           out_nchw);
}

// --- Inference Codelet ---

void cuda_infer_func(void *buffers[], void *cl_arg) {
  auto args = static_cast<InferArgs *>(cl_arg);

  if (!args || args->out_w <= 0 || args->out_h <= 0) {
    std::cerr << "Infer codelet received invalid args!" << std::endl;
    return;
  }

  float *in_nchw = (float *)STARPU_VECTOR_GET_PTR(buffers[0]);
  void *out_raw = (void *)STARPU_VECTOR_GET_PTR(buffers[1]);

  size_t in_bytes =
      STARPU_VECTOR_GET_NX(buffers[0]) * STARPU_VECTOR_GET_ELEMSIZE(buffers[0]);
  size_t out_bytes =
      STARPU_VECTOR_GET_NX(buffers[1]) * STARPU_VECTOR_GET_ELEMSIZE(buffers[1]);

  if (args->runner) {
    args->runner->Infer(in_nchw, in_bytes, out_raw, out_bytes);
  } else {
    std::cerr << "TRT Runner NULL inside CUDA task!" << std::endl;
  }
}

// Allow CPU fallback if CUDA isn't strictly requested by StarPU config,
// though we usually want to force it to CUDA.
void cpu_infer_func(void *buffers[], void *cl_arg) {
  // Exactly the same, TrtRunner handles H2D -> Compute -> D2H.
  // StarPU merely invokes it from a CPU worker thread if pushed there.
  cuda_infer_func(buffers, cl_arg);
}

// --- Postprocess Codelet ---

void cpu_post_func(void *buffers[], void *cl_arg) {
  auto args = static_cast<PostArgs *>(cl_arg);

  if (!args || args->out_w <= 0 || args->out_h <= 0) {
    std::cerr << "Post codelet received invalid args!" << std::endl;
    return;
  }

  void *in_raw = (void *)STARPU_VECTOR_GET_PTR(buffers[0]);
  uint8_t *out_labels = (uint8_t *)STARPU_VECTOR_GET_PTR(buffers[1]);

  // We assume out_labels is pre-sized to out_w * out_h
  std::vector<uint8_t> wrap_vec;

  if (args->is_logits) {
    const float *logits = static_cast<const float *>(in_raw);
    postprocess::ArgmaxLogitsToLabels(logits, args->out_c, args->out_h,
                                      args->out_w, wrap_vec);
  } else {
    const int32_t *direct = static_cast<const int32_t *>(in_raw);
    postprocess::DirectLabelsToUint8(direct, args->out_h, args->out_w,
                                     wrap_vec);
  }

  // Copy to the StarPU-managed output buffer
  if (!wrap_vec.empty()) {
    std::copy(wrap_vec.begin(), wrap_vec.end(), out_labels);
  }
}

// Expose the global StarPU codelets

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc99-designator"

struct starpu_codelet cl_preproc = {.cpu_funcs = {cpu_preproc_func},
                                    .cpu_funcs_name = {"cpu_preproc"},
                                    .nbuffers = 2,
                                    .modes = {STARPU_R, STARPU_W},
                                    .model = nullptr,
                                    .name = "cl_preproc"};

struct starpu_codelet cl_infer_trt = {.cpu_funcs = {cpu_infer_func},
                                      .cuda_funcs = {cuda_infer_func},
                                      .cpu_funcs_name = {"cpu_infer_trt"},
                                      .nbuffers = 2,
                                      .modes = {STARPU_R, STARPU_W},
                                      .model = nullptr,
                                      .name = "cl_infer_trt"};

struct starpu_codelet cl_post = {.cpu_funcs = {cpu_post_func},
                                 .cpu_funcs_name = {"cpu_post"},
                                 .nbuffers = 2,
                                 .modes = {STARPU_R, STARPU_W},
                                 .model = nullptr,
                                 .name = "cl_post"};

#pragma GCC diagnostic pop
