# CArt-OnePiece Framework: StarPU ADV Pipeline Client

This folder contains the incremental development of the ADV workload measurement client integrating CARLA and StarPU.

## Project Structure
- `apps/` - Small executables and smoke tests.
- `src/` - Core implementation source files.
- `include/` - Header files.
- `tools/` - Scripts for running servers, clients, and monitoring.
- `third_party/` - Dependencies like stb and tiny utilities.
- `models/` - Placeholder for TensorRT engines.
- `runs/` - Generated outputs (ignored by git).

## Step 0 Verification Checklist

**Commands to run:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/client_smoke --help
./build/starpu_smoke
```

**Expected Results:**
- [ ] Build succeeds without errors.
- [ ] Both executables run without errors.
- [ ] `starpu_smoke` prints "OK".

**What to check if it fails:**
- **CMake configure step fails:** Make sure `pkg-config` can find `starpu-1.4`. Ensure `PKG_CONFIG_PATH` is set correctly. Check if `CARLA_ROOT` exists correctly.
- **Build step fails:** Check if `pkg-config` returned the right flags for StarPU. Make sure your C++ standard compiler supports C++17.
- **`starpu_smoke` fails/aborts:** Ensure the system has enough memory and StarPU configuration hasn't been corrupted. Ensure no conflicting StarPU versions are initialized. Wait for the task to finish completely.

## Step 1 Verification Checklist

**Commands to run:**
1. Build (if not already):
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ```
2. Start sever:
   ```bash
   ./tools/run_server.sh --port 2000
   ```
   *(Note: if CARLA_ROOT is not set, pass `--carla_root /path/to/carla`)*
3. In a second terminal, run client:
   ```bash
   ./build/client_smoke --host 127.0.0.1 --port 2000
   ```
4. Stop server:
   ```bash
   ./tools/kill_server.sh
   ```

**Expected Results:**
- [x] `runs/<timestamp>/server.log` exists and grows.
- [x] Client program prints the map name (e.g. `Map Name: Carla/Maps/Town10HD_Opt`).
- [x] Client program prints `CONNECTED OK`.
- [x] Kill script stops the CARLA process successfully with no leftover `CarlaUE4` background tasks.

**What to check if it fails:**
- **Missing CARLA_ROOT / wrong --carla_root:** The `run_server.sh` script will complain it cannot find `CarlaUE4.sh`. Pass the correct path with `--carla_root /home/eric/CARLA_Latest` or export it.
- **Wrong port / server not started:** The client will hang for 10 seconds and then print a timeout exception.
- **CARLA binary not executable:** The bash script will complain it doesn't have permissions to run `CarlaUE4.sh`. Run `chmod +x` on it.
- **Cannot connect:** Check the `runs/<timestamp>/server.log` file. CARLA might be out of memory, colliding with an existing port, or running on a headless server without `vulkan`/`opengl` fallbacks.

## Step 2 Verification Checklist

**How to set Cores:**
Prefix the launch command with `SERVER_CORES` and `CLIENT_CORES` environment variables. By default, it splits total system cores 50/50.

**Commands to run:**
1. Run profiled launch:
   ```bash
   SERVER_CORES="0-9" CLIENT_CORES="10-19" ./tools/run_profiled_single_machine.sh --port 2000
   ```
2. In another terminal, monitor for 30 seconds:
   ```bash
   ./tools/monitor_split.sh --duration 30
   ```
3. Stop server:
   ```bash
   ./tools/kill_server.sh
   ```

**Expected Results:**
- [ ] `runs/<timestamp>/pids.env` exists and contains `SERVER_PID` and `CLIENT_PID`.
- [ ] Monitoring logs (`cpu_server.log`, `cpu_client.log`, `gpu_pmon.log`) are created and non-empty.
- [ ] CPU logs show the client process using mostly `CLIENT_CORES` (strongly biased).
- [ ] GPU pmon log contains entries for at least one of the PIDs.

**What to check if it fails:**
- **pidstat missing:** install sysstat (`sudo apt install sysstat`)
- **nvidia-smi missing:** NVIDIA driver not installed
- **pmon requires permissions:** Wait for it to fall back to plain `nvidia-smi` sampling natively
- **taskset not found:** install util-linux (`sudo apt install util-linux`)

## Step 3 Verification Checklist

**Commands to run:**
1. Build (if not already):
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ```
2. Start sever:
   ```bash
   ./tools/run_server.sh --port 2000
   ```
3. Run RGB smoke:
   ```bash
   ./build/carla_rgb_stream_smoke --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames 200
   ```
4. Stop server:
   ```bash
   ./tools/kill_server.sh
   ```

**Optional profiled run:**
```bash
SERVER_CORES="0-9" CLIENT_CORES="10-19" ./tools/run_profiled_single_machine.sh --port 2000 --client ./build/carla_rgb_stream_smoke --client_args "--frames 300"
```

**Expected Results:**
- [ ] Program receives exactly N frames and exits cleanly with 0.
- [ ] Raw buffer size printed is exactly `1920000` (800x600x4).
- [ ] Average dt is close to 50ms for fps=20 (with allowble jitter).
- [ ] Actors are safely destroyed before the program completes.
- [ ] CPU pinning load splits accurately during the optional execution plan.

**What to check if it fails:**
- **No frames received:** The server map is not fully loaded yet, or your client is too quick to time out. Check `server.log`.
- **Spawn fail:** Check if the map is valid and has spawn points.
- **Sensor not ticking:** Ensure synchronous mode vs asynchronous mode mismatch isn't blocking the TICK. (CARLA default is async).
- **Buffer size mismatch:** You chose a different width/height but CARLA API pixel depth assumptions didn't match.

## Step 4 Verification Checklist

**Commands to run:**
1. Build:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ```
2. Start server (terminal 1):
   ```bash
   ./tools/run_server.sh --port 2000
   ```
3. Run GT smoke (terminal 2):
   ```bash
   ./build/carla_gt_stream_smoke --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames 200 --print_every 20 --assume_bgra 1
   ```
4. If output suggests wrong channel (e.g., all zeros), retry:
   ```bash
   ./build/carla_gt_stream_smoke --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames 60 --print_every 10 --assume_bgra 0
   ```
5. Stop server:
   ```bash
   ./tools/kill_server.sh
   ```

**Expected Results:**
- [ ] Program receives exactly N frames and exits 0
- [ ] Raw buffer size is always w*h*4 (for 800x600: 1,920,000 bytes)
- [ ] Unique label count is > 1 for typical scenes (road/vehicle/sidewalk/etc.)
- [ ] Top labels show plausible distribution (e.g., road/sky/buildings dominate)

**What to check if it fails:**
- **Label set looks constant:** Camera is stuck or sensor_tick issue preventing world update.

## Step 5 Verification Checklist

**Description:** 
This step implements `FrameSync`, a thread-safe, bounded synchronizer that pairs RGB and GT frames by `frame_id`. It features window-based overflow protection and age-based cleanup.

**Commands to run:**
1. Build:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ```
2. Start server (terminal 1):
   ```bash
   ./tools/run_server.sh --port 2000
   ```
3. Run sync smoke (terminal 2):
   ```bash
   ./build/sync_smoke --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --matches 200 --window 50 --print_every 20 --sync 1
   ```
4. Stop server:
   ```bash
   ./tools/kill_server.sh
   ```

**Expected Results:**
- [ ] `sync_smoke` reaches 200 matched pairs and exits 0.
- [ ] Match rate is 100% (since --sync 1 ensures simulation alignment).
- [ ] Buffered counts stay near zero after initial startup.
- [ ] Delta between RGB and GT timestamps is exactly 0.0000s.

**Troubleshooting Notes:**
- **Low match rate in Async Mode:** If running with `--sync 0`, match rates may drop to 60-70% due to sensor phase jitter across world ticks. For dataset pairing, always use `--sync 1`.
- **Match stuck:** Ensure both cameras are spawned and listening. If one fails to spawn, the synchronizer will fill to `window` and start dropping.
- **Increasing drops:** Machine is too slow to process callbacks, or the consumer loop is blocked. Decrease resolution or FPS.

## Step 6 Verification Checklist

**Description:** 
This step builds and verifies the math kernel implementing Bilinear/Nearest Neighbor interpolation from natively interleaved `BGRA` bytes to separated `NCHW` Float32 arrays scaled using standard ImageNet `mean`/`std` normalizations.

**Commands to run:**
1. Build:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ```
2. Run pure math unit-tests without CARLA:
   ```bash
   ctest --test-dir build --output-on-failure
   ```
3. Start server:
   ```bash
   ./tools/run_server.sh --port 2000
   ```
4. Run live smoke test on frame 1:
   ```bash
   ./build/preprocess_smoke --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --out_w 512 --out_h 256 --resize bilinear --assume_bgra 1 --to_rgb 1
   ```
5. Stop server:
   ```bash
   ./tools/kill_server.sh
   ```

**Expected Results:**
- [ ] `ctest` passes 100% of cases.
- [ ] Output prints valid floating logic (NO `NaN` / NO `Inf`).
- [ ] Normalization distribution generally bounded `[-3.0, 3.0]`. Means per channel vary. 
- [ ] Exits successfully clean after strictly 1 frame.

**Troubleshooting Notes:**
- **NaN Outputs:** Zero variance standard deviation parameters passed in settings logic or buffer memory faults.
- **Wrong channel bounds:** A `false` pass into `--assume_bgra` will blindly invert Red and Blue channels into the network. Validate flags match runtime expectations.

## Step 8 Verification Checklist

**Description:** 
This step builds a full “real ADV-like” client pipeline running on a single machine without StarPU. It integrates RGB collection, preprocessing, TensorRT inference, logit-to-label postprocessing, and asynchronous metrics (Pixel Accuracy and mIoU).

**Prerequisites:**
- TensorRT enabled build (`-DWITH_TRT=ON`).
- Engine file exists (e.g., `models/dummy.engine`).

**Commands to run:**
1. Build:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_TRT=ON
   cmake --build build -j
   ```
2. Start server (terminal 1):
   ```bash
   ./tools/run_server.sh --port 2000
   ```
3. Run pipeline_direct (terminal 2):
   ```bash
   ./build/pipeline_direct --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames 200 --engine models/dummy.engine --out_w 512 --out_h 256 --assume_bgra 1 --resize bilinear --print_every 20 --eval_every 50
   ```
4. Stop server:
   ```bash
   ./tools/kill_server.sh
   ```

**Expected Results:**
- [x] Processes exactly 200 matched frames and exits 0.
- [x] Prints stable timings per stage (Pre, Inf, Post, Total).
- [x] Creates `runs/<timestamp>/pipeline_direct.csv` with 200 data lines.
- [x] Evaluation prints pixel accuracy and mIoU every 50 frames.
- [x] Overload test (e.g., higher resolutions) reports drops if backlog exceeds threshold.

**Troubleshooting Notes:**
- **Engine output mismatch:** Check if the runner detected "logit" or "label" format correctly.
- **Low Match Rate:** Ensure `--sync 1` is not being overridden to async. 
- **CUDA Out of Memory:** Reduce resolution if running multiple heavy clients.

## Step 10 Verification Checklist

**Description:** 
Generate a reproducible single-machine workload split report identifying CPU/GPU loads and StarPU task latencies specifically for the CARLA server vs the Advanced Engine pipeline.

**Commands to run:**
1. Build with all components enabled:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_TRT=ON -DWITH_STARPU=ON
   cmake --build build -j
   ```
2. Run profiled single-machine launch (starts server + client):
   ```bash
   SERVER_CORES="0-9" CLIENT_CORES="10-19" ./tools/run_profiled_single_machine.sh --port 2000 --client ./build/pipeline_starpu --client_args "--host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames 120 --engine models/dummy.engine --out_w 512 --out_h 256 --inflight 2 --cpu_workers 8 --print_every 20 --eval_every 60"
   ```
3. Monitor telemetry for 60 seconds (run simultaneously in another terminal):
   ```bash
   ./tools/monitor_split.sh --seconds 60
   ```
4. Generate the summary report (TXT & JSON):
   ```bash
   python3 ./tools/summarize_run.py
   ```
5. Stop CARLA server:
   ```bash
   ./tools/kill_server.sh
   ```

**Expected Results:**
- [x] `runs/<timestamp>/pids.env` is properly populated with `SERVER_PID`, `CLIENT_PID`, `RUN_DIR`.
- [x] CPU logs `monitor_cpu_server.log` and `monitor_cpu_client.log` are recorded successfully.
- [x] `summary.txt` cleanly separates Server CPU% from Client CPU%, along with VRAM utilization and End-to-End Latencies.
- [x] No unbounded log growth is detected, and scripts exit with code `0`.

**Troubleshooting Notes:**
- **`pidstat` not found error:** Ensure `sysstat` is installed (`sudo apt-get install sysstat`).
- **`nvidia-smi` empty or lacks permissions:** The script automatically attempts `compute-apps` log fallbacks (`GPU_METHOD=compute-apps`) and records limits in `monitor_meta.txt`.
- **Client or Server PIDs tracking 0% CPU:** Because bash wrapper scripts (like `CarlaUE4.sh`) spawn children executing dense logic, the monitored PID might be anchored to a sleeping parent.
- **Float Locale Mismatches:** On European systems, `pidstat` typically yields float values using commas (e.g., `3,40`); `summarize_run.py` automatically sanitizes `.replace(',', '.')` to avert type casting errors.

## Step 10.5 Verification Checklist

**Description:** 
Create a small "sanity dataset" and visual output to prove that RGB and GT are synchronized correctly by `frame_id`, GT decoding is physically plausible, Pred output formatting + postprocessing is fundamentally sound, and Pred/GT alignments are pixel-accurate (with resizing natively covered). This functionality will be off-by-default for benchmarks but serves as a deterministic debug toggle.

**Commands to run:**
1. Build with all components enabled:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_TRT=ON -DWITH_STARPU=ON
   cmake --build build -j
   ```
2. Start server (terminal 1):
   ```bash
   ./tools/run_server.sh --port 2000
   ```
3. Run RGB+GT Bounding Test (terminal 2):
   ```bash
   ./build/dataset_sanity --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 10 --frames 30 --no_pred 1 --assume_bgra 1
   ```
4. Verify RGB and GT mapping logic inside `runs/<timestamp>/sanity_dataset/` bounds.
5. Run Full Inference + Overlay Extraction Test (terminal 2):
   ```bash
   ./build/dataset_sanity --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 10 --frames 30 --engine models/dummy.engine --out_w 512 --out_h 256 --assume_bgra 1
   ```
6. Stop CARLA server:
   ```bash
   ./tools/kill_server.sh
   ```

**Expected Results:**
- [x] RGB and GT binary/png logic extracts smoothly without aborting on "actor spawn failure" or iteration pointer faults.
- [x] `meta.json` drops explicitly into the `sanity_dataset` block containing runtime shapes.
- [x] `vis.hpp` blends alpha overlays of prediction mapped onto the upsampled source image correctly.
- [x] Dynamic shapes (`dummy.engine`) are captured by `SetInputShapeIfDynamic` and class channels bound properly via `GetOutputBytes`.

**Troubleshooting Notes:**
- **`Terminate called after throwing an instance of 'std::runtime_error'`:** Often `Spawn failed because of invalid actor description`. Check if `vehicle.mercedes.coupe_2020` is explicitly available in your simulator's `.egg` distribution.
- **`Unexpected engine output shape/type`:** Engine outputs dimensions misaligned with inferred shape parameters or bytes layout mappings. Ensure explicit inference resolutions `--out_w` and `--out_h` match the model definition.
