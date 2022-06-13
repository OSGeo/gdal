add_gdal_driver(TARGET gdal_USGSDEM SOURCES usgsdem_create.cpp usgsdemdataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_USGSDEM)
target_include_directories(gdal_USGSDEM PRIVATE $<TARGET_PROPERTY:alg,SOURCE_DIR> ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
