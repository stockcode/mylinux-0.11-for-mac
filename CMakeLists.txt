cmake_minimum_required(VERSION 3.24)
project(kernel)
enable_language(ASM_NASM)

add_library(kernel OBJECT
    init/main.c
    kernel/sched.c
        kernel/fork.c
        kernel/traps.c
        kernel/vsprintf.c
        lib/string.c
        mm/memory.c
        fs/read_write.c
        fs/char_dev.c
        kernel/chr_drv/tty_io.c
        kernel/chr_drv/console.c
        include/fcntl.h
        include/time.h
)

target_include_directories(kernel PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/asm"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/linux"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/sys"
)