add_gdal_driver(TARGET ogr_GMT SOURCES ogr_gmt.h ogrgmtdatasource.cpp ogrgmtdriver.cpp ogrgmtlayer.cpp PLUGIN_CAPABLE
                                       NO_DEPS)
gdal_standard_includes(ogr_GMT)
