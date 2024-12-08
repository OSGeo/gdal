.. _gdal_raster_convert_subcommand:

================================================================================
"gdal raster convert" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert a raster dataset.

.. Index:: gdal raster convert

Synopsis
--------

.. code-block::

    Usage: gdal raster convert [OPTIONS] <INPUT> <OUTPUT>

    Convert a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]
      -o, --output <OUTPUT>                                Output raster dataset (created by algorithm) [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
                                                           Mutually exclusive with --append
      --append                                             Append as a subdataset to existing output
                                                           Mutually exclusive with --overwrite

    Advanced Options:
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]

Description
-----------

:program:`gdal raster convert` can be used to convert raster data between
different formats.

The following options are available:

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: --append

    Append input raster as a new subdataset to existing output file.
    Only works with drivers that support adding subdatasets such as
    :ref:`raster.gtiff` and :ref:`raster.gpkg`

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Converting file :file:`utm.tif` to a cloud-optimized GeoTIFF using JPEG compression

   .. code-block:: console

       $ gdal raster convert --format=COG --co COMPRESS=JPEG utm.tif utm_cog.tif

.. example::
   :title: Converting file :file:`utm.tif` to GeoPackage raster

   .. code-block:: console

       $ gdal raster convert utm.tif utm.gpkg
