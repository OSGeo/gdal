add_gdal_driver(TARGET gdal_DIMAP SOURCES dimapdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_DIMAP)
target_include_directories(gdal_DIMAP PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt)
