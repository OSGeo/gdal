add_gdal_driver(TARGET gdal_DDS SOURCES ddsdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_DDS)
gdal_target_link_libraries(TARGET gdal_DDS LIBRARIES CRNLIB::Crnlib)
target_include_directories(gdal_DDS PRIVATE "${Crnlib_INCLUDE_DIR}/crunch")
