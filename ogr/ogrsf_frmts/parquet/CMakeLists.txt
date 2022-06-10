add_gdal_driver(TARGET ogr_Parquet
                SOURCES ogrparquetdriver.cpp
                        ogrparquetdataset.cpp
                        ogrparquetlayer.cpp
                        ogrparquetwriterdataset.cpp
                        ogrparquetwriterlayer.cpp
                PLUGIN_CAPABLE
                CXX_WFLAGS_EFFCXX)
gdal_standard_includes(ogr_Parquet)
target_include_directories(ogr_Parquet PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>
                                               $<TARGET_PROPERTY:ogr_MEM,SOURCE_DIR>)
gdal_target_link_libraries(ogr_Parquet PRIVATE arrow_shared parquet_shared)

if (GDAL_USE_ARROWDATASET)
    target_compile_definitions(ogr_Parquet PRIVATE -DGDAL_USE_ARROWDATASET)
    gdal_target_link_libraries(ogr_Parquet PRIVATE arrow_dataset_shared)
    target_sources(ogr_Parquet PRIVATE ogrparquetdatasetlayer.cpp)
endif()
