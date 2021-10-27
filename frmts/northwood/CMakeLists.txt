# There are NWT_GRC and NWT_GRD drivers
add_gdal_driver(
  TARGET gdal_northwood
  DEF FRMT_northwood
  SOURCES grddataset.cpp grcdataset.cpp northwood.cpp northwood.h
  BUILTIN STRONG_CXX_WFLAGS)
gdal_standard_includes(gdal_northwood)
