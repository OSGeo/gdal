add_gdal_driver(
  TARGET ogr_OAPIF
  SOURCES ogroapifdriver.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  STRONG_CXX_WFLAGS)
gdal_standard_includes(ogr_OAPIF)
target_include_directories(ogr_OAPIF PRIVATE $<TARGET_PROPERTY:gmlutils,SOURCE_DIR>)
