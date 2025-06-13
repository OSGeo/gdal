add_gdal_driver(
  TARGET ogr_GTFS
  SOURCES ogrgtfsdriver.cpp
  PLUGIN_CAPABLE NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
)
gdal_standard_includes(ogr_GTFS)

