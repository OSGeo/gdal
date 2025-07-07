add_gdal_driver(
  TARGET ogr_Selafin
  SOURCES io_selafin.cpp
          io_selafin.h
          ogr_selafin.h
          ogrselafindatasource.cpp
          ogrselafindriver.cpp
          ogrselafinlayer.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(ogr_Selafin)
