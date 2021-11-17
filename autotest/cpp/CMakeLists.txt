# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

set(GDAL_ROOT_TEST_DIR "${PROJECT_SOURCE_DIR}/autotest")

if (NOT CMAKE_CROSSCOMPILING)
  if (LINUX)
    add_library(vsipreload MODULE ${PROJECT_SOURCE_DIR}/port/vsipreload.cpp)
    target_link_libraries(vsipreload PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
      set_property(SOURCE ${PROJECT_SOURCE_DIR}/port/vsipreload.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
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

target_link_libraries(gdal_unit_test PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
add_dependencies(gdal_unit_test ${GDAL_LIB_TARGET_NAME} gdal_plugins)
gdal_standard_includes(gdal_unit_test)
target_compile_options(gdal_unit_test PRIVATE ${GDAL_CXX_WARNING_FLAGS})
target_compile_definitions(gdal_unit_test PRIVATE -DGDAL_TEST_ROOT_DIR="${GDAL_ROOT_TEST_DIR}")
target_include_directories(
  gdal_unit_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tut
                         $<TARGET_PROPERTY:appslib,SOURCE_DIR> $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR>)
target_compile_definitions(gdal_unit_test PRIVATE -DGDAL_TEST_ROOT_DIR="${GDAL_ROOT_TEST_DIR}")

set(TEST_ENV)
if (MINGW)
  list(APPEND TEST_ENV SKIP_MEM_INTENSIVE_TEST=YES)
endif ()

# Set PATH / LD_LIBRARY_PATH
set(GDAL_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${GDAL_LIB_TARGET_NAME}>>")
if (WIN32)
  # If running GDAL as a CustomBuild Command os MSBuild, "ERROR bla:" is considered as failing the job. This is rarely
  # the intended behavior
  list(APPEND TEST_ENV "CPL_ERROR_SEPARATOR=\\;")

  string(REPLACE ";" "\\;" PATH_ESCAPED "$ENV{PATH}")
  list(APPEND TEST_ENV "PATH=${GDAL_OUTPUT_DIR}\\;${PATH_ESCAPED}")
  # Recoding tests in test_cpl.cpp fail on Windows
  list(APPEND TEST_ENV DO_NOT_FAIL_ON_RECODE_ERRORS=YES)
else ()
  list(APPEND TEST_ENV "LD_LIBRARY_PATH=${GDAL_OUTPUT_DIR}:$ENV{LD_LIBRARY_PATH}")
endif ()

# Set GDAL_DRIVER_PATH We request the TARGET_FILE_DIR of one of the plugins, since the PLUGIN_OUTPUT_DIR will not
# contain the \Release suffix with MSVC generator
get_property(PLUGIN_MODULES GLOBAL PROPERTY PLUGIN_MODULES)
list(LENGTH PLUGIN_MODULES PLUGIN_MODULES_LENGTH)
if (PLUGIN_MODULES_LENGTH GREATER_EQUAL 1)
  list(GET PLUGIN_MODULES 0 FIRST_TARGET)
  set(PLUGIN_OUTPUT_DIR "$<SHELL_PATH:$<TARGET_FILE_DIR:${FIRST_TARGET}>>")
  list(APPEND TEST_ENV "GDAL_DRIVER_PATH=${PLUGIN_OUTPUT_DIR}")
else ()
  # if no plugins are configured, set up a dummy path, to avoid loading installed plugins (from a previous install
  # execution) that could have a different ABI
  list(APPEND TEST_ENV "GDAL_DRIVER_PATH=dummy")
endif ()

# Set GDAL_DATA
file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/data" GDAL_DATA)
list(APPEND TEST_ENV "GDAL_DATA=${GDAL_DATA}")

macro (register_test_as_custom_target _test_name _binary_name)
  if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    add_custom_target(
      ${_test_name}
      COMMAND ${CMAKE_COMMAND} -E env ${TEST_ENV} $<TARGET_FILE:${_binary_name}> ${ARGN}
      DEPENDS ${_binary_name})
  endif ()
endmacro ()

macro (register_test _test_name _binary_name)
  register_test_as_custom_target(${_test_name} ${_binary_name} ${ARGN})
  add_test(NAME ${_test_name} COMMAND ${_binary_name} ${ARGN})
  set_property(TEST ${_test_name} PROPERTY ENVIRONMENT "${TEST_ENV}")
endmacro ()

register_test(test-unit gdal_unit_test)

macro (gdal_test_target _target _source)
  add_executable(${_target} ${_source})
  target_link_libraries(${_target} PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
  gdal_standard_includes(${_target})
  target_compile_options(${_target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${GDAL_CXX_WARNING_FLAGS}>
                                            $<$<COMPILE_LANGUAGE:C>:${GDAL_C_WARNING_FLAGS}>)
  target_include_directories(${_target} PRIVATE $<TARGET_PROPERTY:appslib,SOURCE_DIR>)
  target_compile_definitions(${_target} PRIVATE -DGDAL_TEST_ROOT_DIR="${GDAL_ROOT_TEST_DIR}")
  add_dependencies(${_target} ${GDAL_LIB_TARGET_NAME} gdal_plugins)
endmacro ()

gdal_test_target(testblockcache testblockcache.cpp)

register_test(
  test-block-cache-1
  testblockcache
  -check
  -co
  TILED=YES
  --debug
  TEST,LOCK
  -loops
  3
  --config
  GDAL_RB_LOCK_DEBUG_CONTENTION
  YES)
register_test(
  test-block-cache-2
  testblockcache
  -check
  -co
  TILED=YES
  --debug
  TEST,LOCK
  -loops
  3
  --config
  GDAL_RB_LOCK_DEBUG_CONTENTION
  YES
  --config
  GDAL_RB_LOCK_TYPE
  SPIN)
register_test(test-block-cache-3 testblockcache -check -co TILED=YES -migrate)
register_test(test-block-cache-4 testblockcache -check -memdriver)
register_test(
  test-block-cache-5
  testblockcache
  --config
  GDAL_BAND_BLOCK_CACHE
  HASHSET
  -check
  -co
  TILED=YES
  --debug
  TEST,LOCK
  -loops
  3
  --config
  GDAL_RB_LOCK_DEBUG_CONTENTION
  YES)
register_test(
  test-block-cache-6
  testblockcache
  --config
  GDAL_BAND_BLOCK_CACHE
  HASHSET
  -check
  -co
  TILED=YES
  --debug
  TEST,LOCK
  -loops
  3
  --config
  GDAL_RB_LOCK_DEBUG_CONTENTION
  YES
  --config
  GDAL_RB_LOCK_TYPE
  SPIN)

if ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(x86_64|AMD64)" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  gdal_test_target(testsse2 testsse.cpp)
  gdal_test_target(testsse2_emulation testsse.cpp)
  target_compile_definitions(testsse2_emulation PRIVATE -DUSE_SSE2_EMULATION -DNO_WARN_USE_SSE2_EMULATION)
  register_test(test-sse2 testsse2)
  register_test(test-sse2-emulation testsse2_emulation)

  if (HAVE_AVX_AT_COMPILE_TIME)
    # Do not include in ctest as we aren't sure the target machine support this
    gdal_test_target(testssse3 testsse.cpp)
    gdal_test_target(testsse4_1 testsse.cpp)
    gdal_test_target(testavx2 testsse.cpp)
    target_compile_options(testssse3 PRIVATE ${GDAL_SSSE3_FLAG})
    target_compile_options(testsse4_1 PRIVATE ${GDAL_SSSE3_FLAG} ${GDAL_SSSE41_FLAG})
    target_compile_options(testavx2 PRIVATE ${GDAL_AVX2_FLAG})
    register_test_as_custom_target(test-ssse3 testssse3)
    register_test_as_custom_target(test-sse41 testsse4_1)
    register_test_as_custom_target(test-avx2 testavx2)
  endif ()
endif ()

macro (gdal_autotest_target _target testlabel _source options)
  gdal_test_target(${_target} ${_source})
  register_test(${testlabel} ${_target} ${options})
endmacro ()

set(QUICKTEST_LIST
    test-unit
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
    test-destroy
    test-bug1488
    test-log)

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
gdal_autotest_target(bug1488 test-bug1488 bug1488.cpp "")
gdal_autotest_target(testlog test-log testlog.cpp "")
if (UNIX)
  gdal_autotest_target(proj_with_fork test-proj-with-fork proj_with_fork.cpp "")
  list(APPEND QUICKTEST_LIST test-proj-with-fork)
endif ()

if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
  add_custom_target(quicktest DEPENDS ${QUICKTEST_LIST})
endif ()

foreach (_test IN LISTS QUICKTEST_LIST)
  set_tests_properties(${_test} PROPERTIES LABELS "quicktest")
endforeach ()
