add_gdal_driver(
  TARGET gdal_JPEG
  SOURCES vsidataio.h jpgdataset.h vsidataio.cpp jpgdataset.cpp
  BUILTIN)
gdal_standard_includes(gdal_JPEG)

target_include_directories(gdal_JPEG PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem)
if (GDAL_USE_JPEG12_INTERNAL)
  target_sources(gdal_JPEG PRIVATE jpgdataset_12.cpp vsidataio_12.cpp)
  target_include_directories(gdal_JPEG PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(gdal_JPEG PRIVATE JPEG_DUAL_MODE_8_12)
endif ()

if (GDAL_USE_JPEG_INTERNAL)
  gdal_add_vendored_lib(gdal_JPEG libjpeg)
else ()
  gdal_target_link_libraries(gdal_JPEG PRIVATE JPEG::JPEG)
endif ()

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(gdal_JPEG libz)
else ()
  gdal_target_link_libraries(gdal_JPEG PRIVATE ZLIB::ZLIB)
endif ()
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.16)
  # These files has CPL_CVSID("$id$") which break ODR and UNITY_BUILD
  set_property(SOURCE jpgdataset.cpp vsidataio.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endif ()
