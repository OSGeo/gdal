add_gdal_driver(TARGET gdal_PNG SOURCES pngdataset.cpp pngdataset.h PLUGIN_CAPABLE_IF
                                        "NOT GDAL_USE_LIBPNG_INTERNAL;NOT GDAL_USE_LIBZ_INTERNAL")
gdal_standard_includes(gdal_PNG)

if (GDAL_USE_LIBPNG_INTERNAL)
  target_include_directories(gdal_PNG PRIVATE $<TARGET_PROPERTY:libpng,SOURCE_DIR>)
else ()
  gdal_target_link_libraries(TARGET gdal_PNG LIBRARIES PNG::PNG)
endif ()

if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(gdal_PNG PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
else ()
  gdal_target_link_libraries(TARGET gdal_PNG LIBRARIES ZLIB::ZLIB)
endif ()
