# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

set(TEST_ENV)
# Set PATH / LD_LIBRARY_PATH
set(GDAL_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${GDAL_LIB_TARGET_NAME}>>")
if (WIN32)
  # If running GDAL as a CustomBuild Command os MSBuild, "ERROR bla:" is considered as failing the job. This is rarely
  # the intended behavior
  list(APPEND TEST_ENV "CPL_ERROR_SEPARATOR=\\;")

  string(REPLACE ";" "\\;" PATH_ESCAPED "$ENV{PATH}")
  list(APPEND TEST_ENV "PATH=${GDAL_OUTPUT_DIR}\\;${PATH_ESCAPED}")
else ()
  list(APPEND TEST_ENV "LD_LIBRARY_PATH=${GDAL_OUTPUT_DIR}:$ENV{LD_LIBRARY_PATH}")
endif ()

macro (fuzzer_test_target _test _target)
  add_executable(${_target} ${_target}.cpp)
  target_link_libraries(${_target} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
  gdal_standard_includes(${_target})
  add_dependencies(${_target} ${GDAL_LIB_TARGET_NAME})
  add_test(NAME ${_test} COMMAND ${_target})
  set_property(TEST ${_test} PROPERTY ENVIRONMENT "${TEST_ENV}")
endmacro ()

fuzzer_test_target(test-fuzzer-gdal test_gdal_fuzzer)
fuzzer_test_target(test-fuzzer-ogr test_ogr_fuzzer)
fuzzer_test_target(test-fuzzer-osr_set_from_user_input test_osr_set_from_user_input_fuzzer)
fuzzer_test_target(test-fuzzer-wkb_import test_wkb_import_fuzzer)
fuzzer_test_target(test-fuzzer-wkt_import test_wkt_import_fuzzer)

add_custom_target(
  fuzzer_check
  COMMAND ${CMAKE_CTEST_COMMAND} -R '^test-fuzzer-.*'
  DEPENDS test_gdal_fuzzer test_ogr_fuzzer test_osr_set_from_user_input_fuzzer test_wkb_import_fuzzer
          test_wkt_import_fuzzer)
