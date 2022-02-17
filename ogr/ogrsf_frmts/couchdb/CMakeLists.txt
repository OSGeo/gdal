add_gdal_driver(
  TARGET ogr_CouchDB
  SOURCES ogr_couchdb.h ogrcouchdbdatasource.cpp ogrcouchdbdriver.cpp ogrcouchdblayer.cpp ogrcouchdbrowslayer.cpp
          ogrcouchdbtablelayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_CouchDB)
target_include_directories(ogr_CouchDB PRIVATE $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_CouchDB libjson)
else ()
  gdal_target_link_libraries(ogr_CouchDB PRIVATE ${JSONC_LIBRARIES})
endif ()
