add_gdal_driver(TARGET gdal_PLMOSAIC SOURCES plmosaicdataset.cpp PLUGIN_CAPABLE_IF "NOT GDAL_USE_JSONC_INTERNAL"
                                             NO_DEPS)
gdal_standard_includes(gdal_PLMOSAIC)
target_include_directories(gdal_PLMOSAIC PRIVATE ${GDAL_VECTOR_FORMAT_SOURCE_DIR}/geojson)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(gdal_PLMOSAIC libjson)
else ()
  gdal_target_link_libraries(gdal_PLMOSAIC PRIVATE ${JSONC_TARGET})
endif ()
