# lib/snatch/CMakeLists.txt

# 1) grab every .cpp in this folder (and subfolders) for the library
file(GLOB_RECURSE LIBSNATCH_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_library(libsnatch STATIC ${LIBSNATCH_SOURCES})

# nice output name: libsnatch.a
set_target_properties(libsnatch PROPERTIES OUTPUT_NAME snatch)

# public headers live here
target_include_directories(libsnatch
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# C++ standard
target_compile_features(libsnatch PUBLIC cxx_std_20)

# link deps the lib uses
target_link_libraries(libsnatch
  PUBLIC argparse
  PRIVATE stb_image stb_truetype
)

# plugin manager (if compiled) needs dl on Linux
if(UNIX)
  target_link_libraries(libsnatch PRIVATE dl)
endif()

# warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(libsnatch PRIVATE -Wall -Wextra -Wpedantic)
endif()
