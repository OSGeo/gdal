.. _gdal_info_command:

================================================================================
"gdal info" command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Get information on a dataset.

.. Index:: gdal info

Acts as a shortcut for :ref:`gdal_raster_info_subcommand` or
:ref:`gdal_vector_info_subcommand` depending on the nature of the specified dataset.

Synopsis
--------

.. code-block::

    Usage: gdal info [OPTIONS] <INPUT>

    Return information on a dataset (shortcut for 'gdal raster info' or 'gdal vector info').

    Positional arguments:
      -i, --input <INPUT>                                  Input raster, vector or multidimensional raster dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --json-usage                                         Display usage as JSON document and exit

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format. OUTPUT-FORMAT=json|text (default: json)

    For all options, run 'gdal raster info --help' or 'gdal vector info --help'

Examples
--------

.. example::
   :title: Getting information on the file :file:`utm.tif` (with JSON output)

   .. code-block:: console

       $ gdal info utm.tif

.. example::
   :title: Getting information on the file :file:`poly.gpkg` (with text output), listing all features

   .. code-block:: console

       $ gdal info --format=text --features poly.gpkg
