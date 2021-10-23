add_gdal_driver(TARGET gdal_STACTA SOURCES stactadataset.cpp)
gdal_standard_includes(gdal_STACTA)
target_include_directories(gdal_STACTA PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
