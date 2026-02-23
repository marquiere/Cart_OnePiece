# Build and Run Instructions

## Prerequisites
- A running CARLA 0.9.16 server on localhost (port 2000).
- The `reference/CppClient/libcarla-install` directory must exist and contain the compiled libraries.

## Building

1. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

2. Run CMake:
   ```bash
   cmake ..
   ```

3. Compile:
   ```bash
   make
   ```

## Running

Run the executable from the build directory:

```bash
./carla_mercedes_spawner
```

Or specify a custom host/port:

```bash
./carla_mercedes_spawner 192.168.1.10 2000
```

## Troubleshooting Paths

If you encounter errors about missing headers or libraries:
1. Check `CMakeLists.txt` and ensure `CARLA_INSTALL_DIR` points to the correct location of your `libcarla-install`.
2. Currently it expects it at: `../reference/CppClient/libcarla-install` (relative to the project root, or absolute path in the CMake file).

If you see runtime errors about shared libraries:
- The configuration uses static libraries (`.a`) for `libcarla_client` and `librpc`, so they are built into the binary.
- If it complains about system libraries (like zlib, etc.), ensure they are installed on your OS (`sudo apt-get install zlib1g-dev libpng-dev ...`).
