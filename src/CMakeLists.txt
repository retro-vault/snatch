# src/CMakeLists.txt

# Grab ONLY app sources here (ideally just main.cpp living under src/)
file(GLOB_RECURSE SNATCH_APP_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(snatch ${SNATCH_APP_SOURCES})

# The app should only need the public API
target_include_directories(snatch
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link just our core static lib; it already links third-party deps
target_link_libraries(snatch
  PRIVATE
    libsnatch
)

target_compile_definitions(snatch
  PRIVATE
    SNATCH_DEFAULT_PLUGIN_DIR="${CMAKE_INSTALL_FULL_LIBDIR}/snatch/plugins"
)

target_compile_features(snatch PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(snatch PRIVATE -Wall -Wextra -Wpedantic)
endif()

file(GLOB SNATCH_TEST_TTF_FONTS CONFIGURE_DEPENDS
     "${PROJECT_SOURCE_DIR}/test/data/*.ttf")

foreach(font_file ${SNATCH_TEST_TTF_FONTS})
  add_custom_command(TARGET snatch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${font_file}" "${PROJECT_SOURCE_DIR}/bin/"
    COMMENT "Copying TTF fixture to bin: ${font_file}"
    VERBATIM
  )
endforeach()

install(TARGETS snatch
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
