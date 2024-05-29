add_gdal_driver(
  TARGET gdal_JPEG
  SOURCES vsidataio.h jpgdataset.h vsidataio.cpp jpgdataset.cpp
  CORE_SOURCES jpegdrivercore.cpp
  PLUGIN_CAPABLE_IF
      "NOT GDAL_USE_JPEG_INTERNAL\\\;NOT GDAL_USE_JPEG12_INTERNAL\\\;NOT GDAL_USE_ZLIB_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE
)

if(TARGET gdal_JPEG_core)
    target_include_directories(gdal_JPEG_core PRIVATE $<TARGET_PROPERTY:JPEG::JPEG,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_JPEG_core PRIVATE $<TARGET_PROPERTY:JPEG::JPEG,INTERFACE_COMPILE_DEFINITIONS>)
    if (HAVE_JPEGTURBO_DUAL_MODE_8_12)
        target_compile_definitions(gdal_JPEG_core PRIVATE JPEG_DUAL_MODE_8_12 HAVE_JPEGTURBO_DUAL_MODE_8_12)
    endif()
endif()

if(NOT TARGET gdal_JPEG)
    return()
endif()

gdal_standard_includes(gdal_JPEG)

target_include_directories(gdal_JPEG PRIVATE ${GDAL_RASTER_FORMAT_SOURCE_DIR}/mem ${GDAL_RASTER_FORMAT_SOURCE_DIR}/gtiff)
if (GDAL_USE_JPEG12_INTERNAL)
  target_sources(gdal_JPEG PRIVATE jpgdataset_12.cpp vsidataio_12.cpp)
  set_property(SOURCE jpgdataset_12.cpp vsidataio_12.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
  target_include_directories(gdal_JPEG PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(gdal_JPEG PRIVATE JPEG_DUAL_MODE_8_12)
endif ()

if (GDAL_USE_JPEG_INTERNAL)
  gdal_add_vendored_lib(gdal_JPEG libjpeg)
endif()

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(gdal_JPEG libz)
endif()

# Include libjpeg first so that if we want to use jpeg-turbo on homebrew
# /usr/local/opt/jpeg-turbo gets included first, before /usr/local/opt
# that contains libjpeg 9e
if (NOT GDAL_USE_JPEG_INTERNAL)
  gdal_target_link_libraries(gdal_JPEG PRIVATE JPEG::JPEG)
  if (HAVE_JPEGTURBO_DUAL_MODE_8_12 AND NOT GDAL_USE_JPEG12_INTERNAL)
    target_sources(gdal_JPEG PRIVATE jpgdataset_12.cpp vsidataio_12.cpp)
    target_compile_definitions(gdal_JPEG PRIVATE JPEG_DUAL_MODE_8_12 HAVE_JPEGTURBO_DUAL_MODE_8_12)
  endif()
endif ()

if (NOT GDAL_USE_ZLIB_INTERNAL)
  gdal_target_link_libraries(gdal_JPEG PRIVATE ZLIB::ZLIB)
endif ()

# Breaks ODR
set_property(SOURCE jpgdataset.cpp vsidataio.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

