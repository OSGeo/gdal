# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt. There are gdal_ADRG and
# gdal_SRP driver
add_gdal_driver(
  TARGET gdal_ADRG
  SOURCES adrgdataset.cpp srpdataset.cpp
  BUILTIN
  NO_CXX_WFLAGS_EFFCXX
  NO_WFLAG_OLD_STYLE_CAST
)
gdal_standard_includes(gdal_ADRG)
target_include_directories(gdal_ADRG PRIVATE $<TARGET_PROPERTY:gdal_iso8211,SOURCE_DIR>)
