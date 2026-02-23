cmake_minimum_required(VERSION 3.10)
project(starpu_adv_pipeline LANGUAGES C CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)

# StarPU 1.4 support via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(STARPU REQUIRED starpu-1.4)

# TaskStubs Support (APEX Integration)
option(ENABLE_TASKSTUBS_APEX "Enable APEX integration via StarPU TaskStubs" OFF)

if(ENABLE_TASKSTUBS_APEX)
    # Force Enable APEX because timer_plugin depends on it
    set(ENABLE_APEX ON CACHE BOOL "Enable APEX measurements" FORCE)
    
    pkg_check_modules(TASKSTUBS REQUIRED taskstubs)
    message(STATUS "TaskStubs Found (Static Lib)")
    
    include_directories(${TASKSTUBS_INCLUDE_DIRS})
    link_directories(${TASKSTUBS_LIBRARY_DIRS})
    
    add_definitions(-DENABLE_TASKSTUBS_APEX)
endif()

# APEX support (Optional)
option(ENABLE_APEX "Enable APEX measurements" OFF)

if(ENABLE_APEX)
    # Try finding APEX via pkg-config
    pkg_check_modules(APEX apex)
    
    if(NOT APEX_FOUND)
        # Fallback: Manual prefix
        set(APEX_PREFIX "$ENV{APEX_PREFIX}" CACHE PATH "Path to APEX installation")
        if(NOT APEX_PREFIX)
            set(APEX_PREFIX "~/src/apex/install") # Default as requested
        endif()
        
        message(STATUS "APEX pkg-config not found, using prefix: ${APEX_PREFIX}")
        include_directories(${APEX_PREFIX}/include)
        link_directories(${APEX_PREFIX}/lib)
        set(APEX_LIBRARIES apex)
    else()
        message(STATUS "APEX found via pkg-config")
        include_directories(${APEX_INCLUDE_DIRS})
        link_directories(${APEX_LIBRARY_DIRS})
    endif()
    
    add_definitions(-DENABLE_APEX)
endif()

# Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g -Wall")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -O3 -g")

include_directories(${STARPU_INCLUDE_DIRS})
link_directories(${STARPU_LIBRARY_DIRS})

# Add kernels executable
add_executable(starpu_adv_pipeline main.cpp kernels.cu)
target_link_libraries(starpu_adv_pipeline ${STARPU_LIBRARIES})


if(ENABLE_APEX)
    target_link_libraries(starpu_adv_pipeline ${APEX_LIBRARIES})
endif()

if(ENABLE_TASKSTUBS_APEX)
    # Static plugin must be forced via --whole-archive to register constructors
    target_link_libraries(starpu_adv_pipeline "-Wl,--whole-archive" ${TASKSTUBS_LIBRARIES} "-Wl,--no-whole-archive")
endif()
