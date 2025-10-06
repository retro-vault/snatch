cmake_minimum_required(VERSION 3.16)
project(snatch LANGUAGES C CXX)

# make sure both languages are initialized before subdirs
enable_language(C)
enable_language(CXX)

if(NOT UNIX)
  message(FATAL_ERROR "Linux only build.")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# C/C++
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Executables go to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# public headers
include_directories("${PROJECT_SOURCE_DIR}/include")

# enable CTest / testing
include(CTest)       # defines BUILD_TESTING option and enable_testing()

# subdirs
add_subdirectory(lib)
add_subdirectory(src)

# add tests dir only if tests enabled
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
