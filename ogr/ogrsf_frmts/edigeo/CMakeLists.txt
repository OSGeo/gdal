add_gdal_driver(TARGET ogr_EDIGEO SOURCES ogredigeodatasource.cpp ogredigeodriver.cpp ogr_edigeo.h ogredigeolayer.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_EDIGEO)
