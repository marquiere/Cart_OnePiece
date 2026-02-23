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
