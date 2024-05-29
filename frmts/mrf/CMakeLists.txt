add_gdal_driver(
  TARGET gdal_MRF
  SOURCES BitMask2D.h
          marfa.h
          marfa_dataset.cpp
          mrf_band.cpp
          mrf_overview.cpp
          mrf_util.cpp
          Packer.h
          Packer_RLE.cpp
          Packer_RLE.h
          Tif_band.cpp
  CORE_SOURCES
          mrfdrivercore.cpp
  PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_JPEG_INTERNAL\\\;NOT GDAL_USE_JPEG12_INTERNAL\\\;NOT GDAL_USE_LERC_INTERNAL\\\;NOT GDAL_USE_PNG_INTERNAL\\\;NOT GDAL_USE_TIFF_INTERNAL\\\;NOT GDAL_USE_ZLIB_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE
)

if(TARGET gdal_MRF_core)
    if (GDAL_USE_LERC OR GDAL_USE_LERC_INTERNAL)
        target_compile_definitions(gdal_MRF_core PRIVATE -DLERC)
    endif()
endif()

if(NOT TARGET gdal_MRF)
    return()
endif()

gdal_standard_includes(gdal_MRF)

if (GDAL_USE_JPEG OR GDAL_USE_JPEG_INTERNAL)
  target_compile_definitions(gdal_MRF PRIVATE -DHAVE_JPEG)
  target_sources(gdal_MRF PRIVATE JPEG_band.cpp)
endif()

if (GDAL_USE_PNG OR GDAL_USE_PNG_INTERNAL)
  target_compile_definitions(gdal_MRF PRIVATE -DHAVE_PNG)
  target_sources(gdal_MRF PRIVATE PNG_band.cpp)
endif()

if ((GDAL_USE_JPEG OR GDAL_USE_JPEG_INTERNAL) AND (GDAL_USE_PNG OR GDAL_USE_PNG_INTERNAL))
  target_sources(gdal_MRF PRIVATE JPNG_band.cpp)
  endif()

# Internal libs first
if ((GDAL_USE_JPEG OR GDAL_USE_JPEG_INTERNAL) AND GDAL_USE_JPEG12_INTERNAL)
  target_sources(gdal_MRF PRIVATE JPEG12_band.cpp)
  target_compile_definitions(gdal_MRF PRIVATE -DJPEG12_SUPPORTED)
  target_include_directories(gdal_MRF PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  set_property(SOURCE JPEG12_band.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
elseif (GDAL_USE_JPEG AND NOT GDAL_USE_JPEG_INTERNAL AND HAVE_JPEGTURBO_DUAL_MODE_8_12)
  target_sources(gdal_MRF PRIVATE JPEG12_band.cpp)
  target_compile_definitions(gdal_MRF PRIVATE JPEG12_SUPPORTED HAVE_JPEGTURBO_DUAL_MODE_8_12)
endif ()

if (GDAL_USE_JPEG_INTERNAL)
  gdal_add_vendored_lib(gdal_MRF libjpeg)
endif ()

if (GDAL_USE_PNG_INTERNAL)
  gdal_add_vendored_lib(gdal_MRF libpng)
endif ()

if (GDAL_USE_ZLIB_INTERNAL)
  gdal_add_vendored_lib(gdal_MRF libz)
endif ()

if (GDAL_USE_LERC OR GDAL_USE_LERC_INTERNAL)
  target_sources(gdal_MRF PRIVATE LERC_band.cpp)
  gdal_add_vendored_lib(gdal_MRF PRIVATE liblercv1)
  target_compile_definitions(gdal_MRF PRIVATE -DLERC)
  add_subdirectory(LERCV1)
endif ()

if (GDAL_USE_LERC_INTERNAL)
  gdal_add_vendored_lib(gdal_MRF lerc)
endif ()

# Now external libs

# Include libjpeg first so that if we want to use jpeg-turbo on homebrew
# /usr/local/opt/jpeg-turbo gets included first, before /usr/local/opt
# triggered by libtiff, that contains libjpeg 9e
if (GDAL_USE_JPEG AND NOT GDAL_USE_JPEG_INTERNAL)
  gdal_target_link_libraries(gdal_MRF PRIVATE JPEG::JPEG)
endif ()

if (GDAL_USE_PNG AND NOT GDAL_USE_PNG_INTERNAL)
  gdal_target_link_libraries(gdal_MRF PRIVATE PNG::PNG)
endif ()

if (NOT GDAL_USE_ZLIB_INTERNAL)
  gdal_target_link_libraries(gdal_MRF PRIVATE ZLIB::ZLIB)
endif ()

if (NOT GDAL_USE_LERC_INTERNAL AND GDAL_USE_LERC)
  gdal_target_link_libraries(gdal_MRF PRIVATE LERC::LERC)
endif ()

if (GDAL_USE_ZSTD)
  target_compile_definitions(gdal_MRF PRIVATE -DZSTD_SUPPORT)
  gdal_target_link_libraries(gdal_MRF PRIVATE ${ZSTD_TARGET})
endif ()

if (GDAL_USE_BRUNSLI)
  target_compile_definitions(gdal_MRF PRIVATE -DBRUNSLI)
  gdal_target_link_libraries(gdal_MRF PRIVATE BRUNSLI::ENCODE BRUNSLI::DECODE)
endif ()

if (GDAL_USE_LIBQB3)
  target_sources(gdal_MRF PRIVATE QB3_band.cpp)
  target_compile_definitions(gdal_MRF PRIVATE -DQB3_SUPPORT)
  gdal_target_link_libraries(gdal_MRF PRIVATE QB3::libQB3)
endif()

