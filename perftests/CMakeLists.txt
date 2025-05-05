include(GdalTestTarget)

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

gdal_test_target(testperfcopywords FILES testperfcopywords.cpp)
gdal_test_target(testperfdeinterleave FILES testperfdeinterleave.cpp)

add_executable(bench_ogr_batch bench_ogr_batch.cpp)
gdal_standard_includes(bench_ogr_batch)
target_link_libraries(bench_ogr_batch PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)

add_executable(bench_ogr_c_api bench_ogr_c_api.cpp)
gdal_standard_includes(bench_ogr_c_api)
target_link_libraries(bench_ogr_c_api PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)

gdal_test_target(testperf_gdal_minmax_element FILES testperf_gdal_minmax_element.cpp)
if (GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS)
  target_compile_definitions(testperf_gdal_minmax_element PRIVATE -DUSE_NEON_OPTIMIZATIONS)
endif()
add_test(NAME testperf_gdal_minmax_element COMMAND testperf_gdal_minmax_element)
set_property(TEST testperf_gdal_minmax_element PROPERTY ENVIRONMENT "${TEST_ENV}")

gdal_test_target(testperftranspose FILES testperftranspose.cpp)
if (HAVE_SSSE3_AT_COMPILE_TIME)
  target_compile_definitions(testperftranspose PRIVATE -DHAVE_SSSE3_AT_COMPILE_TIME)
endif()
add_test(NAME testperftranspose COMMAND testperftranspose)
set_property(TEST testperftranspose PROPERTY ENVIRONMENT "${TEST_ENV}")
