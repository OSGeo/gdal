Raster recipes
==============

Clip a raster using a vector cutline
------------------------------------

**Using GDAL CLI**

.. code-block:: console

   gdal raster clip input.tif output.tif --cutline boundary.geojson

**Using Python**

.. code-block:: python

   from osgeo import gdal

   gdal.Warp(
       "output.tif",
       "input.tif",
       cutlineDSName="boundary.geojson",
       cropToCutline=True
   )
