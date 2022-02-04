add_gdal_driver(TARGET gdal_RASDAMAN SOURCES rasdamandataset.cpp rasdamandataset.h)
gdal_standard_includes(gdal_RASDAMAN)
target_include_directories(gdal_RASDAMAN PRIVATE ${RASDAMAN_INCLUDE_DIR})
gdal_target_link_libraries(gdal_RASDAMAN PRIVATE ${RASDAMAN_LIBRARY})
