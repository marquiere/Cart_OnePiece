#pragma once

#include "starpu_codelets.hpp"
#include <iostream>
#include <starpu.h>
#include <starpu_profiling.h>
#include <stdexcept>

class StarpuRuntime {
public:
  StarpuRuntime(int cpu_workers) {
    int ret;
    struct starpu_conf conf;
    starpu_conf_init(&conf);

    conf.ncuda = 1; // 1 CUDA worker enabled if available
    conf.ncpus = cpu_workers;

    ret = starpu_init(&conf);
    if (ret != 0) {
      throw std::runtime_error("StarPU initialization failed.");
    }

    // Default: do not enable profiling globally to avoid FxT tracer bugs
    // starpu_profiling_status_set(STARPU_PROFILING_ENABLE);

    const char *env_sched = getenv("STARPU_SCHED");
    std::cout << "--- StarPU Runtime Initialization ---\n";
    std::cout << "[StarPU] Scheduler: " << (env_sched ? env_sched : "default")
              << "\n";
    std::cout << "[StarPU] Profiling: "
              << (starpu_profiling_status_get() ? "ENABLED" : "DISABLED")
              << "\n";

    if (cl_preproc.model) {
      std::cout << "[StarPU] PerfModel attached to preproc  : "
                << cl_preproc.model->symbol << "\n";
    }
    if (cl_infer_trt.model) {
      std::cout << "[StarPU] PerfModel attached to infer_trt: "
                << cl_infer_trt.model->symbol << "\n";
    }
    if (cl_post.model) {
      std::cout << "[StarPU] PerfModel attached to post       : "
                << cl_post.model->symbol << "\n";
    }
    std::cout << "-------------------------------------\n";
  }

  ~StarpuRuntime() { starpu_shutdown(); }

  StarpuRuntime(const StarpuRuntime &) = delete;
  StarpuRuntime &operator=(const StarpuRuntime &) = delete;
};
