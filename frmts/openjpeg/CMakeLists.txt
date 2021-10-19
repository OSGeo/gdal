add_gdal_driver(
  TARGET gdal_JP2OpenJPEG
  SOURCES openjpegdataset.cpp
  DEF FRMT_openjpeg PLUGIN_CAPABLE)
gdal_standard_includes(gdal_JP2OpenJPEG)
target_include_directories(gdal_JP2OpenJPEG PRIVATE ${OPENJPEG_INCLUDE_DIRS})
gdal_target_link_libraries(TARGET gdal_JP2OpenJPEG LIBRARIES ${OPENJPEG_LIBRARIES})
