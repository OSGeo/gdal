add_gdal_driver(
  TARGET gdal_MBTiles
  SOURCES mbtilesdataset.cpp
  BUILTIN)
gdal_standard_includes(gdal_MBTiles)
target_include_directories(gdal_MBTiles PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/mvt)
if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(gdal_MBTiles PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
endif ()
if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(gdal_MBTiles PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
endif ()

if (GDAL_USE_GEOS)
  target_compile_definitions(gdal_MBTiles PRIVATE -DHAVE_GEOS=1)
  gdal_target_link_libraries(TARGET gdal_MBTiles LIBRARIES GEOS::GEOS)
endif ()

target_compile_definitions(gdal_MBTiles PRIVATE -DHAVE_SQLITE)
gdal_target_link_libraries(TARGET gdal_MBTiles LIBRARIES SQLite::SQLite3)

if (NOT GDAL_USE_LIBJSONC_INTERNAL)
  gdal_target_link_libraries(TARGET gdal_MBTiles LIBRARIES JSONC::JSONC)
endif ()
target_include_directories(
  gdal_MBTiles PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/geojson ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/gpkg
                       ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/sqlite)
