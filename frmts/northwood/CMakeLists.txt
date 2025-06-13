# There are NWT_GRC and NWT_GRD drivers
add_gdal_driver(
  TARGET gdal_northwood
  DEF FRMT_northwood
  SOURCES grddataset.cpp grcdataset.cpp northwood.cpp northwood.h
  BUILTIN)
gdal_standard_includes(gdal_northwood)

if (NOT OGR_ENABLE_DRIVER_TAB OR OGR_ENABLE_DRIVER_TAB_PLUGIN)
  target_compile_definitions(gdal_northwood PRIVATE -DNO_MITAB_SUPPORT)
endif()
