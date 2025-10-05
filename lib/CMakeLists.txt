include(FetchContent)

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

set(stb_truetype_impl ${CMAKE_CURRENT_BINARY_DIR}/stb_truetype_impl.c)
file(WRITE ${stb_truetype_impl}
"#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>\n")

set_source_files_properties(${stb_image_impl} ${stb_truetype_impl} PROPERTIES LANGUAGE C)

add_library(stb_image STATIC ${stb_image_impl})
target_include_directories(stb_image PUBLIC ${stb_SOURCE_DIR})

add_library(stb_truetype STATIC ${stb_truetype_impl})
target_include_directories(stb_truetype PUBLIC ${stb_SOURCE_DIR})

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
