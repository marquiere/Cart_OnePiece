#include <cstdlib>
#include <iostream>
#include <starpu.h>

// Minimal StarPU CPU task
void minimal_cpu_func(void *buffers[], void *cl_arg) {
  (void)buffers; // Unused
  (void)cl_arg;  // Unused
  std::cout << "Inside minimal StarPU task." << std::endl;
}

static struct starpu_codelet minimal_cl = {
    .cpu_funcs = {minimal_cpu_func},
    .cpu_funcs_name = {"minimal_cpu_func"},
    .nbuffers = 0,
    .name = "minimal_task"};

int main(int argc, char **argv) {
  int ret;

  // Initialize StarPU
  ret = starpu_init(NULL);
  if (ret != 0) {
    std::cerr << "starpu_init failed with error code: " << ret << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "StarPU initialized successfully." << std::endl;

  // Submit a minimal task
  struct starpu_task *task = starpu_task_create();
  task->cl = &minimal_cl;
  task->synchronous = 1;

  ret = starpu_task_submit(task);
  if (ret != 0) {
    std::cerr << "starpu_task_submit failed with error code: " << ret
              << std::endl;
    starpu_shutdown();
    return EXIT_FAILURE;
  }

  std::cout << "Task executed." << std::endl;

  // Shutdown StarPU
  starpu_shutdown();

  // Required expected output
  std::cout << "OK" << std::endl;

  return EXIT_SUCCESS;
}
