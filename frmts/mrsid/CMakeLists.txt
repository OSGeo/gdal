add_gdal_driver(TARGET gdal_MrSID SOURCES mrsidstream.h mrsidstream.cpp mrsiddataset.cpp PLUGIN_CAPABLE_IF
                                          "NOT GDAL_USE_LIBGEOTIFF_INTERNAL")
gdal_standard_includes(gdal_MrSID)
gdal_target_link_libraries(TARGET gdal_MrSID LIBRARIES MRSID::MRSID)

option(GDAL_ENABLE_FRMT_JP2MRSID "Whether to enable JPEG2000 support with MrSID SDK" OFF)
if (GDAL_ENABLE_FRMT_JP2MRSID)
  target_compile_definitions(gdal_MrSID PRIVATE -DMRSID_J2K)
endif ()

if (GDAL_USE_LIBGEOTIFF_INTERNAL)
  if (RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
    target_compile_definitions(gdal_MrSID PRIVATE -DRENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
  endif ()
  target_include_directories(gdal_MrSID PRIVATE $<TARGET_PROPERTY:geotiff,SOURCE_DIR>)
else ()
  gdal_target_link_libraries(TARGET gdal_MrSID LIBRARIES GEOTIFF::GEOTIFF)
endif ()
