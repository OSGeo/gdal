add_gdal_driver(TARGET gdal_SAGA SOURCES sagadataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_SAGA)
