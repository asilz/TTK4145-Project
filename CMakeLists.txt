cmake_minimum_required(VERSION 3.22)
project(TTK4145)

set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O3")

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

add_executable(elevator)
add_subdirectory(src)

if(NOT DEFINED FLOOR_COUNT)
    set(FLOOR_COUNT 4)
endif()

if(NOT DEFINED ELEVATOR_COUNT)
    set(ELEVATOR_COUNT 3)
endif()

if(NOT DEFINED LOG_LEVEL)
    set(LOG_LEVEL 3)
endif()

target_compile_definitions(elevator PRIVATE FLOOR_COUNT=${FLOOR_COUNT} ELEVATOR_COUNT=${ELEVATOR_COUNT} LOG_LEVEL=${LOG_LEVEL})
target_compile_options(elevator PRIVATE -Wall -Werror=vla)
target_include_directories(elevator PUBLIC include)