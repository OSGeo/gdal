add_gdal_driver(TARGET gdal_JPEGLS SOURCES jpegls_header.h jpeglsdataset.cpp PLUGIN_CAPABLE)
gdal_standard_includes(gdal_JPEGLS)
gdal_target_link_libraries(TARGET gdal_JPEGLS LIBRARIES CharLS::charls)

if (CharLS_VERSION STREQUAL 1)
  target_compile_definitions(gdal_JPEGLS PRIVATE -DCHARLS_INTERFACE_H)
elseif (CharLS_VERSION STREQUAL 2.1)
  target_compile_definitions(gdal_JPEGLS PRIVATE -DCHARLS_2_1)
  # libcharls 2.2.0 requires C++14
  set_property(TARGET gdal_JPEGLS PROPERTY CXX_STANDARD 14)
else ()
  target_compile_definitions(gdal_JPEGLS PRIVATE -DCHARLS_2)
endif ()
