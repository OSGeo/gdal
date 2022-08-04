add_gdal_driver(TARGET gdal_BASISU_KTX2
                SOURCES
                basisudataset.cpp
                ktx2dataset.cpp
                common.cpp
                STRONG_CXX_WFLAGS
                PLUGIN_CAPABLE)
gdal_standard_includes(gdal_BASISU_KTX2)
gdal_target_link_libraries(gdal_BASISU_KTX2 PRIVATE basisu::basisu_lib)
