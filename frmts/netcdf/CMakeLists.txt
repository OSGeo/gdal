# There are netCDF, GMT drivers. When PLUGIN specifying NETCDF_PLUGIN then automatically register GMT from netCDF.
set(_SOURCES
    netcdfdataset.cpp
    netcdflayer.cpp
    netcdfwriterconfig.cpp
    netcdfsg.cpp
    netcdflayersg.cpp
    netcdfsgwriterutil.cpp
    netcdfmultidim.cpp
    netcdfvirtual.cpp
    netcdf_sentinel3_sral_mwr.cpp)
add_gdal_driver(TARGET gdal_netCDF
                SOURCES ${_SOURCES}
                CORE_SOURCES netcdfdrivercore.cpp
                PLUGIN_CAPABLE)
unset(_SOURCES)

function(declare_def DEF)
    if(TARGET gdal_netCDF)
        target_compile_definitions(gdal_netCDF PRIVATE ${DEF})
    endif()
    if(TARGET gdal_netCDF_core)
        target_compile_definitions(gdal_netCDF_core PRIVATE ${DEF})
    endif()
endfunction()

# NETCDF prefix is from Find module, netCDF prefix is from Config
if (NETCDF_HAS_NC4 OR netCDF_HAS_NC4)
  declare_def(-DNETCDF_HAS_NC4)
endif ()
if (NETCDF_HAS_HDF4 OR netCDF_HAS_HDF4)
  declare_def(-DNETCDF_HAS_HDF4)
endif ()
if (NETCDF_HAS_MEM OR netCDF_HAS_DISKLESS)
  declare_def(-DHAVE_NETCDF_MEM)
  if (HAVE_USERFAULTFD_H)
    declare_def(-DENABLE_UFFD)
  endif ()
endif ()
if (HAVE_HDF4)
  declare_def(-DHAVE_HDF4)
endif ()
if (HAVE_HDF5)
  declare_def(-DHAVE_HDF5)
endif ()

if(NOT TARGET gdal_netCDF)
    return()
endif()

gdal_standard_includes(gdal_netCDF)
gdal_target_link_libraries(gdal_netCDF PRIVATE ${NetCDF_TARGET})

