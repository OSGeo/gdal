.. _gdal_cli_gdalg:

================================================================================
.gdalg files to replay serialized ``gdal`` commands
================================================================================

A subset of subcommands of :program:`gdal` support generating
on-the-fly / streamed raster of vector datasets. They are typically saved to
a "real" output dataset, but it is also possible to save them in a JSON file
of extension ``.gdalg.json``, that can be read by the :ref:`raster.gdalg` driver or
:ref:`vector.gdalg` driver to apply the saved processing on-the-fly and use
the resulting dataset as a regular input dataset for :cpp:func:`GDALOpenEx`,
or anywhere in GDAL API or command line utilities where a raster input dataset
is expected. GDALG files are conceptually close to :ref:`VRT (Virtual) files <raster.vrt>`,
although the implementation is substantially different.

.. note:: GDALG is the contraction of GDAL and ALGorithm.

In-process stream execution
---------------------------

For algorithms that support GDALG output, it is also possible to use the
``stream`` output format to indicate that a raster or vector streamed dataset
must be returned.

For example the following snippet, runs that "gdal vector set-type"
algorithm on a source dataset and iterates over features from the returned
streamed dataset.

.. code-block:: python

    from osgeo import gdal
    gdal.UseExceptions()

    alg = gdal.GetGlobalAlgorithmRegistry()["vector"]["geom"]["set-type"]
    alg["input"] = src_ds
    alg["output"] = ""
    alg["output-format"] = "stream"
    alg["geometry-type"] = "LINESTRING Z"

    alg.Run()

    out_ds = alg["output"].GetDataset()
    out_lyr = out_ds.GetLayer(0)
    for f in our_lyr:
        f.DumpReadable()


Examples
--------

.. example::
   :title: Serialize the command of a reprojection of a GeoPackage file in a GDALG file, and later read it

   .. code-block:: bash

        $ gdal vector pipeline ! read in.gpkg ! reproject --dst-crs=EPSG:32632 ! write in_epsg_32632.gdalg.json --overwrite
        $ gdal vector info in_epsg_32632.gdalg.json

    The content of :file:`in_epsg_32632.gdalg.json` is:

    .. code-block:: json

        {
            "type": "gdal_streamed_alg",
            "command_line": "gdal vector pipeline ! read in.gpkg ! reproject --dst-crs=EPSG:32632"
        }
