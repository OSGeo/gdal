add_gdal_driver(
  TARGET gdal_MEM
  SOURCES gdal_mem.h memdataset.cpp memdataset.h ogrmemlayer.cpp
  BUILTIN)
gdal_standard_includes(gdal_MEM)

include(TargetPublicHeader)
target_public_header(TARGET gdal HEADERS memdataset.h gdal_mem.h)
