add_gdal_driver(TARGET gdal_PNG SOURCES pngdataset.cpp pngdataset.h PLUGIN_CAPABLE_IF
                                        "NOT GDAL_USE_PNG_INTERNAL;NOT GDAL_USE_ZLIB_INTERNAL")
gdal_standard_includes(gdal_PNG)

if (GDAL_USE_PNG_INTERNAL)
  gdal_add_vendored_lib(gdal_PNG libpng)
else ()
  gdal_target_link_libraries(gdal_PNG PRIVATE PNG::PNG)
endif ()

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(gdal_PNG libz)
else ()
  gdal_target_link_libraries(gdal_PNG PRIVATE ZLIB::ZLIB)
endif ()
