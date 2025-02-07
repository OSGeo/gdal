.. _gdal_cli_from_python:

================================================================================
How to use "gdal" CLI algorithms from Python
================================================================================

Raster commands
---------------

* Getting information on a raster dataset as JSON

.. code-block:: python

    from osgeo import gdal
    import json

    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["info"]
    alg["input"] = "byte.tif"
    alg.Run()
    info = json.loads(alg["output-string"])
    print(info)


* Converting a georeferenced netCDF file to cloud-optimized GeoTIFF

.. code-block:: python

    from osgeo import gdal
    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["convert"]
    alg["input"] = "in.nc"
    alg["output"] = "out.tif"
    alg["output-format"] = "COG"
    alg["overwrite"] = True  # if the output file may exist
    alg.Run()
    alg.Finalize() # ensure output dataset is closed

or

.. code-block:: python

    from osgeo import gdal
    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["convert"]
    alg.ParseRunAndFinalize(["--input=in.nc", "--output-format=COG", "--output=out.tif", "--overwrite"])


* Reprojecting a GeoTIFF file to a Deflate compressed tiled GeoTIFF file

.. code-block:: python

    from osgeo import gdal
    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["reproject"]
    alg["input"] = "in.tif"
    alg["output"] = "out.tif"
    alg["dst-crs"] = "EPSG:4326"
    alg["creation-options"] = ["TILED=YES", "COMPRESS=DEFLATE"]
    alg["overwrite"] = True  # if the output file may exist
    alg.Run()
    alg.Finalize() # ensure output dataset is closed

or

.. code-block:: python

    from osgeo import gdal
    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["reproject"]
    alg.ParseRunAndFinalize(["--input=in.tif", "--output=out.tif", "--dst-crs=EPSG:4326", "--co=TILED=YES,COMPRESS=DEFLATE", "--overwrite"])


* Reprojecting a (possibly in-memory) dataset to a in-memory dataset

.. code-block:: python

    from osgeo import gdal
    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["raster"]["reproject"]
    alg["input"] = input_ds
    alg["output"] = "dummy_name"
    alg["output-format"] = "MEM"
    alg["dst-crs"] = "EPSG:4326"
    alg.Run()
    output_ds = alg["output"].GetDataset()



Vector commands
---------------

* Getting information on a vector dataset as JSON

.. code-block:: python

    from osgeo import gdal
    import json

    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["vector"]["info"]
    alg["input"] = "poly.gpkg"
    alg.Run()
    info = json.loads(alg["output-string"])
    print(info)


* Converting a shapefile to a GeoPackage

.. code-block:: python

    from osgeo import gdal
    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["vector"]["convert"]
    alg["input"] = "in.shp"
    alg["output"] = "out.gpkg"
    alg["overwrite"] = True  # if the output file may exist
    alg.Run()
    alg.Finalize() # ensure output dataset is closed

or

.. code-block:: python

    from osgeo import gdal
    gdal.UseExceptions()
    alg = gdal.GetGlobalAlgorithmRegistry()["vector"]["convert"]
    alg.ParseRunAndFinalize(["--input=in.shp", "--output=out.gpkg", "--overwrite"])
