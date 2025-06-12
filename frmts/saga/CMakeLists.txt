add_gdal_driver(TARGET gdal_SAGA SOURCES sagadataset.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(gdal_SAGA)
