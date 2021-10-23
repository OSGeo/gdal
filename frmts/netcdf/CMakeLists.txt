# There are netCDF, GMT drivers. When PLUGIN specifying NETCDF_PLUGIN then automatically register GMT from netCDF.
option(GDAL_ENABLE_NETCDF_PLUGIN "Enable to build NetCDF plugin" ON)
set(_SOURCES
    netcdfdataset.cpp
    netcdflayer.cpp
    netcdfwriterconfig.cpp
    gmtdataset.cpp
    netcdfsg.cpp
    netcdflayersg.cpp
    netcdfsgwriterutil.cpp
    netcdfmultidim.cpp
    netcdfvirtual.cpp
    netcdf_sentinel3_sral_mwr.cpp)
if (GDAL_ENABLE_NETCDF_PLUGIN)
  add_gdal_driver(
    TARGET gdal_netCDF
    SOURCES ${_SOURCES}
    PLUGIN)
else ()
  add_gdal_driver(TARGET gdal_netCDF SOURCES ${_SOURCES})
endif ()
unset(_SOURCES)
if (GDAL_ENABLE_PLUGIN OR GDAL_ENABLE_NETCDF_PLUGIN)
  target_compile_definitions(gdal_netCDF PRIVATE -DNETCDF_PLUGIN)
endif ()

gdal_standard_includes(gdal_netCDF)
target_include_directories(gdal_netCDF PRIVATE ${NETCDF_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET gdal_netCDF LIBRARIES NETCDF::netCDF)
if (NETCDF_HAS_NC4)
  target_compile_definitions(gdal_netCDF PRIVATE -DNETCDF_HAS_NC4)
endif ()
if (NETCDF_HAS_HDF4)
  target_compile_definitions(gdal_netCDF PRIVATE -DNETCDF_HAS_HDF4)
endif ()
if (HAVE_USERFAULTFD_H AND NETCDF_MEM)
  target_compile_definitions(gdal_netCDF PRIVATE -DENABLE_UFFD)
endif ()
if (HAVE_HDF4)
  target_compile_definitions(gdal_netCDF PRIVATE -DHAVE_HDF4)
endif ()
if (HAVE_HDF5)
  target_compile_definitions(gdal_netCDF PRIVATE -DHAVE_HDF5)
endif ()
