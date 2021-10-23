add_library(
  libtiff STATIC
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
  tif_ojpeg.c
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
  tif_jpeg.c
  tif_packbits.c
  tif_thunder.c
  tiff.h
  tif_compress.c
  tif_error.c
  tif_jpeg_12.c
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
target_compile_definitions(libtiff PRIVATE -DDONT_DEPRECATE_SPRINTF)

if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
  target_compile_definitions(libtiff PRIVATE -DPIXARLOG_SUPPORT -DZIP_SUPPORT)
else ()
  target_link_libraries(libtiff PRIVATE ZLIB::ZLIB)
  target_compile_definitions(libtiff PRIVATE -DPIXARLOG_SUPPORT -DZIP_SUPPORT)
endif ()

if (GDAL_USE_LIBJPEG_INTERNAL)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:libjpeg,SOURCE_DIR>)
else ()
  target_link_libraries(libtiff PRIVATE JPEG::JPEG)
endif ()

if (GDAL_JPEG12_SUPPORTED)
  target_compile_definitions(libtiff PRIVATE -DJPEG_DUAL_MODE_8_12)
endif ()
target_compile_definitions(libtiff PRIVATE -DJPEG_SUPPORT -DOJPEG_SUPPORT)

if (HAVE_LIBLZMA)
  target_compile_definitions(libtiff PRIVATE -DLZMA_SUPPORT)
  target_link_libraries(libtiff PRIVATE LibLZMA::LibLZMA)
endif ()

if (HAVE_ZSTD)
  target_compile_definitions(libtiff PRIVATE -DZSTD_SUPPORT)
  target_include_directories(libtiff PRIVATE ${ZSTD_INCLUDE_DIRS})
  target_link_libraries(libtiff PRIVATE ZSTD::zstd)
endif ()

if (HAVE_WEBP)
  target_compile_definitions(libtiff PRIVATE -DWEBP_SUPPORT)
  target_link_libraries(GDAL_LINK_LIBRARY INTERFACE WEBP::WebP)
endif ()

if (HAVE_LERC)
  target_compile_definitions(libtiff PRIVATE -DLERC_SUPPORT)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:lerc,SOURCE_DIR>)
endif ()

if (RENAME_INTERNAL_LIBTIFF_SYMBOLS)
  target_compile_definitions(libtiff PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
  target_sources(libtiff PRIVATE gdal_libtiff_symbol_rename.h)
endif ()
if (MSVC)
  if (CMAKE_CL_64)
    target_compile_definitions(libtiff PRIVATE -D_WIN64)
  endif ()
endif ()

set_target_properties(libtiff PROPERTIES CXX_STANDARD 11)
target_link_libraries(GDAL_LINK_LIBRARY INTERFACE libtiff)
set_property(TARGET libtiff PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET libtiff PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# ifeq ($(RENAME_INTERNAL_LIBTIFF_SYMBOLS),yes) ../../o/tif_jpeg.$(OBJ_EXT) : tif_jpeg.c t4.h tif_config.h tif_dir.h
# tif_fax3.h tif_predict.h tiff.h tiffconf.h tiffio.h tiffiop.h tiffvers.h uvcode.h sed "s/defined(TIFFInitJPEG)/0/" <
# tif_jpeg.c > tmp_tif_jpeg.c $(CC) -c -I../../port $(ALL_C_FLAGS) tmp_tif_jpeg.c -o $@ rm tmp_tif_jpeg.c
#
# ../../o/tif_jpeg_12.$(OBJ_EXT) : tif_jpeg_12.c t4.h tif_config.h tif_dir.h tif_fax3.h tif_predict.h tiff.h tiffconf.h
# tiffio.h tiffiop.h tiffvers.h uvcode.h sed "s/#  define TIFFInitJPEG/#undef TIFFInitJPEG\n#  define TIFFInitJPEG/" <
# tif_jpeg_12.c > tmp_tif_jpeg_12.c $(CC) -c -I../../port $(ALL_C_FLAGS) tmp_tif_jpeg_12.c -o $@ rm tmp_tif_jpeg_12.c
# endif
