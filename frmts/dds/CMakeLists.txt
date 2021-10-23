add_gdal_driver(TARGET gdal_DDS SOURCES ddsdataset.cpp)
gdal_standard_includes(gdal_DDS)
gdal_target_link_libraries(TARGET gdal_DDS LIBRARIES CRNLIB::Crnlib)
