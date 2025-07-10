include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(gdal_AVIF)
    set(STRICT_VERSION_CHECK ON)
    include("${PROJECT_SOURCE_DIR}/../../cmake/helpers/SetupStandalonePlugin.cmake" )
    include(CheckDependentLibrariesAVIF)
    standalone_driver_finalize(GDAL_ENABLE_DRIVER_AVIF)
endif()

add_gdal_driver(TARGET gdal_AVIF
                SOURCES avifdataset.cpp
                CORE_SOURCES avifdrivercore.cpp
                PLUGIN_CAPABLE
                NO_SHARED_SYMBOL_WITH_CORE
)

if(NOT TARGET gdal_AVIF)
    return()
endif()

gdal_standard_includes(gdal_AVIF)
gdal_target_link_libraries(gdal_AVIF PRIVATE AVIF::AVIF)
