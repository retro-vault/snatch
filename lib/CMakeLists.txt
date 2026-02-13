include(FetchContent)

# FreeType library (static)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://github.com/freetype/freetype.git
  GIT_TAG VER-2-13-3
)
FetchContent_MakeAvailable(freetype)

# STB library
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
)
FetchContent_MakeAvailable(stb)

add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE ${stb_SOURCE_DIR})

# Implementations as C sources (force LANGUAGE C just in case)
set(stb_image_impl ${CMAKE_CURRENT_BINARY_DIR}/stb_image_impl.c)
file(WRITE ${stb_image_impl}
"#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>\n")

set_source_files_properties(${stb_image_impl} PROPERTIES LANGUAGE C)

add_library(stb_image STATIC ${stb_image_impl})
target_include_directories(stb_image PUBLIC ${stb_SOURCE_DIR})

set(stb_image_write_impl ${CMAKE_CURRENT_BINARY_DIR}/stb_image_write_impl.c)
file(WRITE ${stb_image_write_impl}
"#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>\n")
set_source_files_properties(${stb_image_write_impl} PROPERTIES LANGUAGE C)

add_library(stb_image_write STATIC ${stb_image_write_impl})
target_include_directories(stb_image_write PUBLIC ${stb_SOURCE_DIR})
set_target_properties(stb_image_write PROPERTIES POSITION_INDEPENDENT_CODE ON)

# argparse library
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/cofyc/argparse.git
)
FetchContent_MakeAvailable(argparse)

add_library(argparse STATIC ${argparse_SOURCE_DIR}/argparse.c)
set_source_files_properties(${argparse_SOURCE_DIR}/argparse.c PROPERTIES LANGUAGE C)
target_include_directories(argparse PUBLIC ${argparse_SOURCE_DIR})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(argparse PRIVATE -Wall -Wextra -Wpedantic)
endif()

# GoogleTest library
# Tip: pin a release tag for reproducibility, e.g. v1.15.2
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
# gtest uses threads on Linux
set(gtest_disable_pthreads OFF CACHE BOOL "" FORCE)
# avoid overriding our compiler/linker flags on MSVC (harmless on Linux)
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)
# Exposes targets: GTest::gtest, GTest::gtest_main, GTest::gmock, GTest::gmock_main

# Build our own library in lib/snatch
add_subdirectory(snatch)
