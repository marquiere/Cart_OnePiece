# Custom StarPU Scheduler: rr_workers

This directory contains a custom StarPU scheduler plugin implementing **strict round-robin across workers**.

### Features
- Global FIFO queue for tasks.
- Strict turn-based round-robin enforcement across all available workers in the context.
- Deadlock-avoidance (yields turn if the selected worker cannot execute any available tasks).

### Build Instruction

```bash
mkdir build && cd build
cmake ..
make
```

### Run Instructions

Load the plugin at runtime using StarPU's `STARPU_SCHED_LIB` dynamically loaded library feature:

```bash
export STARPU_SCHED_LIB=$(pwd)/build/libstarpu_sched_rr_workers.so
export STARPU_SCHED=rr_workers

# Enforce exactly 3 CPU and 1 CUDA worker
export STARPU_NCPU=3
export STARPU_NCUDA=1

# Enable Debug (optional)
# export RR_WORKERS_DEBUG=1
```

Or just use the main `verify_profiling.sh` script to automate everything:

```bash
./verify_profiling.sh --sched rr_workers
```
