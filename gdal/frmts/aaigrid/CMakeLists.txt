# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

# it provides AAIGrid and GRASSASCIIGrid
add_gdal_driver(
  TARGET gdal_AAIGrid
  SOURCES aaigriddataset.cpp aaigriddataset.h
  BUILTIN)
gdal_standard_includes(gdal_AAIGrid)
