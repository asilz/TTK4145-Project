cmake_minimum_required(VERSION 3.22)
project(TTK4145)

set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O3")

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

add_executable(master)
add_executable(slave)
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

if(NOT DEFINED SLAVE_PORT)
    set(SLAVE_PORT 20000)
endif()

if(NOT DEFINED MASTER_PORT)
    set(MASTER_PORT 17532)
endif()

target_compile_definitions(master PRIVATE FLOOR_COUNT=${FLOOR_COUNT} ELEVATOR_COUNT=${ELEVATOR_COUNT} LOG_LEVEL=${LOG_LEVEL} SLAVE_PORT=${SLAVE_PORT} MASTER_PORT=${MASTER_PORT})
target_compile_options(master PRIVATE -Wall -Werror=vla)
target_include_directories(master PUBLIC include)

target_compile_definitions(slave PRIVATE FLOOR_COUNT=${FLOOR_COUNT} ELEVATOR_COUNT=${ELEVATOR_COUNT} LOG_LEVEL=${LOG_LEVEL} SLAVE_PORT=${SLAVE_PORT} MASTER_PORT=${MASTER_PORT})
target_compile_options(slave PRIVATE -Wall -Werror=vla)
target_include_directories(slave PUBLIC include)