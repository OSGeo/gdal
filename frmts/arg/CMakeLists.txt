add_gdal_driver(TARGET gdal_ARG SOURCES argdataset.cpp PLUGIN_CAPABLE_IF "NOT GDAL_USE_JSONC_INTERNAL" NO_DEPS)
gdal_standard_includes(gdal_ARG)
target_include_directories(gdal_ARG PRIVATE $<TARGET_PROPERTY:gdal_raw,SOURCE_DIR>
                                            $<TARGET_PROPERTY:ogr_geojson,SOURCE_DIR>)
if (GDAL_USE_JSONC_INTERNAL)
  gdal_add_vendored_lib(gdal_ARG libjson)
else ()
  gdal_target_link_libraries(gdal_ARG PRIVATE ${JSONC_TARGET})
endif ()
