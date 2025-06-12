add_gdal_driver(TARGET gdal_SIGDEM SOURCES sigdemdataset.cpp sigdemdataset.h PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_SIGDEM)
