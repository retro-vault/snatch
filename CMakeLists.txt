cmake_minimum_required(VERSION 3.16)

# Project
project(snatch LANGUAGES CXX)

# Linux-only sanity guard
if(NOT UNIX)
  message(FATAL_ERROR "This project is intended for Linux/Unix (Ubuntu).")
endif()

# Prefer debug by default (good for F5 debugging)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Use Makefiles (picked at configure time). NOTE: You must configure with:
#   cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug
# CMake cannot force the generator from within CMakeLists.

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Put all runtime outputs (executables) in bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# Helpful for clang-tidy/IDE tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Top-level include dir for your external plugin API header(s)
include_directories("${PROJECT_SOURCE_DIR}/include")

# Add sources
add_subdirectory(src)
