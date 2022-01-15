add_gdal_driver(
  TARGET gdal_JP2Grok
  SOURCES grokdataset.cpp
  DEF FRMT_grok PLUGIN_CAPABLE)
gdal_standard_includes(gdal_JP2Grok)
gdal_target_link_libraries(TARGET gdal_JP2Grok LIBRARIES GROK::Grok)
