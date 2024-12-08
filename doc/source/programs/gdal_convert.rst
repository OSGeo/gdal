.. _gdal_convert_command:

================================================================================
"gdal convert" command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert a dataset.

.. Index:: gdal convert

Acts as a shortcut for :ref:`gdal_raster_convert_subcommand` or
:ref:`gdal_vector_convert_subcommand` depending on the nature of the specified dataset.

Synopsis
--------

.. code-block::

    Usage: gdal convert [OPTIONS] <INPUT> <OUTPUT>

    Convert a dataset (shortcut for 'gdal raster convert' or 'gdal vector convert').

    Positional arguments:
      -i, --input <INPUT>                                  Input raster, vector or multidimensional raster dataset [required]
      -o, --output <OUTPUT>                                Output raster, vector or multidimensional raster dataset (created by algorithm) [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format

    For all options, run 'gdal raster convert --help' or 'gdal vector convert --help'

Examples
--------

.. example::
   :title: Converting file :file:`utm.tif` to GeoPackage raster

   .. code-block:: console

       $ gdal convert utm.tif utm.gpkg
