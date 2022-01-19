add_gdal_driver(
  TARGET ogr_MVT
  SOURCES mvt_tile.cpp mvt_tile.h mvtutils.cpp mvtutils.h ogrmvtdataset.cpp
  BUILTIN)
gdal_standard_includes(ogr_MVT)
target_include_directories(ogr_MVT PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/osm)
if (GDAL_USE_GEOS)
  target_compile_definitions(ogr_MVT PRIVATE -DHAVE_GEOS=1)
  gdal_target_link_libraries(TARGET ogr_MVT LIBRARIES GEOS::GEOS)
endif ()

if (GDAL_USE_SQLITE3)
  target_compile_definitions(ogr_MVT PRIVATE -DHAVE_SQLITE)
  gdal_target_link_libraries(TARGET ogr_MVT LIBRARIES SQLite::SQLite3)
endif ()

add_executable(mvt_tile_test EXCLUDE_FROM_ALL mvt_tile_test.cpp)
gdal_standard_includes(mvt_tile_test)
target_include_directories(mvt_tile_test PRIVATE $<TARGET_PROPERTY:ogr_MVT,SOURCE_DIR>
                                                 $<TARGET_PROPERTY:ogr_OSM,SOURCE_DIR>)
target_link_libraries(mvt_tile_test PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
