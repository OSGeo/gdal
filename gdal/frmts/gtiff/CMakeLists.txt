add_gdal_driver(
  TARGET gdal_GTIFF
  SOURCES cogdriver.cpp
          cogdriver.h
          gtiff.h
          gt_jpeg_copy.h
          gt_citation.h
          gt_overview.h
          gt_wkt_srs.h
          gt_wkt_srs_priv.h
          gt_wkt_srs_for_gdal.h
          tifvsi.h
          tif_float.h
          geotiff.cpp
          gt_jpeg_copy.cpp
          gt_citation.cpp
          gt_overview.cpp
          gt_wkt_srs.cpp
          tifvsi.cpp
          tif_float.c
  BUILTIN)
gdal_standard_includes(gdal_GTIFF)
target_include_directories(gdal_GTIFF PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/jpeg)
option(HAVE_LERC "Enable LERC" ON)
if (GDAL_USE_LIBTIFF_INTERNAL)
  target_compile_definitions(gdal_GTIFF PRIVATE -DINTERNAL_LIBTIFF)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)
  if (RENAME_INTERNAL_LIBTIFF_SYMBOLS)
    target_compile_definitions(gdal_GTIFF PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
  endif ()
else ()
  target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_TIFF -DBIGTIFF_SUPPORT -DPIXARLOG_SUPPORT)
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES TIFF::TIFF)
endif ()

if (GDAL_USE_LIBJPEG_INTERNAL)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:libjpeg,SOURCE_DIR>)
  target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_LIBJPEG)
else ()
  target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_LIBJPEG)
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES JPEG::JPEG)
endif ()

if (GDAL_USE_LIBGEOTIFF_INTERNAL)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:geotiff,SOURCE_DIR>)
  target_compile_definitions(gdal_GTIFF PRIVATE -DINTERNAL_LIBGEOTIFF)
  if (RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
    target_compile_definitions(gdal_GITFF PRIVATE -DRENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
  endif ()
else ()
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES GEOTIFF::GEOTIFF)
endif ()

if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
else ()
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES ZLIB::ZLIB)
endif ()

if (HAVE_WEBP)
  target_compile_definitions(gdal_GTIFF PRIVATE -DWEBP_SUPPORT)
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES WEBP::WebP)
endif ()

if (HAVE_ZSTD)
  target_compile_definitions(gdal_GTIFF PRIVATE -DZSTD_SUPPORT)
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES ${ZSTD_LIBRARIES})
endif ()

gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES PROJ::PROJ)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(SOURCE geotiff.cpp gt_wkt_srs.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE gt_citation.cpp gt_wkt_srs.cpp geotiff.cpp gt_jpeg_copy.cpp tifvsi.cpp gt_overview.cpp
               PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()

if (HAVE_ZSTD)
  target_compile_definitions(gdal_GTIFF PRIVATE -DZSTD_SUPPORT)
  target_include_directories(gdal_GTIFF PRIVATE ${ZSTD_INCLUDE_DIRS})
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES ZSTD::zstd)
endif ()
