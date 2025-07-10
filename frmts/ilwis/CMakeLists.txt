add_gdal_driver(TARGET gdal_ILWIS SOURCES ilwisdataset.h ilwisdataset.cpp ilwiscoordinatesystem.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(gdal_ILWIS)
