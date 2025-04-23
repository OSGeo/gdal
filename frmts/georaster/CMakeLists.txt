include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(gdal_GEOR)
    include("${PROJECT_SOURCE_DIR}/../../cmake/helpers/SetupStandalonePlugin.cmake")
    include(CheckDependentLibrariesZLIB)
    include(CheckDependentLibrariesJpeg)
    include(CheckDependentLibrariesOracle)
    if (GDAL_USE_ZLIB_INTERNAL)
        message(FATAL_ERROR "Internal zlib not supported for GeoRaster standalone plugin build")
    endif()
    if (GDAL_USE_JPEG_INTERNAL)
        message(FATAL_ERROR "Internal libjpeg not supported for GeoRaster standalone plugin build")
    endif()
    standalone_driver_finalize(GDAL_ENABLE_DRIVER_GEOR)
endif()

add_gdal_driver(
  TARGET gdal_GEOR
  SOURCES georaster_dataset.cpp georaster_priv.h georaster_rasterband.cpp georaster_wrapper.cpp oci_wrapper.cpp
          cpl_vsil_ocilob.cpp
  CORE_SOURCES georasterdrivercore.cpp
  DEF FRMT_georaster PLUGIN_CAPABLE_IF "NOT GDAL_USE_ZLIB_INTERNAL\\\;NOT GDAL_USE_JPEG_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_GEOR)
    return()
endif()

gdal_standard_includes(gdal_GEOR)
if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(gdal_GEOR libz)
endif ()

if (GDAL_ENABLE_DRIVER_JPEG)
  target_compile_definitions(gdal_GEOR PRIVATE -DJPEG_SUPPORTED)
  target_sources(gdal_GEOR PRIVATE georaster_jpeg_vsidataio.cpp)
  # Need a separate compilation unit given that libjpeg's boolean typedef would conflict with OCI one
  set_property(SOURCE georaster_jpeg_vsidataio.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif()

if (GDAL_ENABLE_DRIVER_JPEG AND GDAL_USE_JPEG_INTERNAL)
  gdal_add_vendored_lib(gdal_GEOR libjpeg)
endif ()

# Include libjpeg first so that if we want to use jpeg-turbo on homebrew
# /usr/local/opt/jpeg-turbo gets included first, before /usr/local/opt
# that contains libjpeg 9e
if (GDAL_ENABLE_DRIVER_JPEG AND (NOT GDAL_USE_JPEG_INTERNAL))
  gdal_target_link_libraries(gdal_GEOR PRIVATE JPEG::JPEG)
endif ()

gdal_target_link_libraries(gdal_GEOR PRIVATE Oracle::OCI)

if (NOT GDAL_USE_ZLIB_INTERNAL)
  gdal_target_link_libraries(gdal_GEOR PRIVATE ZLIB::ZLIB)
endif ()
