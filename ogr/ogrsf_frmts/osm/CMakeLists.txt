# referred from ogr_MVT
add_gdal_driver(
  TARGET ogr_OSM
  SOURCES ogrosmdatasource.cpp ogrosmdriver.cpp ogrosmlayer.cpp osm_parser.cpp
  BUILTIN)
gdal_standard_includes(ogr_OSM)
target_include_directories(ogr_OSM PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                                           ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/sqlite)
if (GDAL_USE_EXPAT)
  target_compile_definitions(ogr_OSM PRIVATE -DHAVE_EXPAT=1)
  gdal_target_link_libraries(ogr_OSM PRIVATE ${EXPAT_TARGET})
endif ()
if (GDAL_USE_SQLITE3)
  gdal_target_link_libraries(ogr_OSM PRIVATE SQLite::SQLite3)
endif ()

if (BROKEN)
  add_executable(osm2osm EXCLUDE_FROM_ALL osm2osm.cpp)
  gdal_standard_includes(osm2osm)
  target_link_libraries(osm2osm PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}> ogr_OSM PROJ::proj)
  if (GDAL_USE_EXPAT)
    target_link_libraries(osm2osm PRIVATE ${EXPAT_TARGET})
  endif ()
  if (GDAL_USE_SQLITE3)
    target_sources(osm2osm PRIVATE ../sqlite/ogrsqlitevfs.cpp ../sqlite/ogrsqliteexecutesql.cpp)
    target_link_libraries(osm2osm PRIVATE SQLite::SQLite3)
  endif ()
endif ()
