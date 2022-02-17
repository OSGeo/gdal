# should not be a plugin - dependency from elastic
add_gdal_driver(
  TARGET ogr_geojson
  SOURCES ogrgeojsondatasource.cpp
          ogrgeojsonlayer.cpp
          ogrgeojsonreader.cpp
          ogrgeojsonutils.cpp
          ogrgeojsonwritelayer.cpp
          ogrgeojsonwriter.cpp
          ogrgeojsondriver.cpp
          ogrgeojsonseqdriver.cpp
          ogresrijsonreader.cpp
          ogresrijsondriver.cpp
          ogrtopojsonreader.cpp
          ogrtopojsondriver.cpp
  BUILTIN)
gdal_standard_includes(ogr_geojson)
target_include_directories(ogr_geojson PRIVATE $<TARGET_PROPERTY:appslib,SOURCE_DIR>)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_geojson libjson)
else ()
  gdal_target_link_libraries(ogr_geojson PRIVATE ${JSONC_LIBRARIES})
endif ()
