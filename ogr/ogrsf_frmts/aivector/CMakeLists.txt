add_gdal_driver(
  TARGET ogr_AIVector
  SOURCES
      ograivectordriver.cpp
  PLUGIN_CAPABLE
  NO_DEPS
)

gdal_standard_includes(ogr_AIVector)
