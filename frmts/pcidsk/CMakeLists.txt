add_gdal_driver(
  TARGET gdal_PCIDSK
  SOURCES pcidskdataset2.h
          pcidskdataset2.cpp
          gdal_edb.cpp
          vsi_pcidsk_io.cpp
          ogrpcidsklayer.cpp
          PLUGIN_CAPABLE_IF
          "NOT GDAL_USE_JPEG_INTERNAL")
gdal_standard_includes(gdal_PCIDSK)

target_include_directories(gdal_PCIDSK PRIVATE $<TARGET_PROPERTY:pcidsk_sdk,SOURCE_DIR>)
add_subdirectory(sdk)

if (NOT GDAL_USE_JPEG_INTERNAL)
  gdal_target_link_libraries(gdal_PCIDSK PRIVATE JPEG::JPEG)
endif ()
