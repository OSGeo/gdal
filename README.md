GDAL JS
==============
An [Emscripten](https://github.com/kripken/emscripten) port of [GDAL](http://www.gdal.org).

Developing
-----------
1. Install Docker
2. Run `./scripts/setup`, which will build the Docker container.
3. Run `./scripts/make gdal`. The make script just calls `make` from inside the Docker container.
4. If you need to run `make clean`, first run `./scripts/console` and then `make clean`. In the
   future, the goal is for all make targets to run correctly from `./scripts/make`.

Usage
---------------
This library exports the following GDAL functions:
- GDALAllRegister
- GDALOpen
- GDALGetRasterCount
- GDALGetRasterXSize
- GDALGetRasterYSize
- GDALGetProjectionRef
- GDALGetGeoTransform
- OSRNewSpatialReference
- OCTNewCoordinateTransformation
- OCTTransform
(and untested)
- GDALTranslate
- GDALTranslateOptionsNew
- GDALTranslateOptionsFree

To see full-fledged examples using all of these functions from within a WebWorker, check out the
`examples` directory.

In order to limit Javascript build size, GDAL is currently built with support for GeoTIFFs only.
