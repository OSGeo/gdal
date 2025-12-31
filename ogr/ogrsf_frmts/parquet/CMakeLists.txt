include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/helpers/GdalCMakeMinimumRequired.cmake")
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

if(NOT DEFINED PROJECT_SOURCE_DIR)
    # Standalone plugin building
    project(ogr_Parquet)
    # The driver depends on some non exported headers. Make sure the version we
    # build against against is the same as this source tree.
    set(STRICT_VERSION_CHECK ON)
    include("${PROJECT_SOURCE_DIR}/../../../cmake/helpers/SetupStandalonePlugin.cmake" )
    include(CheckDependentLibrariesArrowParquet)
    standalone_driver_finalize(OGR_ENABLE_DRIVER_PARQUET)
endif()

add_gdal_driver(TARGET ogr_Parquet
                SOURCES ogrparquetdriver.cpp
                        ogrparquetdataset.cpp
                        ogrparquetlayer.cpp
                        ogrparquetwriterdataset.cpp
                        ogrparquetwriterlayer.cpp
                CORE_SOURCES
                        ogrparquetdrivercore.cpp
                PLUGIN_CAPABLE
                SKIP_GDAL_PRIV_HEADER  # because we may need to use C++20
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_Parquet)
    return()
endif()

gdal_standard_includes(ogr_Parquet)
if (STANDALONE)
target_compile_definitions(ogr_Parquet PRIVATE STANDALONE)
target_include_directories(ogr_Parquet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../port
                                               ${CMAKE_CURRENT_SOURCE_DIR}/../generic
                                               ${CMAKE_CURRENT_SOURCE_DIR}/../../frmts/mem
                                               ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
else()
target_include_directories(ogr_Parquet PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
endif()

if(${Parquet_VERSION} VERSION_GREATER_EQUAL 10.0)
    if(ARROW_USE_STATIC_LIBRARIES)
        gdal_target_link_libraries(ogr_Parquet PRIVATE Arrow::arrow_static Parquet::parquet_static)
    else()
        gdal_target_link_libraries(ogr_Parquet PRIVATE Arrow::arrow_shared Parquet::parquet_shared)
    endif()
else()
    if(ARROW_USE_STATIC_LIBRARIES)
        gdal_target_link_libraries(ogr_Parquet PRIVATE arrow_static parquet_static)
    else()
        gdal_target_link_libraries(ogr_Parquet PRIVATE arrow_shared parquet_shared)
    endif()
endif()

if (GDAL_USE_ARROWDATASET)
    target_compile_definitions(ogr_Parquet PRIVATE -DGDAL_USE_ARROWDATASET)
    if(${ArrowDataset_VERSION} VERSION_GREATER_EQUAL 10.0)
        if(ARROW_USE_STATIC_LIBRARIES)
            gdal_target_link_libraries(ogr_Parquet PRIVATE ArrowDataset::arrow_dataset_static)
        else()
            gdal_target_link_libraries(ogr_Parquet PRIVATE ArrowDataset::arrow_dataset_shared)
        endif()
    else()
        if(ARROW_USE_STATIC_LIBRARIES)
            gdal_target_link_libraries(ogr_Parquet PRIVATE arrow_dataset_static)
        else()
            gdal_target_link_libraries(ogr_Parquet PRIVATE arrow_dataset_shared)
        endif()
    endif()
    target_sources(ogr_Parquet PRIVATE ogrparquetdatasetlayer.cpp)
endif()

if (GDAL_USE_ARROWCOMPUTE)
    target_compile_definitions(ogr_Parquet PRIVATE -DGDAL_USE_ARROWCOMPUTE)
    if(ARROW_USE_STATIC_LIBRARIES)
        gdal_target_link_libraries(ogr_Parquet PRIVATE ArrowCompute::arrow_compute_static)
    else()
        gdal_target_link_libraries(ogr_Parquet PRIVATE ArrowCompute::arrow_compute_shared)
    endif()
endif()
