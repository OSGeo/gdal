GDAL JS
==============
An [Emscripten](https://github.com/kripken/emscripten) port of [GDAL](http://www.gdal.org).

Developing
-----------
1. Install Docker
2. Run `./scripts/setup`, which will build the Docker container.
3. Run `./scripts/make gdal`. The make script just calls `make` from inside the Docker container.
4. `./scripts/make clean` works as expected.
5. To package up a release, run `./scripts/make VERSION=<number> release`

Usage
---------------
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
- GDALGetProjectionRef
- GDALSetProjection
- GDALGetGeoTransform
- GDALSetGeoTransform
- OSRNewSpatialReference
- OSRImportFromEPSG
- OCTNewCoordinateTransformation
- OCTTransform
- GDALCreateGenImgProjTransformer
- GDALGenImgProjTransform
- GDALDestroyGenImgProjTransformer
- GDALSuggestedWarpOutput
- GDALTranslate
- GDALTranslateOptionsNew
- GDALTranslateOptionsFree
- GDALReprojectImage

To see full-fledged examples using all of these functions from within a WebWorker, check out the
`examples` directory. From simplest to most complex, these examples are:

1. `inspect_geotiff`
2. `map_extent`
3. `thumbnail`
4. `thumbnail_map`
5. `tile_tiff`

In order to limit Javascript build size, GDAL is currently built with support for GeoTIFFs and PNGs only.
