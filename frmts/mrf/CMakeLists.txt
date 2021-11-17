add_gdal_driver(
  TARGET gdal_MRF
  SOURCES BitMask2D.h
          JPEG_band.cpp
          PNG_band.cpp
          LERC_band.cpp
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
if (GDAL_JPEG12_SUPPORTED)
  target_sources(gdal_MRF PRIVATE JPEG12_band.cpp)
  target_compile_definitions(gdal_MRF PRIVATE -DJPEG12_SUPPORTED)
  target_include_directories(gdal_MRF PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif ()

if (GDAL_USE_LIBJPEG_INTERNAL)
  target_include_directories(gdal_MRF PRIVATE $<TARGET_PROPERTY:libjpeg,SOURCE_DIR>)
endif ()

if (GDAL_USE_LIBPNG_INTERNAL)
  target_include_directories(gdal_MRF PRIVATE $<TARGET_PROPERTY:libpng,SOURCE_DIR>)
  target_compile_definitions(gdal_MRF PRIVATE -DINTERNAL_PNG)
endif ()

if (GDAL_USE_LIBZ_INTERNAL)
  target_include_directories(gdal_MRF PRIVATE $<TARGET_PROPERTY:libz,SOURCE_DIR>)
endif ()

if (GDAL_USE_LIBLERC_INTERNAL)
  target_sources(gdal_MRF PRIVATE LERC_band.cpp)
  target_include_directories(gdal_MRF PRIVATE $<TARGET_PROPERTY:liblerc,SOURCE_DIR> $<TARGET_PROPERTY:lerc,SOURCE_DIR>)
  target_compile_definitions(gdal_MRF PRIVATE -DLERC)
endif ()

if (GDAL_USE_LIBLERC_INTERNAL)
  add_subdirectory(LERCV1)
endif ()

# Now external libs

if (NOT GDAL_USE_LIBJPEG_INTERNAL)
  gdal_target_link_libraries(TARGET gdal_MRF LIBRARIES JPEG::JPEG)
endif ()

if (NOT GDAL_USE_LIBPNG_INTERNAL)
  gdal_target_link_libraries(TARGET gdal_MRF LIBRARIES PNG::PNG)
endif ()

if (NOT GDAL_USE_LIBZ_INTERNAL)
  gdal_target_link_libraries(TARGET gdal_MRF LIBRARIES ZLIB::ZLIB)
endif ()

if (GDAL_USE_ZSTD)
  target_compile_definitions(gdal_MRF PRIVATE -DZSTD_SUPPORT)
  gdal_target_link_libraries(TARGET gdal_MRF LIBRARIES ZSTD::zstd)
endif ()
