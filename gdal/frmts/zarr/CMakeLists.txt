add_gdal_driver(TARGET gdal_Zarr SOURCES zarr_array.cpp zarr_attribute.cpp zarr_group.cpp zarrdriver.cpp)
gdal_standard_includes(gdal_Zarr)
target_include_directories(
  gdal_Zarr PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem ${GDAL_RASTER_FORMAT_SOURCE_DIR}/gtiff
                    ${GDAL_RASTER_FORMAT_SOURCE_DIR}/netcdf)
