add_gdal_driver(
  TARGET gdal_GTIFF
  SOURCES cogdriver.cpp
          cogdriver.h
          gtiff.h
          gtiffbitmapband.h
          gtiffbitmapband.cpp
          gtiffdataset.h
          gtiffdataset.cpp
          gtiffdataset_read.cpp
          gtiffdataset_write.cpp
          gtiffjpegoverviewds.h
          gtiffjpegoverviewds.cpp
          gtiffoddbitsband.h
          gtiffoddbitsband.cpp
          gtiffrasterband.h
          gtiffrasterband.cpp
          gtiffrasterband_read.cpp
          gtiffrasterband_write.cpp
          gtiffrgbaband.h
          gtiffrgbaband.cpp
          gtiffsplitband.h
          gtiffsplitband.cpp
          gtiffsplitbitmapband.h
          gtiffsplitbitmapband.cpp
          gt_jpeg_copy.h
          gt_citation.h
          gt_overview.h
          gt_wkt_srs.h
          gt_wkt_srs_priv.h
          gt_wkt_srs_for_gdal.h
          tifvsi.h
          geotiff.cpp
          gt_jpeg_copy.cpp
          gt_citation.cpp
          gt_overview.cpp
          gt_wkt_srs.cpp
          tifvsi.cpp
  BUILTIN STRONG_CXX_WFLAGS)
gdal_standard_includes(gdal_GTIFF)

if (GDAL_ENABLE_DRIVER_JPEG)
  target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_LIBJPEG)
  target_include_directories(gdal_GTIFF PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/jpeg)
endif ()

if (GDAL_USE_TIFF_INTERNAL AND NOT GDAL_USE_GEOTIFF_INTERNAL)
  message(FATAL_ERROR "When GDAL_USE_TIFF_INTERNAL=ON, GDAL_USE_GEOTIFF_INTERNAL must be also set to ON")
endif ()

# Make sure to add first all internal libraries, so that their include path goes before external ones, that could also
# have the headers of libtiff/libgeotiff

if (GDAL_USE_TIFF_INTERNAL)
  gdal_add_vendored_lib(gdal_GTIFF libtiff)
  target_compile_definitions(gdal_GTIFF PRIVATE -DINTERNAL_LIBTIFF)

  if (GDAL_USE_DEFLATE)
    target_compile_definitions(gdal_GTIFF PRIVATE -DLIBDEFLATE_SUPPORT)
  endif ()

endif ()

if (GDAL_USE_JPEG_INTERNAL)
  gdal_add_vendored_lib(gdal_GTIFF libjpeg)
endif ()

if (GDAL_USE_GEOTIFF_INTERNAL)
  gdal_add_vendored_lib(gdal_GTIFF geotiff)
  target_compile_definitions(gdal_GTIFF PRIVATE -DINTERNAL_LIBGEOTIFF)
endif ()

if (GDAL_USE_LERC_INTERNAL)
  # Just for the sake of reporting the liblerc version in internal metadata
  target_compile_definitions(gdal_GTIFF PRIVATE -DLERC_SUPPORT)
  gdal_add_vendored_lib(gdal_GTIFF lerc)
endif ()

# Now deal with external dependencies

# Include libjpeg first so that if we want to use jpeg-turbo on homebrew
# /usr/local/opt/jpeg-turbo gets included first, before /usr/local/opt
# triggered by libtiff, that contains libjpeg 9e
if (NOT GDAL_USE_JPEG_INTERNAL AND GDAL_USE_JPEG)
  gdal_target_link_libraries(gdal_GTIFF PRIVATE JPEG::JPEG)
endif ()

gdal_target_link_libraries(gdal_GTIFF PRIVATE PROJ::proj)

if (NOT GDAL_USE_TIFF_INTERNAL)
  target_compile_definitions(gdal_GTIFF PRIVATE -DPIXARLOG_SUPPORT)
  gdal_target_link_libraries(gdal_GTIFF PRIVATE TIFF::TIFF)
endif ()

if (NOT GDAL_USE_GEOTIFF_INTERNAL)
  gdal_target_link_libraries(gdal_GTIFF PRIVATE ${GeoTIFF_TARGET})
endif ()

if (GDAL_USE_JXL)
  if (GDAL_USE_TIFF_INTERNAL)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES ${JXL_LIBRARIES})
    check_function_exists(JxlEncoderFrameSettingsSetOption HAVE_JxlEncoderFrameSettingsSetOption)
    check_function_exists(JxlEncoderFrameSettingsCreate HAVE_JxlEncoderFrameSettingsCreate)
    check_function_exists(JxlEncoderSetFrameLossless HAVE_JxlEncoderSetFrameLossless)
    check_function_exists(JxlEncoderSetFrameDistance HAVE_JxlEncoderSetFrameDistance)
    check_function_exists(JxlEncoderSetCodestreamLevel HAVE_JxlEncoderSetCodestreamLevel)
    check_function_exists(JxlEncoderInitExtraChannelInfo HAVE_JxlEncoderInitExtraChannelInfo)
    check_function_exists(JxlEncoderSetExtraChannelDistance HAVE_JxlEncoderSetExtraChannelDistance)
    cmake_pop_check_state()
    target_sources(gdal_GTIFF PRIVATE tif_jxl.c)
    target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JXL)
    if (HAVE_JxlEncoderFrameSettingsSetOption)
      target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JxlEncoderFrameSettingsSetOption)
    endif ()
    if (HAVE_JxlEncoderFrameSettingsCreate)
      target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JxlEncoderFrameSettingsCreate)
    endif ()
    if (HAVE_JxlEncoderSetFrameLossless)
      target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JxlEncoderSetFrameLossless)
    endif ()
    if (HAVE_JxlEncoderSetFrameDistance)
      target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JxlEncoderSetFrameDistance)
    endif ()
    if (HAVE_JxlEncoderSetCodestreamLevel)
      target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JxlEncoderSetCodestreamLevel)
    endif ()
    if (HAVE_JxlEncoderInitExtraChannelInfo)
      target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JxlExtraChannels)
    endif ()
    if (HAVE_JxlEncoderSetExtraChannelDistance)
      target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JxlEncoderSetExtraChannelDistance)
    endif ()
    gdal_target_link_libraries(gdal_GTIFF PRIVATE JXL::JXL)
  else ()
    message(WARNING "Cannot build JXL as a TIFF codec as it requires building with -DGDAL_USE_TIFF_INTERNAL=ON")
  endif ()
endif ()

if (GDAL_USE_WEBP)
  target_compile_definitions(gdal_GTIFF PRIVATE -DWEBP_SUPPORT)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:WEBP::WebP,INTERFACE_INCLUDE_DIRECTORIES>)
endif ()

if (NOT GDAL_USE_LERC_INTERNAL AND GDAL_USE_LERC)
  target_compile_definitions(gdal_GTIFF PRIVATE -DLERC_SUPPORT)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:LERC::LERC,INTERFACE_INCLUDE_DIRECTORIES>)
endif ()

if(GDAL_USE_JPEG OR GDAL_USE_JPEG_INTERNAL)
    add_executable(generate_quant_table_md5sum EXCLUDE_FROM_ALL generate_quant_table_md5sum.cpp)
    gdal_standard_includes(generate_quant_table_md5sum)
    target_link_libraries(generate_quant_table_md5sum PRIVATE ${GDAL_LIB_TARGET_NAME})
    if (GDAL_USE_TIFF_INTERNAL)
      target_sources(generate_quant_table_md5sum PRIVATE $<TARGET_OBJECTS:libtiff> ../../port/cpl_md5.cpp)
      target_include_directories(generate_quant_table_md5sum PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)
      if (RENAME_INTERNAL_TIFF_SYMBOLS)
        target_compile_definitions(generate_quant_table_md5sum PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
      endif ()
      if (GDAL_USE_ZLIB_INTERNAL)
        target_sources(generate_quant_table_md5sum PRIVATE $<TARGET_OBJECTS:libz>)
      else ()
        target_link_libraries(generate_quant_table_md5sum PRIVATE ZLIB::ZLIB)
      endif ()
      if (GDAL_USE_DEFLATE)
        target_link_libraries(generate_quant_table_md5sum PRIVATE Deflate::Deflate)
      endif ()
      if (GDAL_USE_JPEG_INTERNAL)
        target_sources(generate_quant_table_md5sum PRIVATE $<TARGET_OBJECTS:libjpeg>)
      else ()
        target_link_libraries(generate_quant_table_md5sum PRIVATE JPEG::JPEG)
      endif ()
      if (GDAL_USE_JPEG12_INTERNAL)
        target_sources(generate_quant_table_md5sum PRIVATE $<TARGET_OBJECTS:libjpeg12>)
      endif ()
      if (GDAL_USE_ZSTD)
        target_link_libraries(generate_quant_table_md5sum PRIVATE ${ZSTD_TARGET})
      endif ()
      if (GDAL_USE_LIBLZMA)
        target_link_libraries(generate_quant_table_md5sum PRIVATE LibLZMA::LibLZMA)
      endif ()
      if (GDAL_USE_WEBP)
        target_link_libraries(generate_quant_table_md5sum PRIVATE WEBP::WebP)
      endif ()
      if (GDAL_USE_LERC_INTERNAL)
        target_sources(generate_quant_table_md5sum PRIVATE $<TARGET_OBJECTS:lerc>)
      endif ()
    else ()
      target_link_libraries(generate_quant_table_md5sum PRIVATE TIFF::TIFF)
    endif ()
endif()
