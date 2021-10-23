add_gdal_driver(TARGET ogr_CSV SOURCES ogr_csv.h ogrcsvdatasource.cpp ogrcsvdriver.cpp ogrcsvlayer.cpp)
gdal_standard_includes(ogr_CSV)
target_include_directories(ogr_CSV PRIVATE $<TARGET_PROPERTY:ogrsf_generic,SOURCE_DIR>)
target_compile_definitions(ogr_CSV PRIVATE -DTAB_ENABLED)
