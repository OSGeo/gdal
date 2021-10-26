add_gdal_driver(TARGET gdal_RS2 SOURCES rs2dataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_RS2)
