include(GdalDriverHelper)

add_library(
  libtiff OBJECT
  tif_dir.c
  tif_fax3.c
  tif_lzma.c
  tif_predict.h
  tif_vsi.c
  tiffiop.h
  t4.h
  tif_dir.h
  tif_fax3.h
  tif_lzw.c
  tif_print.c
  tif_warning.c
  tiffvers.h
  tif_aux.c
  tif_dirinfo.c
  tif_fax3sm.c
  tif_next.c
  tif_read.c
  tif_write.c
  uvcode.h
  tif_close.c
  tif_dirread.c
  tif_flush.c
  tif_strip.c
  tif_zip.c
  tif_codec.c
  tif_dirwrite.c
  tif_getimage.c
  tif_open.c
  tif_swab.c
  tif_zstd.c
  tif_webp.c
  tif_color.c
  tif_dumpmode.c
  tif_packbits.c
  tif_thunder.c
  tiff.h
  tif_compress.c
  tif_error.c
  tif_pixarlog.c
  tif_tile.c
  tiffconf.h
  tif_config.h
  tif_extension.c
  tif_luv.c
  tif_predict.c
  tif_version.c
  tif_lerc.c
  tiffio.h)
target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:cpl,SOURCE_DIR> $<TARGET_PROPERTY:cpl,BINARY_DIR>)
target_compile_definitions(libtiff PRIVATE -DDONT_DEPRECATE_SPRINTF -DHOST_FILLORDER=FILLORDER_LSB2MSB)
target_compile_options(libtiff PRIVATE ${GDAL_C_WARNING_FLAGS})

if (MSVC)
  # Suppress '<unnamed-tag>': structure was padded due to alignment specifier in tif_jpeg.c
  target_compile_options(libtiff PRIVATE /wd4324)
endif ()

target_compile_definitions(libtiff PRIVATE -DPIXARLOG_SUPPORT -DZIP_SUPPORT)

if (GDAL_USE_JPEG_INTERNAL OR GDAL_USE_JPEG)
  target_compile_definitions(libtiff PRIVATE -DJPEG_SUPPORT -DOJPEG_SUPPORT)
endif ()

# Include first internal libraries
if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(libtiff libz)
endif ()

if (GDAL_USE_JPEG_INTERNAL)
  target_sources(libtiff PRIVATE tif_jpeg.c tif_ojpeg.c)
  gdal_add_vendored_lib(libtiff libjpeg)
endif ()

if (GDAL_USE_LERC_INTERNAL)
  target_compile_definitions(libtiff PRIVATE -DLERC_SUPPORT)
  gdal_add_vendored_lib(libtiff lerc)
endif ()

# Now external libraries
if (NOT GDAL_USE_ZLIB_INTERNAL)
  gdal_target_link_libraries(libtiff PRIVATE ZLIB::ZLIB)
endif ()

if (NOT GDAL_USE_JPEG_INTERNAL)
  # Use a separate object library so that external libjpeg headers are only applied to tif_jpeg.c and tif_ojpeg.c, but
  # not tif_jpeg_12.c !
  add_library(libtiff_jpeg OBJECT tif_jpeg.c tif_ojpeg.c)
  target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:libtiff_jpeg>)
  target_compile_options(libtiff_jpeg PRIVATE ${GDAL_C_WARNING_FLAGS})
  set_property(TARGET libtiff_jpeg PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
  target_compile_definitions(libtiff_jpeg PRIVATE -DJPEG_SUPPORT -DOJPEG_SUPPORT)
  target_include_directories(libtiff_jpeg PRIVATE $<TARGET_PROPERTY:cpl,SOURCE_DIR> $<TARGET_PROPERTY:cpl,BINARY_DIR>)
  gdal_target_link_libraries(libtiff_jpeg PRIVATE JPEG::JPEG)
  if (GDAL_USE_JPEG12_INTERNAL)
    target_compile_definitions(libtiff_jpeg PRIVATE -DJPEG_DUAL_MODE_8_12)
  endif ()
  if (RENAME_INTERNAL_TIFF_SYMBOLS)
    target_compile_definitions(libtiff_jpeg PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
  endif ()
endif ()

if (GDAL_USE_DEFLATE)
  target_compile_definitions(libtiff PRIVATE -DLIBDEFLATE_SUPPORT)
  gdal_target_link_libraries(libtiff PRIVATE Deflate::Deflate)
endif ()

if (GDAL_USE_JPEG12_INTERNAL)
  target_sources(libtiff PRIVATE tif_jpeg_12.c)
  target_compile_definitions(libtiff PRIVATE -DJPEG_SUPPORT -DJPEG_DUAL_MODE_8_12)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:libjpeg12,BINARY_DIR>)
endif ()

if (GDAL_USE_LIBLZMA)
  target_compile_definitions(libtiff PRIVATE -DLZMA_SUPPORT)
  gdal_target_link_libraries(libtiff PRIVATE LibLZMA::LibLZMA)
endif ()

if (GDAL_USE_ZSTD)
  target_compile_definitions(libtiff PRIVATE -DZSTD_SUPPORT)
  gdal_target_link_libraries(libtiff PRIVATE ${ZSTD_TARGET})
endif ()

if (GDAL_USE_WEBP)
  target_compile_definitions(libtiff PRIVATE -DWEBP_SUPPORT)
  gdal_target_link_libraries(libtiff PRIVATE WEBP::WebP)
endif ()

if (NOT GDAL_USE_LERC_INTERNAL AND GDAL_USE_LERC)
  target_compile_definitions(libtiff PRIVATE -DLERC_SUPPORT)
  gdal_target_link_libraries(libtiff PRIVATE LERC::LERC)
endif ()

if (RENAME_INTERNAL_TIFF_SYMBOLS)
  target_compile_definitions(libtiff PUBLIC -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
endif ()

if (MSVC)
  if (CMAKE_CL_64)
    target_compile_definitions(libtiff PRIVATE -D_WIN64)
  endif ()
endif ()

set_target_properties(libtiff PROPERTIES CXX_STANDARD 11)
set_property(TARGET libtiff PROPERTY POSITION_INDEPENDENT_CODE ${GDAL_OBJECT_LIBRARIES_POSITION_INDEPENDENT_CODE})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:libtiff>)
