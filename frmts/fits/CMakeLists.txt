add_gdal_driver(TARGET gdal_FITS SOURCES fitsdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_FITS)
gdal_target_link_libraries(TARGET gdal_FITS LIBRARIES CFITSIO::CFITSIO)
