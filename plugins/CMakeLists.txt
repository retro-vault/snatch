function(add_snatch_plugin target)
  add_library(${target} MODULE ${ARGN})
  target_include_directories(${target}
    PRIVATE
      ${PROJECT_SOURCE_DIR}/include
  )
  target_compile_features(${target} PRIVATE cxx_std_20)

  # Keep plugin file names simple for lookup (e.g. dummy.so).
  set_target_properties(${target} PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/plugins"
  )

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  install(TARGETS ${target}
    LIBRARY DESTINATION ${SNATCH_PLUGIN_INSTALL_DIR}
  )
endfunction()

add_subdirectory(dummy)
add_subdirectory(png)
