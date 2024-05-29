add_gdal_driver(
  TARGET gdal_PCIDSK
  SOURCES pcidskdataset2.h
          pcidskdataset2.cpp
          gdal_edb.cpp
          vsi_pcidsk_io.cpp
          ogrpcidsklayer.cpp
  CORE_SOURCES
          pcidskdrivercore.cpp
  PLUGIN_CAPABLE_IF "NOT GDAL_USE_JPEG_INTERNAL"
  NO_SHARED_SYMBOL_WITH_CORE)

if(NOT TARGET gdal_PCIDSK)
    return()
endif()

gdal_standard_includes(gdal_PCIDSK)

target_include_directories(gdal_PCIDSK PRIVATE $<TARGET_PROPERTY:pcidsk_sdk,SOURCE_DIR>)

set(PCIDSK_INTERNAL ON)
add_subdirectory(sdk)

target_compile_definitions(gdal_PCIDSK PRIVATE -DPCIDSK_INTERNAL)

if (GDAL_USE_JPEG AND NOT GDAL_USE_JPEG_INTERNAL)
  gdal_target_link_libraries(gdal_PCIDSK PRIVATE JPEG::JPEG)
endif ()
