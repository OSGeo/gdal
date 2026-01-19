include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(ogr_Arrow)
    # The driver depends on some non exported headers. Make sure the version we
    # build against against is the same as this source tree.
    set(STRICT_VERSION_CHECK ON)
    include("${PROJECT_SOURCE_DIR}/../../../cmake/helpers/SetupStandalonePlugin.cmake" )
    include(CheckDependentLibrariesArrowParquet)
    standalone_driver_finalize(OGR_ENABLE_DRIVER_ARROW)
endif()

add_gdal_driver(TARGET ogr_Arrow
                SOURCES ogrfeatherdriver.cpp
                        ogrfeatherdataset.cpp
                        ogrfeatherlayer.cpp
                        ogrfeatherwriterdataset.cpp
                        ogrfeatherwriterlayer.cpp
                CORE_SOURCES
                        ogrfeatherdrivercore.cpp
                PLUGIN_CAPABLE
                SKIP_GDAL_PRIV_HEADER  # because we may need to use C++20
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_Arrow)
    return()
endif()

gdal_standard_includes(ogr_Arrow)
if (STANDALONE)
target_include_directories(ogr_Arrow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../port
                                             ${CMAKE_CURRENT_SOURCE_DIR}/../generic
                                             ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
else()
target_include_directories(ogr_Arrow PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
endif()

if(${Arrow_VERSION} VERSION_GREATER_EQUAL 16.0)
    target_sources(ogr_Arrow PRIVATE vsifilesystemregistrar.cpp)
endif()

if(${Arrow_VERSION} VERSION_GREATER_EQUAL 10.0)
    if(ARROW_USE_STATIC_LIBRARIES)
        gdal_target_link_libraries(ogr_Arrow PRIVATE Arrow::arrow_static)
    else()
        gdal_target_link_libraries(ogr_Arrow PRIVATE Arrow::arrow_shared)
    endif()
else()
    if(ARROW_USE_STATIC_LIBRARIES)
        gdal_target_link_libraries(ogr_Arrow PRIVATE arrow_static)
    else()
        gdal_target_link_libraries(ogr_Arrow PRIVATE arrow_shared)
    endif()
endif()
