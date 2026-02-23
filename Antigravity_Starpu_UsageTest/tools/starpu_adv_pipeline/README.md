# StarPU Advanced Pipeline Profiling Harness

A StarPU project skeleton and testing harness using CMake. It implements a CPU-GPU-CPU task pipeline designed to validate and profile complex StarPU executions, including tracing, hardware counters, APEX integrations, and energy measurements.

---

## 1. Quick Start

### Prerequisites
Ensure `pkg-config` can find the StarPU 1.4 library:
```bash
pkg-config --modversion starpu-1.4
```
If this fails, verify your `PKG_CONFIG_PATH`.

### Build Instructions
You can build the project from the root folder `Antigravity_Starpu_UsageTest`:

```bash
cmake -S tools/starpu_adv_pipeline -B tools/starpu_adv_pipeline/build
cmake --build tools/starpu_adv_pipeline/build -j
```

*(Troubleshooting: Use `ldd tools/starpu_adv_pipeline/build/starpu_adv_pipeline | grep starpu` to ensure the executable finds `libstarpu`. If not, update your `LD_LIBRARY_PATH`.)*

---

## 2. Using the Verification Script (Recommended)

The easiest and most comprehensive way to run the pipeline is via the `./verify_profiling.sh` script located in the root directory. It automates execution, tracing, environment setup, and artifact post-processing.

### Basic Run
```bash
./verify_profiling.sh
```

### Complete Run
To run the full suite using all measurement tools available (PAPI, Energy RAPL sampling, memory transfer stress, APEX trace generation, experimental schedulers, and FxT tracing):
```bash
sudo -v
export STARPU_PROFILING=1
export STARPU_PAPI_EVENTS="PAPI_TOT_CYC,PAPI_TOT_INS"
export USE_TASKSTUBS=1

./verify_profiling.sh \
  --frames 40 \
  --mb 4 \
  --infer-iters 5 \
  --memory \
  --transfer-mb 128 \
  --apex-mode all \
  --sched rr_workers \
  --trace
```

### Adjusting Workload
You can override the default workload parameters:
```bash
# Run with 50 frames, 4MB data payload per frame
./verify_profiling.sh --frames 50 --mb 4

# Increase the compute intensity of the GPU kernel
./verify_profiling.sh --infer-iters 10
```

### Scheduler Selection
Test different StarPU scheduling policies using the `--sched` flag:
```bash
# Default StarPU schedulers
./verify_profiling.sh --sched lws
./verify_profiling.sh --sched dmda

# Custom experimental schedulers
./verify_profiling.sh --sched rr_workers
```

---

## 3. Profiling & Measurement Features

The harness supports various advanced measurement tools, which can be toggled when using the verification script.

### Hardware Counters (PAPI)
Collects cache misses, CPU cycles, and instructions via PAPI.
1. Enable profiling in your environment:
   ```bash
   export STARPU_PROFILING=1
   export STARPU_PAPI_EVENTS="PAPI_TOT_CYC,PAPI_TOT_INS"
   ```
2. Verify kernel permissions (needs to be `1` or `0`):
   ```bash
   sudo sysctl -w kernel.perf_event_paranoid=1
   ```
3. Run the script: `./verify_profiling.sh`

### Energy Measurement (Powercap/RAPL & nvidia-smi)
The harness automatically samples CPU and GPU power draw at 10Hz and estimates total energy consumption in Joules.
- **CPU (RAPL)**: Reads `/sys/class/powercap/intel-rapl`. You may need to run `sudo -v` before executing the script to grant read access.
- **GPU (NVIDIA)**: Uses background `nvidia-smi` sampling.

### Memory / PCIE Transfer Stressing
Analyze bus bandwidth and PCIE bottlenecks by forcing dummy buffer transfers between CPU and GPU:
```bash
./verify_profiling.sh --memory --transfer-mb 512
```

### APEX Tracing via TaskStubs
APEX integration allows for highly detailed tracing (e.g., Google Trace logs `chrome://tracing`) and task dependency graphing.

1. **Build with TaskStubs**:
   ```bash
   cmake -S tools/starpu_adv_pipeline -B tools/starpu_adv_pipeline/build \
         -DENABLE_TASKSTUBS_APEX=ON -DAPEX_PREFIX=~/src/apex/install
   cmake --build tools/starpu_adv_pipeline/build -j
   ```
2. **Run APEX Modes**:
   ```bash
   export USE_TASKSTUBS=1
   # Run all APEX profiling modes (gtrace, taskgraph, etc.)
   ./verify_profiling.sh --apex-mode all
   ```

---

## 4. Output Organization (Run Artifacts)

The verification script generates a **Timestamped Run Folder** in `/tmp/starpu_traces_adv/` (e.g., `/tmp/starpu_traces_adv/20260216_143000/`).

The layout is strictly organized by tool:

| Folder | Description | Key Artifacts |
| :--- | :--- | :--- |
| **00_raw** | Immutable raw binary traces from StarPU. | `prof_file_*` |
| **01_starpu_fxt_tool** | Processed StarPU trace formats. | `paje.trace`, `tasks.rec` |
| **02_graphviz** | Visual Task Dependency Graph (DAG). | `dag.pdf`, `dag.dot` |
| **03_tasks_complete** | Task details with completion info. | `tasks.rec`, `tasks2.rec` |
| **04_codelet_profile** | Per-codelet performance profiles. | `distrib.data`, `*.pdf` |
| **05_histo** | Execution time histograms. | `histo.log`, `*.pdf` |
| **06_data_trace** | Data transfer traces and timing. | `data_trace.txt` |
| **07_state_stats** | State statistics (Python analysis). | `state_stats.txt` |
| **08_starvz** | StarVZ R-based visualization (if installed). | `starvz.pdf` |
| **09_papi** | PAPI hardware counters (if enabled). | `papi.rec` |
| **10_apex** | APEX profiling traces (if enabled). | `trace_events.*.json`, `*.pdf` |
| **11_memory** | Memory/Bus transfer statistics. | `starpu_stats.txt` |
| **12_energy** | Power sampling logs and Joule estimates. | `cpu_rapl_samples.csv`, `gpu_power_samples.csv` |
| **summary** | Run manifest, arguments, and full execution log. | `run.log`, `cmd.txt`, `env.txt` |

*(Tip: To view APEX `gtrace` files, open [ui.perfetto.dev](https://ui.perfetto.dev) and drag & drop the `trace_events.*.json` file).*

---

## 5. Advanced / Manual Execution

If you prefer to bypass `verify_profiling.sh`, you can execute the compiled binary directly.

```bash
./tools/starpu_adv_pipeline/build/starpu_adv_pipeline --frames 20 --mb 2 --infer-iters 5
```

For FxT tracing natively without the script:
```bash
./tools/starpu_adv_pipeline/build/starpu_adv_pipeline --trace
```

Using APEX via `apex_exec` wrapper instead of TaskStubs:
```bash
export APEX_OTF2=1
apex_exec ./tools/starpu_adv_pipeline/build/starpu_adv_pipeline --frames 20 --mb 2
```
