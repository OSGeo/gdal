GDAL JS
==============
An [Emscripten](https://github.com/kripken/emscripten) port of [GDAL](http://www.gdal.org).

Developing
-----------
1. Install Emscripten and its dependencies following the instructions
[here](https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html).
2. If you are on a Mac, you may need to run the following if you receive errors like this: `env:
python2: No such file or directory`
```
./emsdk install sdk-incoming-64bit --build=Release
./emsdk activate sdk-incoming-64bit
```
3. From the project directory, run `make gdal`

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
