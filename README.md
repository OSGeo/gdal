GDAL JS
==============
An [Emscripten](https://github.com/kripken/emscripten) port of [GDAL](http://www.gdal.org).

Developing
-----------
1. Install Docker
2. Run `./scripts/setup`, which will build the Docker container.
3. Run `./scripts/make gdal`. The make script just calls `make` from inside the Docker container.

Usage
---------------
This library exports the following GDAL functions:
- GDALOpen
- GDALGetRasterCount
- GDALGetRasterXSize
- GDALGetRasterYSize
- GDALGetProjectionRef
- GDALGetGeoTransform

To see a full-fledged example using all of these functions from within a WebWorker, check out the
`examples/inspect_geotiff` directory.

In order to limit Javascript build size, GDAL is currently built with support for GeoTIFFs only.
