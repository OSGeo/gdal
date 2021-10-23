add_gdal_driver(TARGET gdal_GTA SOURCES gta_headers.h gtadataset.cpp)
gdal_standard_includes(gdal_GTA)
target_include_directories(gdal_GTA PRIVATE ${GTA_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET gdal_GTA LIBRARIES ${GTA_LIBRARIES})
