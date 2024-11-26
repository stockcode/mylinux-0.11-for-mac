cmake_minimum_required(VERSION 3.23)
project(kernel)

add_library(kernel OBJECT
    init/main.c
    kernel/sched.c
        kernel/fork.c
        kernel/traps.c
        lib/cklib.c
        mm/memroy.c
        include/linux/kernel.h)

target_include_directories(kernel PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/asm"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/linux"
)