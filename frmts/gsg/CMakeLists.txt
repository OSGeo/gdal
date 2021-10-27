# Driver gdal_GSAG and gdal_GSBG are built in
add_gdal_driver(
  TARGET gdal_GSG
  SOURCES gs7bgdataset.cpp gsagdataset.cpp gsbgdataset.cpp
  BUILTIN)
# FIXME should be 'PLUGIN_CAPABLE NO_DEPS' but requires some code to glue the registration of the 3 drivers
gdal_standard_includes(gdal_GSG)
