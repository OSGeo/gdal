add_gdal_driver(TARGET gdal_HEIF SOURCES heifdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_HEIF)
gdal_target_link_libraries(gdal_HEIF PRIVATE HEIF::HEIF)
