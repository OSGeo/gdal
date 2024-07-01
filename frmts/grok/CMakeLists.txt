add_gdal_driver(
  TARGET gdal_JP2Grok
  SOURCES grokdataset.cpp
  DEF FRMT_grok PLUGIN_CAPABLE)
gdal_standard_includes(gdal_JP2Grok)
target_include_directories(gdal_JP2Grok PRIVATE ../opjlike)
gdal_target_link_libraries(gdal_JP2Grok PRIVATE GROK::Grok)
