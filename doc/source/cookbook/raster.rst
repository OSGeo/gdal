Raster recipes
==============

Clip a raster using a vector cutline
------------------------------------

**Using GDAL CLI**

.. code-block:: console

   gdalwarp -cutline boundary.geojson -crop_to_cutline input.tif output.tif

**Using Python**

.. code-block:: python

   from osgeo import gdal

   gdal.Warp(
       "output.tif",
       "input.tif",
       cutlineDSName="boundary.geojson",
       cropToCutline=True
   )
