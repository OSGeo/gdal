# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

macro (fuzzer_test_target _test _target)
  add_executable(${_target} ${_target}.cpp)
  target_link_libraries(${_target} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
  gdal_standard_includes(${_target})
  add_dependencies(${_target} ${GDAL_LIB_TARGET_NAME})
  add_test(NAME ${_test} COMMAND ${_target})
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
