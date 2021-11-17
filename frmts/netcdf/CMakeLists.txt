# There are netCDF, GMT drivers. When PLUGIN specifying NETCDF_PLUGIN then automatically register GMT from netCDF.
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
add_gdal_driver(TARGET gdal_netCDF SOURCES ${_SOURCES} PLUGIN_CAPABLE)
unset(_SOURCES)
if (GDAL_ENABLE_FRMT_NETCDF)
  target_compile_definitions(gdal_netCDF PRIVATE -DNETCDF_PLUGIN)
endif ()

gdal_standard_includes(gdal_netCDF)
gdal_target_link_libraries(TARGET gdal_netCDF LIBRARIES NETCDF::netCDF)
if (NETCDF_HAS_NC4)
  target_compile_definitions(gdal_netCDF PRIVATE -DNETCDF_HAS_NC4)
endif ()
if (NETCDF_HAS_HDF4)
  target_compile_definitions(gdal_netCDF PRIVATE -DNETCDF_HAS_HDF4)
endif ()
if (NETCDF_HAS_MEM)
  target_compile_definitions(gdal_netCDF PRIVATE -DHAVE_NETCDF_MEM)
  if (HAVE_USERFAULTFD_H)
    target_compile_definitions(gdal_netCDF PRIVATE -DENABLE_UFFD)
  endif ()
endif ()
if (HAVE_HDF4)
  target_compile_definitions(gdal_netCDF PRIVATE -DHAVE_HDF4)
endif ()
if (HAVE_HDF5)
  target_compile_definitions(gdal_netCDF PRIVATE -DHAVE_HDF5)
endif ()
