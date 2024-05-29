add_gdal_driver(
  TARGET gdal_NITF
  SOURCES mgrs.c
          nitfdataset.cpp
          rpftocdataset.cpp
          nitfrasterband.cpp
          ecrgtocdataset.cpp
          nitffile.c
          nitfimage.c
          nitfaridpcm.cpp
          nitfbilevel.cpp
          rpftocfile.cpp
          nitfdes.c
          nitf_gcprpc.cpp
          nitfwritejpeg.cpp
          nitfwritejpeg_12.cpp
  CORE_SOURCES
          nitfdrivercore.cpp
  PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_JPEG_INTERNAL\\\;NOT GDAL_USE_JPEG12_INTERNAL\\\;NOT GDAL_USE_TIFF_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE
)

set(GDAL_DATA_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/data/nitf_spec.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/data/nitf_spec.xsd
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gt_datum.csv
    ${CMAKE_CURRENT_SOURCE_DIR}/data/gt_ellips.csv
)
set_property(
  TARGET ${GDAL_LIB_TARGET_NAME}
  APPEND
  PROPERTY RESOURCE "${GDAL_DATA_FILES}")

if(NOT TARGET gdal_NITF)
    return()
endif()

gdal_standard_includes(gdal_NITF)
target_include_directories(gdal_NITF PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/gtiff
                                             ${GDAL_RASTER_FORMAT_SOURCE_DIR}/vrt)

set_property(SOURCE nitfwritejpeg.cpp nitfwritejpeg_12.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

if (GDAL_USE_TIFF_INTERNAL)
  gdal_add_vendored_lib(gdal_NITF libtiff)
endif()

if (GDAL_ENABLE_DRIVER_JPEG AND GDAL_USE_JPEG_INTERNAL)
  gdal_add_vendored_lib(gdal_NITF libjpeg)
endif()

if (GDAL_ENABLE_DRIVER_JPEG)
  target_compile_definitions(gdal_NITF PRIVATE -DJPEG_SUPPORTED)
  # Include libjpeg first so that if we want to use jpeg-turbo on homebrew
  # /usr/local/opt/jpeg-turbo gets included first, before /usr/local/opt
  # triggered by libtiff, that contains libjpeg 9e
  if (NOT GDAL_USE_JPEG_INTERNAL)
    gdal_target_link_libraries(gdal_NITF PRIVATE JPEG::JPEG)
  endif ()
  if (GDAL_USE_JPEG12_INTERNAL)
    target_compile_definitions(gdal_NITF PRIVATE -DJPEG_DUAL_MODE_8_12)
    target_include_directories(gdal_NITF PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  elseif (NOT GDAL_USE_JPEG_INTERNAL AND HAVE_JPEGTURBO_DUAL_MODE_8_12 )
    target_compile_definitions(gdal_NITF PRIVATE JPEG_DUAL_MODE_8_12 HAVE_JPEGTURBO_DUAL_MODE_8_12)
  endif ()
endif()

if (NOT GDAL_USE_TIFF_INTERNAL)
  gdal_target_link_libraries(gdal_NITF PRIVATE TIFF::TIFF)
endif ()

add_executable(nitfdump EXCLUDE_FROM_ALL nitfdump.c nitffile.c nitfimage.c rpftocfile.cpp nitfbilevel.cpp nitfaridpcm.cpp mgrs.c)
if (GDAL_USE_TIFF_INTERNAL)
  target_sources(nitfdump PRIVATE $<TARGET_OBJECTS:libtiff>)
  gdal_add_vendored_lib(nitfdump libtiff)
  if (RENAME_INTERNAL_TIFF_SYMBOLS)
    target_compile_definitions(nitfdump PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
  endif ()
  if (GDAL_USE_ZLIB_INTERNAL)
    target_sources(nitfdump PRIVATE $<TARGET_OBJECTS:libz>)
  else ()
    target_link_libraries(nitfdump PRIVATE ZLIB::ZLIB)
  endif ()
  if (GDAL_USE_DEFLATE)
    target_link_libraries(nitfdump PRIVATE Deflate::Deflate)
  endif ()
  if (GDAL_USE_JPEG AND NOT GDAL_USE_JPEG_INTERNAL)
      target_sources(nitfdump PRIVATE $<TARGET_OBJECTS:libtiff_jpeg>)
  endif()
  if(GDAL_USE_JPEG OR GDAL_USE_JPEG_INTERNAL)
      if (GDAL_USE_JPEG_INTERNAL)
        target_sources(nitfdump PRIVATE $<TARGET_OBJECTS:libjpeg>)
      else ()
        target_link_libraries(nitfdump PRIVATE JPEG::JPEG)
      endif ()
      if (GDAL_USE_JPEG12_INTERNAL)
        target_sources(nitfdump PRIVATE $<TARGET_OBJECTS:libjpeg12>)
      endif ()
  endif()
  if (GDAL_USE_ZSTD)
    target_link_libraries(nitfdump PRIVATE ${ZSTD_TARGET})
  endif ()
  if (GDAL_USE_LIBLZMA)
    target_link_libraries(nitfdump PRIVATE LibLZMA::LibLZMA)
  endif ()
  if (GDAL_USE_WEBP)
    target_link_libraries(nitfdump PRIVATE WEBP::WebP)
  endif ()
  if (GDAL_USE_LERC_INTERNAL)
    target_sources(nitfdump PRIVATE $<TARGET_OBJECTS:lerc>)
  endif ()
else()
  target_link_libraries(nitfdump PRIVATE TIFF::TIFF)
endif()
target_include_directories(nitfdump PRIVATE $<TARGET_PROPERTY:gdal_NITF,SOURCE_DIR> $<TARGET_PROPERTY:gdal_GTIFF,SOURCE_DIR>)
target_link_libraries(nitfdump PRIVATE $<TARGET_NAME:${GDAL_LIB_TARGET_NAME}>)
