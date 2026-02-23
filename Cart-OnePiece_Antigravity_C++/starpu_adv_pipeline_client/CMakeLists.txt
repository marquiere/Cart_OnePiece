cmake_minimum_required(VERSION 3.10)
project(StarpuAdvPipelineClient C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Add definition for NDEBUG as requested by carla headers usually
add_definitions(-DNDEBUG)

# Find PkgConfig
find_package(PkgConfig REQUIRED)

# Find StarPU configuration
pkg_check_modules(STARPU REQUIRED starpu-1.4)

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

# Stubbed targets reusing client_smoke.cpp for now
# 3) carla_rgb_stream_smoke
add_executable(carla_rgb_stream_smoke apps/client_smoke.cpp)
target_link_libraries(carla_rgb_stream_smoke PRIVATE ${CARLA_LIBS})

# 4) carla_gt_stream_smoke
add_executable(carla_gt_stream_smoke apps/client_smoke.cpp) 
target_link_libraries(carla_gt_stream_smoke PRIVATE ${CARLA_LIBS})

# 5) sync_smoke
add_executable(sync_smoke apps/client_smoke.cpp) 
target_link_libraries(sync_smoke PRIVATE ${CARLA_LIBS})

# 6) pipeline_direct
add_executable(pipeline_direct apps/client_smoke.cpp) 
target_link_libraries(pipeline_direct PRIVATE ${CARLA_LIBS})

# 7) pipeline_starpu
add_executable(pipeline_starpu apps/client_smoke.cpp) 
target_link_libraries(pipeline_starpu PRIVATE ${CARLA_LIBS})
