add_gdal_driver(
  TARGET gdal_PALSARJaxa
  DEF FRMT_jaxapalsar DRIVER_NAME_OPTION JAXAPALSAR
  SOURCES jaxapalsardataset.cpp
  PLUGIN_CAPABLE
  NO_DEPS
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(gdal_PALSARJaxa)
