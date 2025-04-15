.. _gdal_cli_from_python:

================================================================================
How to use "gdal" CLI algorithms from Python
================================================================================

Raster commands
---------------

* Getting information on a raster dataset as JSON

.. code-block:: python

    from osgeo import gdal

    gdal.UseExceptions()
    with gdal.run(["raster", "info"], {"input": "byte.tif"}) as info:
        print(info)


* Converting a georeferenced netCDF file to cloud-optimized GeoTIFF

.. code-block:: python

    from osgeo import gdal

    gdal.UseExceptions()
    with gdal.run(["raster", "convert"], {"input": "in.tif", "output": "out.tif", "output-format": "COG", "overwrite": True}):
        pass

or

.. code-block:: python

    from osgeo import gdal

    gdal.UseExceptions()
    with gdal.run(["raster", "convert"], input="in.tif", output="out.tif", output_format="COG", overwrite=True):
        pass


* Reprojecting a GeoTIFF file to a Deflate compressed tiled GeoTIFF file

.. code-block:: python

    from osgeo import gdal

    gdal.UseExceptions()
    with gdal.run(["raster", "reproject"], {"input": "in.tif", "output": "out.tif", "dst-crs": "EPSG:4326", "creation-options": { "TILED": "YES", "COMPRESS": "DEFLATE"} }):
        pass


* Reprojecting a (possibly in-memory) dataset to a in-memory dataset

.. code-block:: python

    from osgeo import gdal

    gdal.UseExceptions()
    with gdal.run(["raster", "reproject"], {"input": "in.tif", "output-format": "MEM", "dst-crs": "EPSG:4326"}) as ds:
        print(ds.ReadAsArray())


Vector commands
---------------

* Getting information on a vector dataset as JSON

.. code-block:: python

    from osgeo import gdal

    gdal.UseExceptions()
    with gdal.run(["raster", "info"], {"input": "poly.gpkg"}) as info:
        print(info)


* Converting a shapefile to a GeoPackage

.. code-block:: python

    from osgeo import gdal

    gdal.UseExceptions()
    with gdal.run(["raster", "convert"], {"input": "in.shp", "output": "out.gpkg", "overwrite": True}):
        pass
