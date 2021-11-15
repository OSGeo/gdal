add_gdal_driver(TARGET gdal_JPIPKAK SOURCES jpipkak_headers.h jpipkakdataset.h jpipkakdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_JPIPKAK)
gdal_target_link_libraries(TARGET gdal_JPIPKAK LIBRARIES KDU::KDU)
