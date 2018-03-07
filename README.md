GDAL JS
==============
An [Emscripten](https://github.com/kripken/emscripten) port of [GDAL](http://www.gdal.org) 2.1.

Usage
---------------
*Caution!* It is strongly recommended to run this code inside of a web worker.
To see complete examples for how to do this, checkout the `examples` directory.
From simplest to most complex, these are:

1. `inspect_geotiff`
2. `map_extent`
3. `thumbnail`
4. `thumbnail_map`
5. `tile_tiff`

If you want to use GDAL from within a Node application, you are probably looking
for [https://www.npmjs.com/package/gdal](https://www.npmjs.com/package/gdal).

This library exports the following GDAL functions:
- CSLCount
- GDALSetCacheMax
- GDALAllRegister
- GDALOpen
- GDALClose
- GDALGetDriverByName
- GDALCreate
- GDALCreateCopy
- GDALGetRasterXSize
- GDALGetRasterYSize
- GDALGetRasterCount
- GDALGetRasterDataType
- GDALGetRasterBand
- GDALGetRasterStatistics
- GDALGetRasterMinimum
- GDALGetRasterMaximum
- GDALGetRasterNoDataValue
- GDALGetProjectionRef
- GDALSetProjection
- GDALGetGeoTransform
- GDALSetGeoTransform
- OSRNewSpatialReference
- OSRDestroySpatialReference
- OSRImportFromEPSG
- OCTNewCoordinateTransformation
- OCTDestroyCoordinateTransformation
- OCTTransform
- GDALCreateGenImgProjTransformer
- GDALDestroyGenImgProjTransformer
- GDALGenImgProjTransform
- GDALDestroyGenImgProjTransformer
- GDALSuggestedWarpOutput
- GDALTranslate
- GDALTranslateOptionsNew
- GDALTranslateOptionsFree
- GDALWarpAppOptionsNew
- GDALWarpAppOptionsSetProgress
- GDALWarpAppOptionsFree
- GDALWarp
- GDALReprojectImage
- CPLError
- CPLSetErrorHandler
- CPLQuietErrorHandler
- CPLErrorReset
- CPLGetLastErrorMsg
- CPLGetLastErrorNo
- CPLGetLastErrorType

For documentation of these functions' behavior, please see the
[GDAL documentation](http://www.gdal.org/gdal_8h.html)

In order to limit build size, GDAL is currently built with support for GeoTIFFs and PNGs only.

Developing
-----------
1. Install Docker
2. Run `./scripts/setup`, which will build the Docker container.
3. Run `./scripts/make gdal`. The make script just calls `make` from inside the Docker container.
4. `./scripts/make clean` works as expected.
5. To package up a release, run `./scripts/make VERSION=<number> release`
