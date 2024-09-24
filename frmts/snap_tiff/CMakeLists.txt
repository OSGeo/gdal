add_gdal_driver(
  TARGET gdal_SNAP_TIFF
  SOURCES snaptiffdriver.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  STRONG_CXX_FLAGS
)

gdal_standard_includes(gdal_SNAP_TIFF)
