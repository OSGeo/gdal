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
          geotiff.cpp
          gt_jpeg_copy.cpp
          gt_citation.cpp
          gt_overview.cpp
          gt_wkt_srs.cpp
          tifvsi.cpp
  BUILTIN CXX_WFLAGS_EFFCXX)
gdal_standard_includes(gdal_GTIFF)
target_include_directories(gdal_GTIFF PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/jpeg)

if (GDAL_USE_LIBTIFF_INTERNAL AND NOT GDAL_USE_LIBGEOTIFF_INTERNAL)
  message(FATAL_ERROR "When GDAL_USE_LIBTIFF_INTERNAL=ON, GDAL_USE_LIBGEOTIFF_INTERNAL must be also set to ON")
endif ()

# Make sure to add first all internal libraries, so that their include path goes before external ones, that could also
# have the headers of libtiff/libgeotiff

if (GDAL_USE_LIBTIFF_INTERNAL)
  target_compile_definitions(gdal_GTIFF PRIVATE -DINTERNAL_LIBTIFF)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:libtiff,SOURCE_DIR>)
  if (RENAME_INTERNAL_LIBTIFF_SYMBOLS)
    target_compile_definitions(gdal_GTIFF PRIVATE -DRENAME_INTERNAL_LIBTIFF_SYMBOLS)
  endif ()

  if (GDAL_USE_DEFLATE)
    target_compile_definitions(gdal_GTIFF PRIVATE -DLIBDEFLATE_SUPPORT)
  endif ()

endif ()

if (GDAL_USE_LIBJPEG_INTERNAL)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:libjpeg,SOURCE_DIR>)
  target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_LIBJPEG)
  if (RENAME_INTERNAL_LIBJPEG_SYMBOLS)
    target_compile_definitions(gdal_GTIFF PRIVATE -DRENAME_INTERNAL_LIBJPEG_SYMBOLS)
  endif ()
endif ()

if (GDAL_USE_LIBGEOTIFF_INTERNAL)
  target_include_directories(gdal_GTIFF PRIVATE $<TARGET_PROPERTY:geotiff,SOURCE_DIR>)
  target_compile_definitions(gdal_GTIFF PRIVATE -DINTERNAL_LIBGEOTIFF)
  if (RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
    target_compile_definitions(gdal_GTIFF PRIVATE -DRENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)
  endif ()
endif ()

# Now deal with external dependencies

gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES PROJ::PROJ)

if (NOT GDAL_USE_LIBTIFF_INTERNAL)
  target_compile_definitions(gdal_GTIFF PRIVATE -DPIXARLOG_SUPPORT)
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES TIFF::TIFF)
endif ()

if (GDAL_ENABLE_FRMT_JPEG AND GDAL_USE_JPEG)
  target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_LIBJPEG)
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES JPEG::JPEG)
endif ()

if (NOT GDAL_USE_LIBGEOTIFF_INTERNAL)
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES GEOTIFF::GEOTIFF)
endif ()

if (GDAL_USE_LIBTIFF_INTERNAL AND GDAL_USE_JXL)
  target_sources(gdal_GTIFF PRIVATE tif_jxl.c)
  target_compile_definitions(gdal_GTIFF PRIVATE -DHAVE_JXL)
  gdal_target_link_libraries(TARGET gdal_GTIFF LIBRARIES JXL::JXL)
endif ()

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  set_property(SOURCE geotiff.cpp gt_wkt_srs.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE gt_citation.cpp gt_wkt_srs.cpp geotiff.cpp gt_jpeg_copy.cpp tifvsi.cpp gt_overview.cpp
               PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
