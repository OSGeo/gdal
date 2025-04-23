add_gdal_driver(
  TARGET ogr_AIVector
  SOURCES
      ograivectordriver.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  STRONG_CXX_WFLAGS)

gdal_standard_includes(ogr_AIVector)
