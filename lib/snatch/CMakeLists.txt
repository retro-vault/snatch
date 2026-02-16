# lib/snatch/CMakeLists.txt

# 1) grab every .cpp in this folder (and subfolders) for the library
file(GLOB_RECURSE LIBSNATCH_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

# Build glyph algorithms as a standalone static library.
set(SNATCH_ALGO_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/glyph_algorithms.cpp"
)
add_library(snatch_algorithms STATIC ${SNATCH_ALGO_SOURCES})
target_include_directories(snatch_algorithms
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_features(snatch_algorithms PUBLIC cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(snatch_algorithms PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Keep algorithm objects out of libsnatch to avoid duplicate compilation/symbols.
list(REMOVE_ITEM LIBSNATCH_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/glyph_algorithms.cpp"
)

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
if(TARGET freetype)
  set(SNATCH_FREETYPE_TARGET freetype)
elseif(TARGET Freetype::Freetype)
  set(SNATCH_FREETYPE_TARGET Freetype::Freetype)
else()
  message(FATAL_ERROR "FreeType target not found after FetchContent_MakeAvailable(freetype)")
endif()

target_link_libraries(libsnatch
  PUBLIC argparse
  PRIVATE snatch_algorithms stb_image ${SNATCH_FREETYPE_TARGET}
)

# plugin manager (if compiled) needs dl on Linux
if(UNIX)
  target_link_libraries(libsnatch PRIVATE dl)
endif()

# warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(libsnatch PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS libsnatch
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(TARGETS snatch_algorithms
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
