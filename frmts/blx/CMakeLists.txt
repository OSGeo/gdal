add_gdal_driver(TARGET gdal_BLX SOURCES blx.c blx.h blxdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_BLX)
target_compile_definitions(gdal_BLX PRIVATE -DGDALDRIVER)
