# Driver gdal_GSAG and gdal_GSBG are built in
add_gdal_driver(
  TARGET gdal_GSG
  SOURCES gs7bgdataset.cpp gsagdataset.cpp gsbgdataset.cpp
  BUILTIN)
gdal_standard_includes(gdal_GSG)
