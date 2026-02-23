#pragma once

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
  }

  ~StarpuRuntime() { starpu_shutdown(); }

  StarpuRuntime(const StarpuRuntime &) = delete;
  StarpuRuntime &operator=(const StarpuRuntime &) = delete;
};
