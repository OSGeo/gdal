:orphan:

.. _solution_reproject:

Exercise solution for raster reprojection
=========================================

::

    $ gdal raster reproject TDR_rgb_byte_clamped.gdalg.json TDR_3035_with_ovr.tif --input-nodata 0 --add-alpha --creation-option TILED=YES --creation-option COMPRESS=JPEG --output-crs EPSG:3035 -r cubic 

    $ gdal raster overview add -r cubic  TDR_3035_with_ovr.tif

or:

::

    $ gdal raster reproject TDR_rgb_byte_clamped.gdalg.json TDR_3035_cog.tif --input-nodata 0 --add-alpha --format COG --creation-option COMPRESS=JPEG --output-crs EPSG:3035 -r cubic 
