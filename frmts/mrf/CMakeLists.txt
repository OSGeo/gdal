add_gdal_driver(
  TARGET gdal_MRF
  SOURCES BitMask2D.h
          JPEG_band.cpp
          PNG_band.cpp
          marfa.h
          marfa_dataset.cpp
          mrf_band.cpp
          mrf_overview.cpp
          mrf_util.cpp
          Packer.h
          Packer_RLE.cpp
          Packer_RLE.h
          JPNG_band.cpp
          Tif_band.cpp
  BUILTIN)
gdal_standard_includes(gdal_MRF)

# Internal libs first
if (GDAL_USE_JPEG12_INTERNAL)
  target_sources(gdal_MRF PRIVATE JPEG12_band.cpp)
  target_compile_definitions(gdal_MRF PRIVATE -DJPEG12_SUPPORTED)
  target_include_directories(gdal_MRF PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
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

if (NOT GDAL_USE_JPEG_INTERNAL)
  gdal_target_link_libraries(gdal_MRF PRIVATE JPEG::JPEG)
endif ()

if (NOT GDAL_USE_PNG_INTERNAL)
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