add_gdal_driver(TARGET ogr_Arrow
                SOURCES ogrfeatherdriver.cpp
                        ogrfeatherdataset.cpp
                        ogrfeatherlayer.cpp
                        ogrfeatherwriterdataset.cpp
                        ogrfeatherwriterlayer.cpp
                PLUGIN_CAPABLE
                CXX_WFLAGS_EFFCXX)
gdal_standard_includes(ogr_Arrow)
target_include_directories(ogr_Arrow PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
gdal_target_link_libraries(ogr_Arrow PRIVATE arrow_shared)
