#!/bin/sh
gdal_translate ../gcore/data/epsg26711_geotiff1_1.tif epsg26711_geotiff1_1.tif -co GEOTIFF_VERSION=1.1
gdal_translate ../gcore/data/epsg4326_3855_geotiff1_1.tif epsg4326_3855_geotiff1_1.tif -co GEOTIFF_VERSION=1.1
