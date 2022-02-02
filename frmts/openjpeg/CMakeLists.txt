add_gdal_driver(
  TARGET gdal_JP2OpenJPEG
  SOURCES openjpegdataset.cpp
  DEF FRMT_openjpeg PLUGIN_CAPABLE)
gdal_standard_includes(gdal_JP2OpenJPEG)
gdal_target_link_libraries(gdal_JP2OpenJPEG PRIVATE OPENJPEG::OpenJPEG)
