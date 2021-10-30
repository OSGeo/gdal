add_gdal_driver(
  TARGET gdal_Zarr
  SOURCES zarr_array.cpp
          zarr_attribute.cpp
          zarr_group.cpp
          zarrdriver.cpp
          STRONG_CXX_WFLAGS
          PLUGIN_CAPABLE
          NO_DEPS)
gdal_standard_includes(gdal_Zarr)
target_include_directories(gdal_Zarr PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem
                                             ${GDAL_RASTER_FORMAT_SOURCE_DIR}/netcdf)

if (GDAL_USE_BLOSC)
  target_compile_definitions(gdal_Zarr PRIVATE -DHAVE_BLOSC)
  gdal_target_link_libraries(TARGET gdal_Zarr LIBRARIES Blosc::Blosc)
endif ()
