add_gdal_driver(TARGET gdal_MrSID SOURCES mrsidstream.h mrsidstream.cpp mrsiddataset.cpp PLUGIN_CAPABLE_IF
                                          "NOT GDAL_USE_GEOTIFF_INTERNAL")
gdal_standard_includes(gdal_MrSID)
gdal_target_link_libraries(gdal_MrSID PRIVATE MRSID::MRSID)

option(GDAL_ENABLE_DRIVER_JP2MRSID "Whether to enable JPEG2000 support with MrSID SDK" OFF)
if (GDAL_ENABLE_DRIVER_JP2MRSID)
  target_compile_definitions(gdal_MrSID PRIVATE -DMRSID_J2K)
endif ()

if (GDAL_USE_GEOTIFF_INTERNAL)
  gdal_add_vendored_lib(gdal_MrSID geotiff)
else ()
  gdal_target_link_libraries(gdal_MrSID PRIVATE ${GeoTIFF_TARGET})
endif ()
