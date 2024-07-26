include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(ogr_OCI)
    include("${PROJECT_SOURCE_DIR}/../../../cmake/helpers/SetupStandalonePlugin.cmake" )
    include(CheckDependentLibrariesOracle)
    standalone_driver_finalize(OGR_ENABLE_DRIVER_OCI)
endif()

set(SOURCE
    oci_utils.cpp
    ocitest.cpp
    ogrocidatasource.cpp
    ogrocidriver.cpp
    ogrocilayer.cpp
    ogrociloaderlayer.cpp
    ogrociselectlayer.cpp
    ogrocisession.cpp
    ogrocistatement.cpp
    ogrocistringbuf.cpp
    ogrocistroke.cpp
    ogrocitablelayer.cpp
    ogrociwritablelayer.cpp)

add_gdal_driver(TARGET ogr_OCI
                SOURCES ${SOURCE}
                CORE_SOURCES ogrocidrivercore.cpp
                PLUGIN_CAPABLE NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_OCI)
    return()
endif()

gdal_standard_includes(ogr_OCI)
gdal_target_link_libraries(ogr_OCI PRIVATE Oracle::OCI)
