add_gdal_driver(TARGET ogr_XLS SOURCES ogr_xls.h ogrxlsdatasource.cpp ogrxlsdriver.cpp ogrxlslayer.cpp PLUGIN_CAPABLE)
gdal_standard_includes(ogr_XLS)
gdal_target_link_libraries(TARGET ogr_XLS LIBRARIES FREEXL::freexl)
