add_gdal_driver(
  TARGET ogr_GTFS
  SOURCES ogrgtfsdriver.cpp
  PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(ogr_GTFS)

