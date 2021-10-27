add_gdal_driver(TARGET gdal_SAFE SOURCES safedataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_SAFE)
