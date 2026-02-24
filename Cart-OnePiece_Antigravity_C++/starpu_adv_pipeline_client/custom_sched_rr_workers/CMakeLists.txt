cmake_minimum_required(VERSION 3.10)
project(starpu_sched_rr_workers C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(STARPU REQUIRED starpu-1.4)

add_library(starpu_sched_rr_workers SHARED rr_workers_sched.c)
target_include_directories(starpu_sched_rr_workers PRIVATE ${STARPU_INCLUDE_DIRS})
target_compile_options(starpu_sched_rr_workers PRIVATE ${STARPU_CFLAGS})
target_link_options(starpu_sched_rr_workers PRIVATE ${STARPU_LDFLAGS})

set_target_properties(starpu_sched_rr_workers PROPERTIES PREFIX "lib")
