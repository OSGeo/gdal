# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.

add_gdal_driver(TARGET gdal_AirSAR SOURCES airsardataset.cpp)
gdal_standard_includes(gdal_AirSAR)
