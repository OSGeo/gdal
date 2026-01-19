add_gdal_driver(
  TARGET gdal_E57
  SOURCES e57driver.cpp
  PLUGIN_CAPABLE
  NO_DEPS
)
gdal_standard_includes(gdal_E57)
