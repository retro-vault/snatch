# test/CMakeLists.txt

# 1) collect tests
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# 2) the test target
add_executable(snatch_tests ${TEST_SOURCES})
set_target_properties(snatch_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
target_link_libraries(snatch_tests PRIVATE libsnatch GTest::gtest_main pthread)
target_include_directories(snatch_tests
  PRIVATE
    "${PROJECT_SOURCE_DIR}/include"
)


# 3) test data handling (robust)
set(TEST_DATA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(TEST_DATA_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/data")

# Always ensure destination exists
add_custom_target(prepare_test_data ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_DATA_BIN_DIR}"
  COMMENT "Ensuring test data dir exists: ${TEST_DATA_BIN_DIR}"
  VERBATIM
)

# Only copy if the source folder exists (avoids first-run errors)
if(EXISTS "${TEST_DATA_SRC_DIR}")
  add_custom_command(TARGET prepare_test_data POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${TEST_DATA_SRC_DIR}" "${TEST_DATA_BIN_DIR}"
    COMMENT "Copying test data to ${TEST_DATA_BIN_DIR}"
    VERBATIM
  )
endif()

add_dependencies(snatch_tests prepare_test_data)

# Provide absolute path macro to tests
target_compile_definitions(snatch_tests
  PRIVATE TEST_DATA_DIR="${TEST_DATA_BIN_DIR}"
          SNATCH_BIN_PATH="${PROJECT_SOURCE_DIR}/bin/snatch"
          SNATCH_PLUGIN_DIR_PATH="${PROJECT_SOURCE_DIR}/bin/plugins"
)

# 4) register with CTest
include(GoogleTest)
gtest_discover_tests(snatch_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
