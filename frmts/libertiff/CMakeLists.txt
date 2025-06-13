add_gdal_driver(
  TARGET gdal_LIBERTIFF
  SOURCES libertiffdataset.cpp
  PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_LERC_INTERNAL\\\;NOT GDAL_USE_ZLIB_INTERNAL"
  NO_DEPS)

gdal_standard_includes(gdal_LIBERTIFF)
target_include_directories(gdal_LIBERTIFF PRIVATE
    ${GDAL_RASTER_FORMAT_SOURCE_DIR}/gtiff/libtiff
    ${PROJECT_SOURCE_DIR}/third_party/libertiff
)

# Include first internal libraries
if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(gdal_LIBERTIFF libz)
endif ()

if (GDAL_USE_LERC_INTERNAL)
  target_compile_definitions(gdal_LIBERTIFF PRIVATE -DLERC_SUPPORT)
  gdal_add_vendored_lib(gdal_LIBERTIFF lerc)
endif ()

# Now external libraries

if (NOT GDAL_USE_ZLIB_INTERNAL)
  gdal_target_link_libraries(gdal_LIBERTIFF PRIVATE ZLIB::ZLIB)
endif ()

if (GDAL_USE_DEFLATE)
  target_compile_definitions(gdal_LIBERTIFF PRIVATE -DLIBDEFLATE_SUPPORT)
  gdal_target_link_libraries(gdal_LIBERTIFF PRIVATE Deflate::Deflate)
endif ()

if (NOT GDAL_USE_LERC_INTERNAL AND GDAL_USE_LERC)
  target_compile_definitions(gdal_LIBERTIFF PRIVATE -DLERC_SUPPORT)
  gdal_target_link_libraries(gdal_LIBERTIFF PRIVATE LERC::LERC)
endif ()

if (GDAL_USE_ZSTD)
  target_compile_definitions(gdal_LIBERTIFF PRIVATE -DZSTD_SUPPORT)
  gdal_target_link_libraries(gdal_LIBERTIFF PRIVATE ${ZSTD_TARGET})
endif ()

