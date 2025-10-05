file(GLOB_RECURSE SNATCH_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(snatch ${SNATCH_SOURCES})

target_include_directories(snatch
  PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(snatch
  PRIVATE
    stb_headers
    stb_image
    stb_truetype
    argparse
    pthread
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(snatch PRIVATE -Wall -Wextra -Wpedantic)
endif()
