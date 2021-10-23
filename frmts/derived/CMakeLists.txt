add_gdal_driver(
  TARGET gdal_Derived
  SOURCES deriveddataset.cpp derivedlist.c derivedlist.h
  BUILTIN)
gdal_standard_includes(gdal_Derived)
