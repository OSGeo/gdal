add_gdal_driver(TARGET ogr_CSV SOURCES ogr_csv.h ogrcsvdatasource.cpp ogrcsvdriver.cpp ogrcsvlayer.cpp PLUGIN_CAPABLE
                                       NO_DEPS)
gdal_standard_includes(ogr_CSV)
target_include_directories(ogr_CSV PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
