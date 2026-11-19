add_gdal_driver(
  TARGET gdal_DTED
  SOURCES dted_api.h
          dted_api.c
          dted_create.c
          dteddataset.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(gdal_DTED)
