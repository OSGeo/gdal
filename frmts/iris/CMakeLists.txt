add_gdal_driver(TARGET gdal_IRIS SOURCES irisdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_IRIS)
