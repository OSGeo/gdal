add_gdal_driver(TARGET ogr_GMT SOURCES ogr_gmt.h ogrgmtdatasource.cpp ogrgmtdriver.cpp ogrgmtlayer.cpp)
gdal_standard_includes(ogr_GMT)
