add_gdal_driver(
  TARGET gdal_PALSARJaxa
  DEF FRMT_jaxapalsar DRIVER_NAME_OPTION JAXAPALSAR
  SOURCES jaxapalsardataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_PALSARJaxa)
