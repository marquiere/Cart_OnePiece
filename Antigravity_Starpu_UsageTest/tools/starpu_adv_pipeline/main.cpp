#include <cstring>
#include <iostream>
#include <starpu.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// APEX Instrumentation
#ifdef ENABLE_APEX
#include <apex_api.hpp>
#define APEX_START(name) apex::profiler *prof_##name = apex::start(#name)
#define APEX_STOP(name) apex::stop(prof_##name)
#else
#define APEX_START(name)
#define APEX_STOP(name)
#endif

// Forward declare CUDA func (from kernels.cu)
extern "C" void infer_cuda_func(void *buffers[], void *cl_arg);

// CPU Codelets with Instrumentation
extern "C" void prep_cpu_func(void *buffers[], void *cl_arg) {
  APEX_START(prep_cpu);
  float *valA = (float *)STARPU_VECTOR_GET_PTR(buffers[0]);
  float *valB = (float *)STARPU_VECTOR_GET_PTR(buffers[1]);
  unsigned n = STARPU_VECTOR_GET_NX(buffers[0]);

  for (unsigned i = 0; i < n; i++)
    valB[i] = valA[i] + 1.00001f;
  APEX_STOP(prep_cpu);
}

extern "C" void post_cpu_func(void *buffers[], void *cl_arg) {
  APEX_START(post_cpu);
  float *valC = (float *)STARPU_VECTOR_GET_PTR(buffers[0]);
  float *valA = (float *)STARPU_VECTOR_GET_PTR(buffers[1]);
  unsigned n = STARPU_VECTOR_GET_NX(buffers[0]);

  for (unsigned i = 0; i < n; i++)
    valA[i] = valC[i] + 0.5f;
  APEX_STOP(post_cpu);
}

extern "C" void logger_cpu_func(void *buffers[], void *cl_arg) {
  APEX_START(logger_cpu);
  float *valA = (float *)STARPU_VECTOR_GET_PTR(buffers[0]);
  unsigned n = STARPU_VECTOR_GET_NX(buffers[0]);

  double sum = 0.0;
  for (unsigned i = 0; i < (n < 5 ? n : 5); i++)
    sum += valA[i];
  if (n > 5) {
    for (unsigned i = n - 5; i < n; i++)
      sum += valA[i];
  }
  APEX_STOP(logger_cpu);
}

// Wrapper for CUDA launcher to add APEX instrumentation
extern "C" void infer_cuda_wrapper(void *buffers[], void *cl_arg) {
  APEX_START(infer_cuda_launch);
  // Could add counter here: apex::sample_value("tensor_bytes",
  // STARPU_VECTOR_GET_NX(buffers[0]) * sizeof(float));
  infer_cuda_func(buffers, cl_arg);
  APEX_STOP(infer_cuda_launch);
}

// Codelets
struct starpu_codelet cl_prep = {.cpu_funcs = {prep_cpu_func},
                                 .cpu_funcs_name = {"prep_cpu_func"},
                                 .nbuffers = 2,
                                 .modes = {STARPU_R, STARPU_W},
                                 .name = "prep_cpu"};

struct starpu_codelet cl_infer = {
    .cuda_funcs = {infer_cuda_wrapper}, // Use wrapper!
    .cuda_flags = {STARPU_CUDA_ASYNC},
    .nbuffers = 2,
    .modes = {STARPU_R, STARPU_W},
    .name = "infer_cuda"};

struct starpu_codelet cl_post = {.cpu_funcs = {post_cpu_func},
                                 .cpu_funcs_name = {"post_cpu_func"},
                                 .nbuffers = 2,
                                 .modes = {STARPU_R, STARPU_W},
                                 .name = "post_cpu"};

struct starpu_codelet cl_logger = {.cpu_funcs = {logger_cpu_func},
                                   .cpu_funcs_name = {"logger_cpu_func"},
                                   .nbuffers = 1,
                                   .modes = {STARPU_R},
                                   .name = "logger_cpu"};

// Perfmodels
struct starpu_perfmodel model_prep = {.type = STARPU_HISTORY_BASED,
                                      .symbol = "adv_prep_cpu"};
struct starpu_perfmodel model_infer = {.type = STARPU_HISTORY_BASED,
                                       .symbol = "adv_infer_cuda"};
struct starpu_perfmodel model_post = {.type = STARPU_HISTORY_BASED,
                                      .symbol = "adv_post_cpu"};
struct starpu_perfmodel model_logger = {.type = STARPU_HISTORY_BASED,
                                        .symbol = "adv_logger_cpu"};

void parse_args(int argc, char **argv, int &frames, int &mb, bool &trace,
                std::string &trace_dir, int &infer_iters, std::string &sched,
                bool &print_stats, int &transfer_mb) {
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--frames" && i + 1 < argc)
      frames = std::atoi(argv[++i]);
    else if (std::string(argv[i]) == "--mb" && i + 1 < argc)
      mb = std::atoi(argv[++i]);
    else if (std::string(argv[i]) == "--trace")
      trace = true;
    else if (std::string(argv[i]) == "--trace-dir" && i + 1 < argc)
      trace_dir = argv[++i];
    else if (std::string(argv[i]) == "--infer-iters" && i + 1 < argc)
      infer_iters = std::atoi(argv[++i]);
    else if (std::string(argv[i]) == "--sched" && i + 1 < argc)
      sched = argv[++i];
    else if (std::string(argv[i]) == "--print-stats")
      print_stats = true;
    else if (std::string(argv[i]) == "--transfer-mb" && i + 1 < argc)
      transfer_mb = std::atoi(argv[++i]);
  }
}

int main(int argc, char **argv) {
  // Defaults
  int frames = 10;
  int mb = 1;
  int infer_iters = 1; // Default lightweight
  bool trace = false;
  std::string trace_dir = "/tmp/starpu_traces_adv";
  std::string sched_policy = "";
  bool print_stats = false;
  int transfer_mb = 0;

  parse_args(argc, argv, frames, mb, trace, trace_dir, infer_iters,
             sched_policy, print_stats, transfer_mb);

  // Set sched policy env var if requested (BEFORE init)
  if (!sched_policy.empty()) {
    setenv("STARPU_SCHED", sched_policy.c_str(), 1);
  }

  // Config
  struct starpu_conf conf;
  starpu_conf_init(&conf);

  // FxT Tracing Config
  if (trace) {
    // setenv("STARPU_FXT_PREFIX", trace_dir.c_str(), 1); // Respect env var
    // from script
    setenv("STARPU_FXT_TRACE", "1", 1);
    // setenv("STARPU_GENERATE_TRACE", "1", 1); // Disable auto-processing for
    // immutable raw
  }

  int ret = starpu_init(&conf);
  STARPU_CHECK_RETURN_VALUE(ret, "starpu_init");

  // Enable profiling (required for PAPI/Details)
  starpu_profiling_status_set(STARPU_PROFILING_ENABLE);

  // Attach models
  cl_prep.model = &model_prep;
  cl_infer.model = &model_infer;
  cl_post.model = &model_post;
  cl_logger.model = &model_logger;

  size_t vector_size = mb * 1024 * 1024 / sizeof(float);

  // Data (Vectors)
  std::vector<float> bufA(vector_size);
  std::vector<float> bufB(vector_size, 0.0f);
  std::vector<float> bufC(vector_size, 0.0f);

  // Init A
  for (size_t i = 0; i < vector_size; i++)
    bufA[i] = (float)i;

  starpu_data_handle_t hA, hB, hC;

  // Register A (Main RAM)
  starpu_memory_pin(bufA.data(), vector_size * sizeof(float));
  starpu_vector_data_register(&hA, STARPU_MAIN_RAM, (uintptr_t)bufA.data(),
                              vector_size, sizeof(float));

  // Register B (Main RAM, destination of prep)
  starpu_memory_pin(bufB.data(), vector_size * sizeof(float));
  starpu_vector_data_register(&hB, STARPU_MAIN_RAM, (uintptr_t)bufB.data(),
                              vector_size, sizeof(float));

  // Register C (GPU output)
  starpu_memory_pin(bufC.data(), vector_size * sizeof(float));
  starpu_vector_data_register(&hC, STARPU_MAIN_RAM, (uintptr_t)bufC.data(),
                              vector_size, sizeof(float));

  // Optional Transfer Buffer
  std::vector<float> bufTransfer;
  starpu_data_handle_t hTransfer = NULL;
  if (transfer_mb > 0) {
    size_t transfer_size = transfer_mb * 1024 * 1024 / sizeof(float);
    bufTransfer.resize(transfer_size, 0.0f);
    starpu_memory_pin(bufTransfer.data(), transfer_size * sizeof(float));
    starpu_vector_data_register(&hTransfer, STARPU_MAIN_RAM,
                                (uintptr_t)bufTransfer.data(), transfer_size,
                                sizeof(float));

    // Increment nbuffers for codelets to accept the extra handle
    cl_prep.nbuffers++;
    cl_infer.nbuffers++;
    cl_post.nbuffers++;
  }

  printf("Configuration:\n  Frames: %d\n  Size:   %d MB (%zu floats)\n  Infer "
         "Iters: %d\n  Tracing: %s\n  Trace Dir: %s\n  Scheduler: %s\n  Stats: "
         "%s\n  Transfer: %d MB\n",
         frames, mb, vector_size, infer_iters, trace ? "ENABLED" : "DISABLED",
         trace_dir.c_str(),
         sched_policy.empty() ? "DEFAULT" : sched_policy.c_str(),
         print_stats ? "YES" : "NO", transfer_mb);

  if (trace) {
    starpu_fxt_start_profiling();
    starpu_fxt_trace_user_event_string("BEGIN_ADV_MAIN_RUN");
  }

  APEX_START(ADV_MAIN_RUN);

  printf("[BEFORE] bufA[0]=%f, bufA[LAST]=%f\n", bufA[0],
         bufA[vector_size - 1]);

  for (int i = 0; i < frames; i++) {
    if (i % 10 == 0)
      printf("Submitting frame %d\n", i);

    // Prep: R(A) -> W(B)
    if (hTransfer) {
      ret = starpu_task_insert(&cl_prep, STARPU_R, hA, STARPU_W, hB, STARPU_W,
                               hTransfer, 0);
    } else {
      ret = starpu_task_insert(&cl_prep, STARPU_R, hA, STARPU_W, hB, 0);
    }
    STARPU_CHECK_RETURN_VALUE(ret, "starpu_task_insert prep");

    // Infer: R(B) -> W(C) [CUDA]
    if (hTransfer) {
      ret = starpu_task_insert(&cl_infer, STARPU_R, hB, STARPU_W, hC, STARPU_RW,
                               hTransfer, STARPU_VALUE, &infer_iters,
                               sizeof(infer_iters), 0);
    } else {
      ret = starpu_task_insert(&cl_infer, STARPU_R, hB, STARPU_W, hC,
                               STARPU_VALUE, &infer_iters, sizeof(infer_iters),
                               0);
    }
    STARPU_CHECK_RETURN_VALUE(ret, "starpu_task_insert infer");

    // Post: R(C) -> W(A)
    if (hTransfer) {
      ret = starpu_task_insert(&cl_post, STARPU_R, hC, STARPU_W, hA, STARPU_R,
                               hTransfer, 0);
    } else {
      ret = starpu_task_insert(&cl_post, STARPU_R, hC, STARPU_W, hA, 0);
    }
    STARPU_CHECK_RETURN_VALUE(ret, "starpu_task_insert post");

    // Logger: R(A)
    ret = starpu_task_insert(&cl_logger, STARPU_R, hA, 0);
    STARPU_CHECK_RETURN_VALUE(ret, "starpu_task_insert logger");
  }

  starpu_task_wait_for_all();

  APEX_STOP(ADV_MAIN_RUN);

  if (trace) {
    starpu_fxt_trace_user_event_string("END_ADV_MAIN_RUN");
    starpu_fxt_stop_profiling();
  }

  printf("[AFTER] bufA[0]=%f, bufA[LAST]=%f\n", bufA[0], bufA[vector_size - 1]);

  // Checksum
  double sum = 0.0;
  // Just sum first 5 and last 5 again for validation
  for (size_t i = 0; i < (vector_size < 5 ? vector_size : 5); i++)
    sum += bufA[i];
  if (vector_size > 5) {
    for (size_t i = vector_size - 5; i < vector_size; i++)
      sum += bufA[i];
  }
  printf("Final Checksum: %f\n", sum);

  if (print_stats) {
    fflush(stdout);
    printf("\n===STARPU_STATS_BEGIN===\n");
    fflush(stdout);
    starpu_profiling_bus_helper_display_summary();
    starpu_profiling_worker_helper_display_summary();
    starpu_data_display_memory_stats();
    fflush(stderr); // Ensure stats (stderr) are flushed
    printf("===STARPU_STATS_END===\n");
    fflush(stdout);
  }

  starpu_data_unregister(hA);
  starpu_data_unregister(hB);
  starpu_data_unregister(hC);
  starpu_memory_unpin(bufA.data(), vector_size * sizeof(float));
  starpu_memory_unpin(bufB.data(), vector_size * sizeof(float));
  starpu_memory_unpin(bufC.data(), vector_size * sizeof(float));

  if (hTransfer) {
    starpu_data_unregister(hTransfer);
    starpu_memory_unpin(bufTransfer.data(), bufTransfer.size() * sizeof(float));
  }

  starpu_shutdown();
  return 0;
}
