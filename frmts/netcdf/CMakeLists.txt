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
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE)
unset(_SOURCES)

function(declare_def DEF)
    if(TARGET gdal_netCDF)
        target_compile_definitions(gdal_netCDF PRIVATE ${DEF})
    endif()
    if(TARGET gdal_netCDF_core)
        target_compile_definitions(gdal_netCDF_core PRIVATE ${DEF})
        if(NetCDF_TARGET AND TARGET ${NetCDF_TARGET})
            target_include_directories(gdal_netCDF_core PRIVATE $<TARGET_PROPERTY:${NetCDF_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
            target_compile_definitions(gdal_netCDF_core PRIVATE $<TARGET_PROPERTY:${NetCDF_TARGET},INTERFACE_COMPILE_DEFINITIONS>)
            target_compile_definitions(gdal_netCDF_core PRIVATE HAS_NETCDF_H)
        endif()
    endif()
endfunction()

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/netcdf_config.xsd
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

# NETCDF prefix is from Find module, netCDF prefix is from Config
if (NETCDF_HAS_HDF4 OR netCDF_HAS_HDF4)
  declare_def(-DNETCDF_HAS_HDF4)
endif ()
if (HAVE_USERFAULTFD_H)
  declare_def(-DENABLE_UFFD)
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
target_compile_definitions(gdal_netCDF PRIVATE HAS_NETCDF_H)
