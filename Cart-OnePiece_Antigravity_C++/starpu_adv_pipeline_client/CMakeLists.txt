cmake_minimum_required(VERSION 3.10)
project(StarpuAdvPipelineClient C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-O3 -g -Wall")
# Add definition for NDEBUG as requested by carla headers usually
add_definitions(-DNDEBUG)

# Find PkgConfig
include(CTest)
enable_testing()

find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)

# Find StarPU configuration
pkg_check_modules(STARPU REQUIRED starpu-1.4)

# Find APEX configuration (Optional Native Integration)
pkg_check_modules(APEX apex)
if(APEX_FOUND)
    message(STATUS "APEX found via pkg-config, enabling native API instrumentation.")
    include_directories(${APEX_INCLUDE_DIRS})
    link_directories(${APEX_LIBRARY_DIRS})
    set(APEX_LIB_DEPS ${APEX_LIBRARIES})
    add_definitions(-DENABLE_APEX)
else()
    set(APEX_PREFIX "$ENV{HOME}/apex/install")
    if(EXISTS "${APEX_PREFIX}/include/apex_api.hpp")
        message(STATUS "APEX found via manual prefix: ${APEX_PREFIX}")
        include_directories(${APEX_PREFIX}/include)
        link_directories(${APEX_PREFIX}/lib)
        set(APEX_LIB_DEPS apex)
        add_definitions(-DENABLE_APEX)
        set(APEX_FOUND TRUE)
    endif()
endif()

# Set CARLA_ROOT
if(NOT DEFINED CARLA_ROOT)
    set(CARLA_ROOT "${CMAKE_SOURCE_DIR}/../reference/CppClient/libcarla-install")
endif()

set(CARLA_INCLUDE_DIRS "${CARLA_ROOT}/include" "${CARLA_ROOT}/include/system")
set(CARLA_LIB_DIR "${CARLA_ROOT}/lib")

# Include dirs
include_directories(
    include
    ${STARPU_INCLUDE_DIRS}
    ${CARLA_INCLUDE_DIRS}
)
link_directories(
    ${CARLA_LIB_DIR}
    ${STARPU_LIBRARY_DIRS}
)

# For carla linkage (stubs don't need to link it if they don't call it, but good to have)
set(BOOST_LIBS
    "${CARLA_ROOT}/lib/libboost_filesystem.a"
    "${CARLA_ROOT}/lib/libboost_atomic.a"
    "${CARLA_ROOT}/lib/libboost_system.a"
)
set(CARLA_LIBS
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

# 1) client_smoke
add_executable(client_smoke apps/client_smoke.cpp)
target_link_libraries(client_smoke PRIVATE ${CARLA_LIBS})

# 2) starpu_smoke
add_executable(starpu_smoke apps/starpu_smoke.cpp)
target_link_options(starpu_smoke PRIVATE ${STARPU_LDFLAGS})
target_link_libraries(starpu_smoke PRIVATE ${STARPU_LIBRARIES})

# 3) carla_rgb_stream_smoke
add_executable(carla_rgb_stream_smoke apps/carla_rgb_stream_smoke.cpp)
target_link_libraries(carla_rgb_stream_smoke PRIVATE ${CARLA_LIBS})

# 4) carla_gt_stream_smoke
add_library(semantic_decode STATIC src/semantic_decode.cpp)
target_include_directories(semantic_decode PUBLIC include)

add_executable(carla_gt_stream_smoke apps/carla_gt_stream_smoke.cpp)
target_link_libraries(carla_gt_stream_smoke PRIVATE ${CARLA_LIBS} semantic_decode)

# 5) sync_smoke
add_library(frame_sync STATIC src/frame_sync.cpp)
target_include_directories(frame_sync PUBLIC include)

add_executable(sync_smoke apps/sync_smoke.cpp)
target_link_libraries(sync_smoke PRIVATE ${CARLA_LIBS} frame_sync semantic_decode)

# 6) Preprocess Module
add_library(preprocess STATIC src/preprocess.cpp)
target_include_directories(preprocess PUBLIC include)

add_library(tensor_stats STATIC src/tensor_stats.cpp)
target_include_directories(tensor_stats PUBLIC include)

add_executable(preprocess_smoke apps/preprocess_smoke.cpp)
target_link_libraries(preprocess_smoke PRIVATE ${CARLA_LIBS} preprocess tensor_stats)

add_executable(preprocess_unit_test tests/preprocess_unit_test.cpp)
target_link_libraries(preprocess_unit_test PRIVATE preprocess)
add_test(NAME PreprocessUnitTest COMMAND preprocess_unit_test)

# 7) TensorRT Runtime
option(WITH_TRT "Compile with TensorRT support" OFF)
if(WITH_TRT)
    find_library(NVINFER_LIB NAMES nvinfer nvinfer.so.10 libnvinfer.so.10 HINTS /usr/lib/x86_64-linux-gnu /usr/lib)
    find_library(NVINFER_PLUGIN_LIB NAMES nvinfer_plugin nvinfer_plugin.so.10 libnvinfer_plugin.so.10 HINTS /usr/lib/x86_64-linux-gnu /usr/lib)
    find_library(NVONNXPARSER_LIB NAMES nvonnxparser nvonnxparser.so.10 libnvonnxparser.so.10 HINTS /usr/lib/x86_64-linux-gnu /usr/lib)
    find_library(CUDA_RT_LIB NAMES cudart HINTS /usr/local/cuda/lib64 /usr/lib/x86_64-linux-gnu)
    find_path(TRT_INCLUDE_DIR NvInfer.h HINTS /usr/include/x86_64-linux-gnu /usr/include)

    if (NVINFER_LIB AND CUDA_RT_LIB AND TRT_INCLUDE_DIR)
        message(STATUS "TensorRT found. Enabling trt_smoke build.")
        add_library(trt_runner STATIC src/trt_runner.cpp)
        target_include_directories(trt_runner PUBLIC include ${TRT_INCLUDE_DIR})
        target_link_libraries(trt_runner PRIVATE ${NVINFER_LIB} ${NVINFER_PLUGIN_LIB} ${NVONNXPARSER_LIB} ${CUDA_RT_LIB})

        add_executable(trt_smoke apps/trt_smoke.cpp)
        target_link_libraries(trt_smoke PRIVATE trt_runner ${NVINFER_LIB} ${CUDA_RT_LIB})
    else()
        message(FATAL_ERROR "WITH_TRT=ON but TensorRT or CUDA dependencies not found! "
                            "Please install Nvidia drivers, CUDA Toolkit, and TensorRT "
                            "(e.g., sudo apt install libnvinfer-dev libnvinfer8).")
    endif()
endif()

# --- Postprocess ---
add_library(postprocess STATIC src/postprocess.cpp)
target_include_directories(postprocess PUBLIC include)

# --- Metrics ---
add_library(metrics STATIC src/metrics.cpp)
target_include_directories(metrics PUBLIC include)

# 8) pipeline_direct
add_executable(pipeline_direct apps/pipeline_direct.cpp) 
target_link_libraries(pipeline_direct PRIVATE 
    ${CARLA_LIBS}
    preprocess
    postprocess
    metrics
    semantic_decode
    frame_sync
)
if(WITH_TRT)
    target_link_libraries(pipeline_direct PRIVATE trt_runner ${NVINFER_LIB} ${CUDA_RT_LIB})
endif()

# --- Visualization ---
add_library(viewer_sdl2 STATIC src/viewer_sdl2.cpp)
target_include_directories(viewer_sdl2 PUBLIC include)
target_link_libraries(viewer_sdl2 PRIVATE ${SDL2_LIBRARIES})
if(SDL2_INCLUDE_DIRS)
  target_include_directories(viewer_sdl2 PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

# 9) pipeline_starpu
add_executable(pipeline_starpu apps/pipeline_starpu.cpp src/starpu_runtime.cpp src/starpu_codelets.cpp)
target_link_libraries(pipeline_starpu PRIVATE
    ${CARLA_LIBS}
    preprocess
    postprocess
    metrics
    semantic_decode
    frame_sync
    viewer_sdl2
    vis
    ${STARPU_LIBRARIES}
)
if(APEX_FOUND)
    target_link_libraries(pipeline_starpu PRIVATE ${APEX_LIB_DEPS})
endif()
target_link_options(pipeline_starpu PRIVATE ${STARPU_LDFLAGS})
if(WITH_TRT)
    target_link_libraries(pipeline_starpu PRIVATE trt_runner ${NVINFER_LIB} ${CUDA_RT_LIB})
endif()

# 8) profile_client_stub
add_executable(profile_client_stub apps/profile_client_stub.cpp)
target_link_libraries(profile_client_stub PRIVATE ${CARLA_LIBS})

# --- Visualization ---
add_library(vis STATIC src/vis.cpp)
target_include_directories(vis PUBLIC include)

# 10) dataset_sanity
add_executable(dataset_sanity apps/dataset_sanity.cpp)
target_link_libraries(dataset_sanity PRIVATE
    ${CARLA_LIBS}
    vis
    preprocess
    postprocess
    semantic_decode
    frame_sync
    viewer_sdl2
)
if(WITH_TRT)
    target_link_libraries(dataset_sanity PRIVATE trt_runner ${NVINFER_LIB} ${CUDA_RT_LIB})
endif()
if(SDL2_INCLUDE_DIRS)
  target_include_directories(dataset_sanity PUBLIC ${SDL2_INCLUDE_DIRS})
endif()
