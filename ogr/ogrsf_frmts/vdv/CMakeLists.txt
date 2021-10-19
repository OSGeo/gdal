add_gdal_driver(TARGET ogr_VDV SOURCES ogr_vdv.h ogrvdvdatasource.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_VDV)
