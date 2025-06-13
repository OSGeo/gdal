add_gdal_driver(
  TARGET ogr_GeoJSON
  SOURCES ogrgeojsondatasource.cpp
          ogrgeojsonlayer.cpp
          ogrgeojsonreader.cpp
          ogrgeojsonutils.cpp
          ogrgeojsonwritelayer.cpp
          ogrgeojsondriver.cpp
          ogrgeojsonseqdriver.cpp
          ogresrijsonreader.cpp
          ogresrijsondriver.cpp
          ogrtopojsonreader.cpp
          ogrtopojsondriver.cpp
          ogrjsoncollectionstreamingparser.cpp
  PLUGIN_CAPABLE_IF "NOT GDAL_USE_JSONC_INTERNAL"
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_GeoJSON)
target_include_directories(ogr_GeoJSON PRIVATE $<TARGET_PROPERTY:appslib,SOURCE_DIR>)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(ogr_GeoJSON libjson)
else ()
  gdal_target_link_libraries(ogr_GeoJSON PRIVATE ${JSONC_TARGET})
endif ()
if (OGR_ENABLE_DRIVER_GEOJSON_PLUGIN)
  target_compile_definitions(ogr_GeoJSON PRIVATE BUILT_AS_PLUGIN)
endif()
