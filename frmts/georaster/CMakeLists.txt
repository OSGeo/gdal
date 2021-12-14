add_gdal_driver(
  TARGET gdal_GEOR
  SOURCES georaster_dataset.cpp georaster_priv.h georaster_rasterband.cpp georaster_wrapper.cpp oci_wrapper.cpp
          cpl_vsil_ocilob.cpp
  DEF FRMT_georaster PLUGIN_CAPABLE_IF "NOT GDAL_USE_LIBZ_INTERNAL;NOT GDAL_USE_LIBJPEG_INTERNAL")
gdal_standard_includes(gdal_GEOR)
if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(gdal_GEOR PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
endif ()

if (GDAL_USE_LIBJPEG_INTERNAL)
  target_include_directories(gdal_GEOR PRIVATE $<TARGET_PROPERTY:libjpeg,SOURCE_DIR>)
  target_compile_definitions(gdal_GEOR PRIVATE -DJPEG_SUPPORTED)
  if (RENAME_INTERNAL_LIBJPEG_SYMBOLS)
    target_compile_definitions(gdal_GEOR PRIVATE -DRENAME_INTERNAL_LIBJPEG_SYMBOLS)
  endif ()
endif ()

gdal_target_link_libraries(TARGET gdal_GEOR LIBRARIES Oracle::OCI)

if (NOT GDAL_USE_LIBJPEG_INTERNAL)
  if (HAVE_JPEG)
    target_include_directories(gdal_GEOR PRIVATE ${JPEG_INCLUDE_DIRS})
    target_compile_definitions(gdal_GEOR PRIVATE -DJPEG_SUPPORTED)
    gdal_target_link_libraries(TARGET gdal_GEOR LIBRARIES JPEG::JPEG)
  endif ()
endif ()

if (NOT GDAL_USE_LIBZ_INTERNAL)
  gdal_target_link_libraries(TARGET gdal_GEOR LIBRARIES ZLIB::ZLIB)
endif ()

if (GDAL_ENABLE_FRMT_GEOR_PLUGIN)
  # A bit ugly to add this file from the JPEG driver !
  target_sources(gdal_GEOR PRIVATE ../jpeg/vsidataio.cpp)
endif ()
