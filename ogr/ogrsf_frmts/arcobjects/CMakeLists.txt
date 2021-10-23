add_gdal_driver(
  TARGET ogr_ao
  DEF ARCOBJECTS_ENABLED
  PLUGIN
  SOURCES aodatasource.cpp aodriver.cpp aolayer.cpp aoutils.cpp aoutils.h ogr_ao.h)
gdal_standard_includes(ogr_ao)
