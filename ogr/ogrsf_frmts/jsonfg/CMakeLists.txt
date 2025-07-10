# should not be a plugin - dependent from non public symbols of geojson/
add_gdal_driver(
  TARGET ogr_JSONFG
  SOURCES
      ogrjsonfgdataset.cpp
      ogrjsonfgdriver.cpp
      ogrjsonfgmemlayer.cpp
      ogrjsonfgreader.cpp
      ogrjsonfgstreamedlayer.cpp
      ogrjsonfgstreamingparser.cpp
      ogrjsonfgwritelayer.cpp
  BUILTIN)
gdal_standard_includes(ogr_JSONFG)
target_include_directories(ogr_JSONFG PRIVATE $<TARGET_PROPERTY:ogr_GeoJSON,SOURCE_DIR>)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_JSONFG libjson)
else ()
  gdal_target_link_libraries(ogr_JSONFG PRIVATE ${JSONC_TARGET})
endif ()
