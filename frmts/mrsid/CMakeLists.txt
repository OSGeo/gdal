include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(gdal_MrSID)
    include("${PROJECT_SOURCE_DIR}/../../cmake/helpers/SetupStandalonePlugin.cmake" )
    include(CheckDependentLibrariesGeoTIFF)
    include(CheckDependentLibrariesMrSID)
    if (GDAL_USE_GEOTIFF_INTERNAL)
        message(FATAL_ERROR "Internal libgeotiff not supported for MrSID standalone plugin build")
    endif()
    standalone_driver_finalize(GDAL_ENABLE_DRIVER_MRSID)
endif()

option(GDAL_ENABLE_DRIVER_JP2MRSID "Whether to enable JPEG2000 support with MrSID SDK" OFF)

add_gdal_driver(TARGET gdal_MrSID
                SOURCES mrsidstream.h mrsidstream.cpp mrsiddataset.cpp
                CORE_SOURCES mrsiddrivercore.cpp
                PLUGIN_CAPABLE_IF "NOT GDAL_USE_GEOTIFF_INTERNAL OR NOT GDAL_HIDE_INTERNAL_SYMBOLS"
                NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_MrSID_core)
    target_include_directories(gdal_MrSID_core PRIVATE $<TARGET_PROPERTY:MRSID::MRSID,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_MrSID_core PRIVATE $<TARGET_PROPERTY:MRSID::MRSID,INTERFACE_COMPILE_DEFINITIONS>)
    if (GDAL_ENABLE_DRIVER_JP2MRSID)
      target_compile_definitions(gdal_MrSID_core PRIVATE -DMRSID_J2K)
    endif ()
endif()

if(NOT TARGET gdal_MrSID)
    return()
endif()

gdal_standard_includes(gdal_MrSID)
gdal_target_link_libraries(gdal_MrSID PRIVATE MRSID::MRSID)

if (GDAL_ENABLE_DRIVER_JP2MRSID)
  target_compile_definitions(gdal_MrSID PRIVATE -DMRSID_J2K)
endif ()

if (GDAL_USE_GEOTIFF_INTERNAL)
  gdal_add_vendored_lib(gdal_MrSID geotiff)
else ()
  gdal_target_link_libraries(gdal_MrSID PRIVATE ${GeoTIFF_TARGET})
endif ()
