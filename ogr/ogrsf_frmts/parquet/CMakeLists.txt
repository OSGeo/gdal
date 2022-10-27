add_gdal_driver(TARGET ogr_Parquet
                SOURCES ogrparquetdriver.cpp
                        ogrparquetdataset.cpp
                        ogrparquetlayer.cpp
                        ogrparquetwriterdataset.cpp
                        ogrparquetwriterlayer.cpp
                PLUGIN_CAPABLE
                STRONG_CXX_WFLAGS)
gdal_standard_includes(ogr_Parquet)
target_include_directories(ogr_Parquet PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                                               $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
if(${Parquet_VERSION} VERSION_GREATER_EQUAL 10.0)
    gdal_target_link_libraries(ogr_Parquet PRIVATE Arrow::arrow_shared Parquet::parquet_shared)
    target_compile_features(ogr_Parquet PRIVATE cxx_std_17)
else()
    gdal_target_link_libraries(ogr_Parquet PRIVATE arrow_shared parquet_shared)
endif()

if (GDAL_USE_ARROWDATASET)
    target_compile_definitions(ogr_Parquet PRIVATE -DGDAL_USE_ARROWDATASET)
    if(${ArrowDataset_VERSION} VERSION_GREATER_EQUAL 10.0)
        gdal_target_link_libraries(ogr_Parquet PRIVATE ArrowDataset::arrow_dataset_shared)
    else()
        gdal_target_link_libraries(ogr_Parquet PRIVATE arrow_dataset_shared)
    endif()
    target_sources(ogr_Parquet PRIVATE ogrparquetdatasetlayer.cpp)
endif()
