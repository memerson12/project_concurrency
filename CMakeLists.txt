cmake_minimum_required(VERSION 3.20)
project(project_concurrency C)

set(CMAKE_C_STANDARD 11)

add_executable(project_concurrency
        generator.c
        scan.c
        scan_serial.c scan.c)
