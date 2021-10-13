# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

enable_testing()

macro (fuzzer_test_target _test _target)
  add_executable(${_target} EXCLUDE_FROM_ALL ${_target}.cpp)
  target_link_libraries(${_target} PRIVATE $<TARGET_NAME:gdal>)
  gdal_standard_includes(${_target})
  add_dependencies(${_target} gdal)
  add_test(NAME ${_test} COMMAND ${_target})
endmacro ()

fuzzer_test_target(test-fuzzer-gdal test_gdal_fuzzer)
fuzzer_test_target(test-fuzzer-ogr test_ogr_fuzzer)
fuzzer_test_target(test-fuzzer-osr_set_from_user_input test_osr_set_from_user_input_fuzzer)
fuzzer_test_target(test-fuzzer-wkb_import test_wkb_import_fuzzer)
fuzzer_test_target(test-fuzzer-wkt_import test_wkt_import_fuzzer)

get_property(_plugins GLOBAL PROPERTY PLUGIN_MODULES)
add_custom_target(
  fuzzer_check
  COMMAND ${CMAKE_CTEST_COMMAND} -R '^test-fuzzer-.*'
  DEPENDS ${_plugins} test_gdal_fuzzer test_ogr_fuzzer test_osr_set_from_user_input_fuzzer test_wkb_import_fuzzer
          test_wkt_import_fuzzer)
