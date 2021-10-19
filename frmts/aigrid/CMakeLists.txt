# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

add_gdal_driver(
  TARGET gdal_AIGrid
  SOURCES aigrid.h aigccitt.c aigdataset.cpp aigopen.c gridlib.c
  BUILTIN)
gdal_standard_includes(gdal_AIGrid)
target_include_directories(gdal_AIGrid PRIVATE $<TARGET_PROPERTY:ogr_AVC,SOURCE_DIR>
                                               $<TARGET_PROPERTY:ogr_Shape,SOURCE_DIR>)
add_executable(aitest EXCLUDE_FROM_ALL aitest.c)
