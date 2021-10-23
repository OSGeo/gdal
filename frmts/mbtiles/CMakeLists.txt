add_gdal_driver(
  TARGET gdal_MBTiles
  SOURCES mbtilesdataset.cpp
  BUILTIN)
gdal_standard_includes(gdal_MBTiles)
target_include_directories(gdal_MBTiles PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/mvt)
if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(gdal_MBTiles PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
endif ()
if (HAVE_GEOS)
  target_compile_definitions(gdal_MBTiles PRIVATE -DHAVE_GEOS=1)
  target_include_directories(gdal_MBTiles PRIVATE ${GEOS_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET gdal_MBTiles LIBRARIES GEOS::GEOS)
endif ()
if (HAVE_SQLITE3)
  target_include_directories(gdal_MBTiles PRIVATE ${SQLite3_INCLUDE_DIRS})
  target_compile_definitions(gdal_MBTiles PRIVATE -DHAVE_SQLITE)
  gdal_target_link_libraries(TARGET gdal_MBTiles LIBRARIES SQLite::SQLite3)
endif ()
if (GDAL_USE_LIBJSONC_INTERNAL)
  target_include_directories(gdal_MBTiles PRIVATE $<TARGET_PROPERTY:libjson,SOURCE_DIR>)
else ()
  target_include_directories(gdal_MBTiles PRIVATE ${JSONC_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET gdal_MBTiles LIBRARIES ${JSONC_LIBRARIES})
endif ()
target_include_directories(
  gdal_MBTiles PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/geojson ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/gpkg
                       ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/sqlite)
