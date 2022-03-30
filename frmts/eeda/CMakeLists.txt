add_gdal_driver(
  TARGET gdal_EEDA
  SOURCES eeda.h
          eedaidataset.cpp
          eedadataset.cpp
          eedacommon.cpp
          PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_JSONC_INTERNAL"
          NO_DEPS)
gdal_standard_includes(gdal_EEDA)
target_include_directories(gdal_EEDA PRIVATE $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>)

if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(gdal_EEDA libjson)
else ()
  gdal_target_link_libraries(gdal_EEDA PRIVATE ${JSONC_TARGET})
endif ()
