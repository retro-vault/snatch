# Collect sources in src/ (you can add subdirs later; this auto-picks new .cpp files)
file(GLOB_RECURSE SNATCH_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(snatch ${SNATCH_SOURCES})

# Include paths (src/ and include/)
target_include_directories(snatch
  PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/src"
)

# Linux bits (pthread often needed eventually; harmless if unused)
target_link_libraries(snatch PRIVATE pthread)

# Nice warnings for GCC/Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(snatch PRIVATE -Wall -Wextra -Wpedantic)
endif()