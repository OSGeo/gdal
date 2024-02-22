add_gdal_driver(TARGET gdal_XPM SOURCES xpmdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_XPM)
target_include_directories(gdal_XPM PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
