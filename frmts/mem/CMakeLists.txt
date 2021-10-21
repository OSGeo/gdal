add_gdal_driver(
  TARGET gdal_MEM
  SOURCES memdataset.cpp memdataset.h
  BUILTIN)
gdal_standard_includes(gdal_MEM)

include(TargetPublicHeader)
target_public_header(TARGET gdal HEADERS memdataset.h)
