function(add_snatch_plugin target)
  add_library(${target} MODULE ${ARGN})
  target_include_directories(${target}
    PRIVATE
      ${PROJECT_SOURCE_DIR}/include
      ${PROJECT_SOURCE_DIR}/plugins/include
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
add_subdirectory(ttf_extractor)
add_subdirectory(image_extractor)
add_subdirectory(image_passthrough_extractor)
add_subdirectory(dither_1bpp_transform)
add_subdirectory(fzx_transform)
add_subdirectory(partner_tiny_transform)
add_subdirectory(partner_bitmap_transform)
add_subdirectory(partner_asm)
add_subdirectory(partner_bitmap_asm)
add_subdirectory(raw_bin)
add_subdirectory(raw_c)
