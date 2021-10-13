add_gdal_driver(TARGET gdal_WEBP SOURCES webp_headers.h webpdataset.cpp)
gdal_standard_includes(gdal_WEBP)
gdal_target_link_libraries(TARGET gdal_WEBP LIBRARIES WEBP::WebP)
