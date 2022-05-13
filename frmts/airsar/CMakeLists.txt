# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.

add_gdal_driver(TARGET gdal_AirSAR SOURCES airsardataset.cpp PLUGIN_CAPABLE NO_DEPS)
gdal_standard_includes(gdal_AirSAR)
