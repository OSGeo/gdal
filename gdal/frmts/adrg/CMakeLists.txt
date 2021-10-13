# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt. There are gdal_ADRG and
# gdal_SRP driver
add_gdal_driver(
  TARGET gdal_ADRG
  SOURCES adrgdataset.cpp srpdataset.cpp
  BUILTIN)
gdal_standard_includes(gdal_ADRG)
target_include_directories(gdal_ADRG PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
