add_gdal_driver(
  TARGET gdal_JP2OpenJPEG
  SOURCES openjpegdataset.cpp
  CORE_SOURCES openjpegdrivercore.cpp
  DEF FRMT_openjpeg PLUGIN_CAPABLE
  NO_SHARED_SYMBOL_WITH_CORE)

if(TARGET gdal_JP2OpenJPEG_core)
    target_include_directories(gdal_JP2OpenJPEG_core PRIVATE $<TARGET_PROPERTY:${OpenJPEG_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(gdal_JP2OpenJPEG_core PRIVATE $<TARGET_PROPERTY:${OpenJPEG_TARGET},INTERFACE_COMPILE_DEFINITIONS>)
endif()

if(NOT TARGET gdal_JP2OpenJPEG)
    return()
endif()

gdal_standard_includes(gdal_JP2OpenJPEG)
target_include_directories(gdal_JP2OpenJPEG PRIVATE ../opjlike)
gdal_target_link_libraries(gdal_JP2OpenJPEG PRIVATE ${OpenJPEG_TARGET})
