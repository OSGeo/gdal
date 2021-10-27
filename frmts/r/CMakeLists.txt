add_gdal_driver(TARGET gdal_R SOURCES rdataset.h rcreatecopy.cpp rdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_R)
