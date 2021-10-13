# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

set(GDAL_ROOT_TEST_DIR "${CMAKE_SOURCE_DIR}/autotest")

if (NOT CMAKE_CROSSCOMPILING)
  if (LINUX)
    add_library(vsipreload MODULE ${CMAKE_SOURCE_DIR}/gdal/port/vsipreload.cpp)
    target_link_libraries(vsipreload PRIVATE $<TARGET_NAME:gdal>)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
      set_property(SOURCE ${CMAKE_SOURCE_DIR}/gdal/port/vsipreload.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
    endif ()
  endif ()
endif ()

add_executable(
  gdal_unit_test
  tut/tut_gdal.cpp
  gdal_unit_test.cpp
  test_alg.cpp
  test_cpl.cpp
  test_gdal.cpp
  test_gdal_aaigrid.cpp
  test_gdal_dted.cpp
  test_gdal_gtiff.cpp
  test_ogr.cpp
  test_ogr_geos.cpp
  test_ogr_shape.cpp
  test_osr.cpp
  test_osr_pci.cpp
  test_osr_ct.cpp
  test_osr_proj4.cpp
  test_triangulation.cpp
  test_marching_squares_contour.cpp
  test_marching_squares_polygon.cpp
  test_marching_squares_square.cpp
  test_marching_squares_tile.cpp)
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(TARGET gdal_unit_test PROPERTY UNITY_BUILD OFF)
endif ()

target_link_libraries(gdal_unit_test PRIVATE $<TARGET_NAME:gdal> $<TARGET_NAME:cpl>)
add_dependencies(gdal_unit_test gdal)
gdal_standard_includes(gdal_unit_test)
target_compile_definitions(gdal_unit_test PRIVATE -DGDAL_TEST_ROOT_DIR="${GDAL_ROOT_TEST_DIR}")
target_include_directories(
  gdal_unit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tut
                         $<TARGET_PROPERTY:appslib,SOURCE_DIR> $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR>)
target_compile_definitions(gdal_unit_test PRIVATE -DGDAL_TEST_ROOT_DIR="${GDAL_ROOT_TEST_DIR}")

set(TEST_ENV)
if (MINGW)
  list(APPEND TEST_ENV SKIP_MEM_INTENSIVE_TEST=YES)
endif ()
if (MSVC)
  set(ENVSEP ";")
else ()
  set(ENVSEP ":")
endif ()
# FIXME: better handling of output path w/o hard coded. set(GDAL_OUTPUT_DIR
# "$<TARGET_PROPERTY:gdal,LIBRARY_OUTPUT_DIRECTORY>")
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/gdal" GDAL_OUTPUT_DIR)
# set(GDAL_DRIVER_PATH "$<TARGET_PROPERTY:gdal,PLUGIN_OUTPUT_DIR")
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/gdal/gdalplugins" GDAL_DRIVER_PATH)
file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/gdal/data" GDAL_DATA)
list(APPEND TEST_ENV PATH="${GDAL_OUTPUT_DIR}${ENVSEP}$ENV{PATH}"
     LD_LIBRARY_PATH="${GDAL_OUTPUT_DIR}:$ENV{LD_LIBRARY_PATH}" GDAL_DRIVER_PATH="${GDAL_DRIVER_PATH}"
     GDAL_DATA="${GDAL_DATA}")
add_custom_target(
  test-unit
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:gdal_unit_test>
  DEPENDS gdal_unit_test)

macro (gdal_test_target _target _source)
  add_executable(${_target} EXCLUDE_FROM_ALL ${_source})
  target_link_libraries(${_target} PRIVATE $<TARGET_NAME:gdal>)
  gdal_standard_includes(${_target})
  target_include_directories(${_target} PRIVATE $<TARGET_PROPERTY:appslib,SOURCE_DIR>)
  target_compile_definitions(${_target} PRIVATE -DGDAL_TEST_ROOT_DIR="${GDAL_ROOT_TEST_DIR}")
  add_dependencies(${_target} gdal)
endmacro ()

gdal_test_target(testblockcache testblockcache.cpp)
add_custom_target(
  test-block-cache-1
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testblockcache> -check -co TILED=YES --debug TEST,LOCK
          -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES
  DEPENDS testblockcache
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-block-cache-2
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testblockcache> -check -co TILED=YES --debug TEST,LOCK
          -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES --config GDAL_RB_LOCK_TYPE SPIN
  DEPENDS testblockcache
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-block-cache-3
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testblockcache> -check -co TILED=YES -migrate
  DEPENDS testblockcache
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-block-cache-4
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testblockcache> -check -memdriver
  DEPENDS testblockcache
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-block-cache-5
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testblockcache> --config GDAL_BAND_BLOCK_CACHE HASHSET
          -check -co TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES
  DEPENDS testblockcache
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-block-cache-6
  COMMAND
    ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testblockcache> --config GDAL_BAND_BLOCK_CACHE HASHSET -check -co
    TILED=YES --debug TEST,LOCK -loops 3 --config GDAL_RB_LOCK_DEBUG_CONTENTION YES --config GDAL_RB_LOCK_TYPE SPIN
  DEPENDS testblockcache
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})

gdal_test_target(testsse1 testsse.cpp)
gdal_test_target(testsse2 testsse.cpp)
gdal_test_target(testsse3 testsse.cpp)
gdal_test_target(testsse4 testsse.cpp)
gdal_test_target(testsse5 testsse.cpp)
target_compile_options(testsse2 PRIVATE -mssse3)
target_compile_options(testsse3 PRIVATE -mssse3 -msse4.1)
target_compile_options(testsse4 PRIVATE -mavx2)
target_compile_definitions(testsse5 PRIVATE -DUSE_SSE2_EMULATION)
add_custom_target(
  test-sse1
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testsse1>
  DEPENDS testsse1
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-sse2
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testsse2>
  DEPENDS testsse2
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-sse3
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testsse3>
  DEPENDS testsse3
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-sse4
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testsse4>
  DEPENDS testsse4
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
add_custom_target(
  test-sse5
  COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:testsse5>
  DEPENDS testsse5
  WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})

macro (gdal_autotest_target _target testlabel _source options)
  if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    gdal_test_target(${_target} ${_source})
    add_custom_target(
      ${testlabel}
      COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:${_target}> ${options}
      WORKING_DIRECTORY ${GDAL_ROOT_TEST_DIR})
  endif ()
endmacro ()

gdal_autotest_target(testcopywords test-copy-words testcopywords.cpp "")
gdal_autotest_target(testclosedondestroydm test-closed-on-destroy-DM testclosedondestroydm.cpp "")
gdal_autotest_target(testthreadcond test-threaded-condition testthreadcond.cpp "")
gdal_autotest_target(testvirtualmem test-virtual-memory testvirtualmem.cpp "")
gdal_autotest_target(testblockcachewrite test-block-cache-write testblockcachewrite.cpp "--debug ON")
gdal_autotest_target(testblockcachelimits test-block-cache-limit testblockcachelimits.cpp "--debug ON")
gdal_autotest_target(testmultithreadedwriting test-multi-threaded-writing testmultithreadedwriting.cpp "")
gdal_autotest_target(testdestroy test-destroy testdestroy.cpp "")
gdal_autotest_target(test_include_from_c_file test-include-from-C-file test_include_from_c_file.c "")
gdal_autotest_target(test_c_include_from_cpp_file test-C-include-from-CPP-file test_c_include_from_cpp_file.cpp "")
gdal_autotest_target(testperfcopywords test-performance-copy-words testperfcopywords.cpp "")

add_custom_target(
  quicktest
  DEPENDS test-unit
          test-block-cache-1
          test-block-cache-2
          test-block-cache-3
          test-block-cache-4
          test-block-cache-5
          test-block-cache-6
          test-copy-words
          test-closed-on-destroy-DM
          test-threaded-condition
          test-virtual-memory
          test-block-cache-write
          test-block-cache-limit
          test-multi-threaded-writing
          test-destroy)
