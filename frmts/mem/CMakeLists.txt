add_gdal_driver(
  TARGET gdal_MEM
  SOURCES memdataset.cpp memdataset.h
  BUILTIN)
gdal_standard_includes(gdal_MEM)
