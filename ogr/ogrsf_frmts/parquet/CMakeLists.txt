add_gdal_driver(TARGET ogr_Parquet
                SOURCES ogrparquetdriver.cpp
                        ogrparquetdataset.cpp
                        ogrparquetlayer.cpp
                        ogrparquetwriterdataset.cpp
                        ogrparquetwriterlayer.cpp
                CORE_SOURCES
                        ogrparquetdrivercore.cpp
                PLUGIN_CAPABLE
                STRONG_CXX_WFLAGS
                NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET ogr_Parquet)
    return()
endif()

gdal_standard_includes(ogr_Parquet)
target_include_directories(ogr_Parquet PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                                               $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)

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
