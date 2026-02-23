cmake_minimum_required(VERSION 3.10)
project(CarlaMercedesSpawner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Configuration for CARLA 0.9.16 ---

# Allow overriding CARLA_ROOT from command line (cmake -DCARLA_ROOT=...)
if(NOT DEFINED CARLA_ROOT)
    set(CARLA_ROOT "${CMAKE_SOURCE_DIR}/reference/CppClient/libcarla-install")
endif()

message(STATUS "Using CARLA Root: ${CARLA_ROOT}")

# Check if the directory actually exists
if(NOT EXISTS "${CARLA_ROOT}")
    message(FATAL_ERROR "Could not find CARLA install directory at ${CARLA_ROOT}. \
    Please make sure the reference material is present or specify -DCARLA_ROOT=/path/to/libcarla-install")
endif()

# Include Directories
include_directories(
    "${CARLA_ROOT}/include"
    "${CARLA_ROOT}/include/system"
)

# Lib Directory
link_directories(
    "${CARLA_ROOT}/lib"
)

# Compiler definitions (from reference Maven/Makefile)
add_definitions(-DNDEBUG) 

# --- Executable ---
add_executable(carla_mercedes_spawner main.cpp)

# --- Linking ---
# Linking
set(BOOST_LIBS
    "${CARLA_ROOT}/lib/libboost_filesystem.a"
    "${CARLA_ROOT}/lib/libboost_atomic.a"
    "${CARLA_ROOT}/lib/libboost_system.a"
)

target_link_libraries(carla_mercedes_spawner
    PRIVATE
    carla_client
    rpc
    ${BOOST_LIBS}
    Recast
    Detour
    DetourCrowd
    png
    tiff
    jpeg
    pthread
    dl
)

# If you are on Linux and using static libs for boost/carla, you might need -latomic or others depending on the compiler version
# target_link_libraries(carla_mercedes_spawner PRIVATE atomic)
