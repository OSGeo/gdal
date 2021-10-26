add_gdal_driver(TARGET gdal_FITS SOURCES fitsdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_FITS)
target_include_directories(gdal_FITS PRIVATE ${CFITSIO_INCLUDE_DIR})
gdal_target_link_libraries(TARGET gdal_FITS LIBRARIES ${CFITSIO_LIBRARIES})
