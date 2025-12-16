cmake_minimum_required(VERSION 3.15)
project(poker_env LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# Python
find_package(Python REQUIRED COMPONENTS Interpreter Development)

# Fetch pybind11
include(FetchContent)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.12.0
)

FetchContent_MakeAvailable(pybind11)

# Build module
pybind11_add_module(poker_env
    src/game_env.cpp
    src/game_logic.cpp
)

target_include_directories(poker_env PRIVATE src)

if (MSVC)
    target_compile_options(poker_env PRIVATE /O2)
else()
    target_compile_options(poker_env PRIVATE -O3 -march=native)
endif()
