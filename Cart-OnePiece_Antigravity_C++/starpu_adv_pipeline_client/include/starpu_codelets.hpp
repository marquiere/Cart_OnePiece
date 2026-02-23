#pragma once

#include "pipeline_types.hpp"
#include "preprocess.hpp"
#include <memory>
#include <starpu.h>

// Define structures to pass to starpu task cl_arg

struct PreprocArgs {
  uint64_t frame_id;
  int in_w;
  int in_h;
  PreprocessConfig cfg;
};

struct InferArgs {
  uint64_t frame_id;
  int out_c;
  int out_h;
  int out_w;
  bool is_logits;
  class TrtRunner *runner; // Pointer to the shared runner
};

struct PostArgs {
  uint64_t frame_id;
  int out_c;
  int out_h;
  int out_w;
  bool is_logits;
};

// Expose the codelets
extern struct starpu_codelet cl_preproc;
extern struct starpu_codelet cl_infer_trt;
extern struct starpu_codelet cl_post;
