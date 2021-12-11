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
target_compile_options(libtiff PRIVATE ${GDAL_C_WARNING_FLAGS})

if (MSVC)
  # Suppress '<unnamed-tag>': structure was padded due to alignment specifier in tif_jpeg.c
  target_compile_options(libtiff PRIVATE /wd4324)
endif ()

target_compile_definitions(libtiff PRIVATE -DPIXARLOG_SUPPORT -DZIP_SUPPORT)
target_compile_definitions(libtiff PRIVATE -DJPEG_SUPPORT -DOJPEG_SUPPORT)

# Include first internal libraries
if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
endif ()

if (GDAL_USE_LIBJPEG_INTERNAL)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:libjpeg,SOURCE_DIR>)
  if (RENAME_INTERNAL_LIBJPEG_SYMBOLS)
    target_compile_definitions(libtiff PRIVATE -DRENAME_INTERNAL_LIBJPEG_SYMBOLS)
  endif ()
endif ()

if (GDAL_USE_LIBLERC_INTERNAL)
  target_compile_definitions(libtiff PRIVATE -DLERC_SUPPORT)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:lerc,SOURCE_DIR>)
endif ()

# Now external libraries
if (NOT GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:ZLIB::ZLIB,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(ZLIB::ZLIB)
endif ()

if (NOT GDAL_USE_LIBJPEG_INTERNAL)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:JPEG::JPEG,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(JPEG::JPEG)
endif ()

if (GDAL_USE_DEFLATE)
  target_compile_definitions(libtiff PRIVATE -DLIBDEFLATE_SUPPORT)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:Deflate::Deflate,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(Deflate::Deflate)
endif ()

if (GDAL_JPEG12_SUPPORTED)
  target_compile_definitions(libtiff PRIVATE -DJPEG_DUAL_MODE_8_12)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:libjpeg12,BINARY_DIR>)
endif ()

if (GDAL_USE_LIBLZMA)
  target_compile_definitions(libtiff PRIVATE -DLZMA_SUPPORT)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:LibLZMA::LibLZMA,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(LibLZMA::LibLZMA)
endif ()

if (GDAL_USE_ZSTD)
  target_compile_definitions(libtiff PRIVATE -DZSTD_SUPPORT)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:ZSTD::zstd,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(ZSTD::zstd)
endif ()

if (GDAL_USE_WEBP)
  target_compile_definitions(libtiff PRIVATE -DWEBP_SUPPORT)
  target_include_directories(libtiff PRIVATE $<TARGET_PROPERTY:WEBP::WebP,INTERFACE_INCLUDE_DIRECTORIES>)
  gdal_add_private_link_libraries(WEBP::WebP)
endif ()

if (RENAME_INTERNAL_LIBTIFF_SYMBOLS)
  target_compile_definitions(libtiff PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
endif ()

if (MSVC)
  if (CMAKE_CL_64)
    target_compile_definitions(libtiff PRIVATE -D_WIN64)
  endif ()
endif ()

set_target_properties(libtiff PROPERTIES CXX_STANDARD 11)
set_property(TARGET libtiff PROPERTY POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS})
target_sources(${GDAL_LIB_TARGET_NAME} PRIVATE $<TARGET_OBJECTS:libtiff>)
