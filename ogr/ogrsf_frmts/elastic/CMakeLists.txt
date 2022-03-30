add_gdal_driver(
  TARGET ogr_Elastic
  SOURCES ogr_elastic.h
          ogrelasticdatasource.cpp
          ogrelasticdriver.cpp
          ogrelasticlayer.cpp
          ogrelasticaggregationlayer.cpp
          PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_JSONC_INTERNAL")
gdal_standard_includes(ogr_Elastic)
target_include_directories(ogr_Elastic PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/geojson)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_Elastic libjson)
else ()
  gdal_target_link_libraries(ogr_Elastic PRIVATE ${JSONC_TARGET})
endif ()
