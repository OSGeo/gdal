# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.


find_package(GTest 1.10.0)
if(GTest_FOUND)
  option(USE_EXTERNAL_GTEST "Compile against external GTest" ON)
else()
  option(USE_EXTERNAL_GTEST "Compile against external GTest" OFF)
endif()

if(USE_EXTERNAL_GTEST)

    if(NOT GTest_FOUND)
        message(FATAL_ERROR "External GTest >= 1.10.0 not found")
    endif()
    message(STATUS "Using external GTest")

    # CMake < 3.20.0 uses GTest::GTest
    # CMake >= 3.20 uses GTest::gtest
    add_library(gtest_for_gdal INTERFACE IMPORTED)
    if(NOT TARGET GTest::gtest)
        set_target_properties(gtest_for_gdal PROPERTIES
                              INTERFACE_LINK_LIBRARIES "GTest::GTest")
    else()
        set_target_properties(gtest_for_gdal PROPERTIES
                              INTERFACE_LINK_LIBRARIES "GTest::gtest")
    endif()

else()

    message(STATUS "Using internal GTest")

    #
    # Build Google Test
    #
    # Source https://github.com/google/googletest/blob/master/googletest/README.md
    # Download and unpack googletest at configure time
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/googletest/CMakeLists.txt.in
      ${CMAKE_CURRENT_BINARY_DIR}/googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
    if(result)
      message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
    if(result)
      message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif()
    # Prevent overriding the parent project's compiler/linker
    # settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    # Add googletest directly to our build. This defines
    # the gtest and gtest_main targets.
    #option(INSTALL_GTEST "Enable installation of googletest" OFF)
    add_subdirectory(
      ${CMAKE_CURRENT_BINARY_DIR}/googletest-src
      ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
      EXCLUDE_FROM_ALL)

    add_library(gtest_for_gdal ALIAS gtest)

endif()  # USE_EXTERNAL_GTEST


set(GDAL_ROOT_TEST_DIR "${PROJECT_SOURCE_DIR}/autotest")

add_executable(
  gdal_unit_test
  gdal_unit_test.cpp
  test_alg.cpp
  test_cpl.cpp
  test_gdal.cpp
  test_gdal_aaigrid.cpp
  test_gdal_dted.cpp
  test_gdal_gtiff.cpp
  test_gdal_pixelfn.cpp
  test_gdal_typetraits.cpp
  test_ogr.cpp
  test_ogr_organize_polygons.cpp
  test_ogr_geometry_stealing.cpp
  test_ogr_lgpl.cpp
  test_ogr_geos.cpp
  test_ogr_shape.cpp
  test_ogr_swq.cpp
  test_ogr_wkb.cpp
  test_osr.cpp
  test_osr_pci.cpp
  test_osr_ct.cpp
  test_osr_proj4.cpp
  test_triangulation.cpp
  test_utilities.cpp
  test_marching_squares_contour.cpp
  test_marching_squares_polygon.cpp
  test_marching_squares_square.cpp
  test_marching_squares_tile.cpp
  test_viewshed.cpp)

target_link_libraries(gdal_unit_test PRIVATE gtest_for_gdal)
target_link_libraries(gdal_unit_test PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
if(NOT MSVC AND CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(gdal_unit_test PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()
gdal_standard_includes(gdal_unit_test)
target_compile_options(gdal_unit_test PRIVATE ${GDAL_CXX_WARNING_FLAGS})
target_compile_definitions(gdal_unit_test PRIVATE -DGDAL_TEST_ROOT_DIR="${GDAL_ROOT_TEST_DIR}")
target_include_directories(
  gdal_unit_test PRIVATE $<TARGET_PROPERTY:appslib,SOURCE_DIR> $<TARGET_PROPERTY:gdal_vrt,SOURCE_DIR>)
if (GDAL_USE_SQLITE3)
  target_compile_definitions(gdal_unit_test PRIVATE -DHAVE_SQLITE3)
  target_include_directories(
      gdal_unit_test PRIVATE $<TARGET_PROPERTY:SQLite::SQLite3,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

target_compile_definitions(gdal_unit_test PRIVATE -DGDAL_TEST_ROOT_DIR="${GDAL_ROOT_TEST_DIR}")
if (GDAL_USE_GEOS)
  target_compile_definitions(gdal_unit_test PRIVATE -DHAVE_GEOS=1)
  target_link_libraries(gdal_unit_test PRIVATE ${GEOS_TARGET})
endif ()
if (GDAL_USE_CURL)
  target_compile_definitions(gdal_unit_test PRIVATE -DHAVE_CURL)
endif ()
target_compile_definitions(gdal_unit_test PRIVATE "-DPROJ_DB_TMPDIR=\"${CMAKE_CURRENT_BINARY_DIR}/proj_db_tmpdir\"" "-DPROJ_GRIDS_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/../proj_grids\"")

# gtest with lots of assertion can be very slow to build in optimized mode.
# As we don't need optimizations for that unit test, override it with -O0 for gcc-style compilers
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # -D_FORTIFY_SOURCE can't be used with -O0
    string(REPLACE "-D_FORTIFY_SOURCE=2" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
    target_compile_options(gdal_unit_test PRIVATE -O0)
endif()

add_executable(gdallimits gdallimits.c)
gdal_standard_includes(gdallimits)
target_link_libraries(gdallimits PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)

include(GdalSetRuntimeEnv)
gdal_set_runtime_env(TEST_ENV)

if (MINGW)
  list(APPEND TEST_ENV SKIP_MEM_INTENSIVE_TEST=YES)
endif ()

if (WIN32)
  # If running GDAL as a CustomBuild Command os MSBuild, "ERROR bla:" is considered as failing the job. This is rarely
  # the intended behavior
  list(APPEND TEST_ENV "CPL_ERROR_SEPARATOR=\\;")
endif ()

if (WIN32 OR APPLE)
  # Recoding tests in test_cpl.cpp fail on Windows and Mac
  list(APPEND TEST_ENV DO_NOT_FAIL_ON_RECODE_ERRORS=YES)
endif ()

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
if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
    add_dependencies(test-unit ${GDAL_LIB_TARGET_NAME} gdal_plugins)
endif()

include(GdalTestTarget)

add_library(main_gtest STATIC main_gtest.cpp)
target_link_libraries(main_gtest PRIVATE gtest_for_gdal $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
gdal_standard_includes(main_gtest)

gdal_test_target(testblockcache testblockcache.cpp)
target_link_libraries(testblockcache PRIVATE gtest_for_gdal main_gtest)

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

if ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(x86_64|AMD64)" AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND HAVE_SSE_AT_COMPILE_TIME)
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
    target_compile_options(testsse4_1 PRIVATE ${GDAL_SSSE3_FLAG} ${GDAL_SSE41_FLAG})
    target_compile_options(testavx2 PRIVATE ${GDAL_AVX2_FLAG})
    register_test_as_custom_target(test-ssse3 testssse3)
    register_test_as_custom_target(test-sse41 testsse4_1)
    register_test_as_custom_target(test-avx2 testavx2)
  endif ()
endif ()

macro (gdal_autotest_target _target testlabel _source)
  gdal_test_target(${_target} ${_source})
  register_test(${testlabel} ${_target} ${ARGN})
endmacro ()

function (gdal_gtest_target _target testlabel _source)
  gdal_autotest_target(${_target} ${testlabel} ${_source} ${ARGN})
  target_link_libraries(${_target} PRIVATE gtest_for_gdal main_gtest)
endfunction ()

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
    test-log
    test-deferred-plugin
)

gdal_gtest_target(testcopywords test-copy-words testcopywords.cpp)
gdal_gtest_target(testclosedondestroydm test-closed-on-destroy-DM testclosedondestroydm.cpp)
gdal_gtest_target(testthreadcond test-threaded-condition testthreadcond.cpp)
gdal_gtest_target(testvirtualmem test-virtual-memory testvirtualmem.cpp)
gdal_gtest_target(testblockcachewrite test-block-cache-write testblockcachewrite.cpp --debug ON)
gdal_gtest_target(testblockcachelimits test-block-cache-limit testblockcachelimits.cpp --debug ON)
gdal_gtest_target(testmultithreadedwriting test-multi-threaded-writing testmultithreadedwriting.cpp)
gdal_gtest_target(testdestroy test-destroy testdestroy.cpp)
gdal_autotest_target(test_include_from_c_file test-include-from-C-file test_include_from_c_file.c "")
gdal_autotest_target(test_c_include_from_cpp_file test-C-include-from-CPP-file test_c_include_from_cpp_file.cpp "")
gdal_gtest_target(bug1488 test-bug1488 bug1488.cpp)
gdal_gtest_target(testlog test-log testlog.cpp)
gdal_gtest_target(test_deferred_plugin test-deferred-plugin test_deferred_plugin.cpp)

if (GDAL_ENABLE_DRIVER_JPEG_PLUGIN)
    target_compile_definitions(test_deferred_plugin PRIVATE -DJPEG_PLUGIN)
endif()

if (UNIX)
  list(APPEND QUICKTEST_LIST test-osr-set-proj-search-paths)
  gdal_gtest_target(test_osr_set_proj_search_paths test-osr-set-proj-search-paths test_osr_set_proj_search_paths.cpp)
endif()

if (UNIX)
  gdal_gtest_target(proj_with_fork test-proj-with-fork proj_with_fork.cpp)
  list(APPEND QUICKTEST_LIST test-proj-with-fork)
endif ()

if (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR)
  add_custom_target(quicktest DEPENDS ${QUICKTEST_LIST})
endif ()

foreach (_test IN LISTS QUICKTEST_LIST)
  set_tests_properties(${_test} PROPERTIES LABELS "quicktest")
endforeach ()
