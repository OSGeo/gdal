add_gdal_driver(TARGET gdal_USGSDEM SOURCES usgsdem_create.cpp usgsdemdataset.cpp)
gdal_standard_includes(gdal_USGSDEM)
target_include_directories(gdal_USGSDEM PRIVATE $<TARGET_PROPERTY:alg,SOURCE_DIR>)
