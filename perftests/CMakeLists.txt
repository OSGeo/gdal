include(GdalTestTarget)

gdal_test_target(testperfcopywords testperfcopywords.cpp)
gdal_test_target(testperfdeinterleave testperfdeinterleave.cpp)

add_executable(bench_ogr_batch bench_ogr_batch.cpp)
gdal_standard_includes(bench_ogr_batch)
target_link_libraries(bench_ogr_batch PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)

add_executable(bench_ogr_c_api bench_ogr_c_api.cpp)
gdal_standard_includes(bench_ogr_c_api)
target_link_libraries(bench_ogr_c_api PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
