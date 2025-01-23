.. _gdal_raster_reproject_subcommand:

================================================================================
"gdal raster reproject" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Reproject a raster dataset.

.. Index:: gdal raster reproject

Synopsis
--------

.. code-block::

    Usage: gdal raster reproject [OPTIONS] <INPUT> <OUTPUT>

    Reproject a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]
      -o, --output <OUTPUT>                                Output raster dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --version                                            Display GDAL version and exit
      --json-usage                                         Display usage as JSON document and exit
      --drivers                                            Display driver list as JSON document and exit
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      -s, --src-crs <SRC-CRS>                              Source CRS
      -d, --dst-crs <DST-CRS>                              Destination CRS
      -r, --resampling <RESAMPLING>                        Resampling method. RESAMPLING=nearest|bilinear|cubic|cubicspline|lanczos|average|rms|mode|min|max|med|q1|q3|sum
      --resolution <xres>,<yres>                           Target resolution (in destination CRS units)
      --bbox <xmin>,<ymin>,<xmax>,<ymax>                   Target bounding box (in destination CRS units)
      --target-aligned-pixels                              Round target extent to target resolution

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]


Description
-----------

:program:`gdal raster reproject` can be used to reproject a raster dataset.

Examples
--------

.. example::
   :title: Reproject a GeoTIFF file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N")

   .. code-block:: bash

        $ gdal raster reproject --dst-crs=EPSG:32632 in.tif out.tif --overwrite
