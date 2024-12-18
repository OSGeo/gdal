.. _gdal_program:

================================================================================
gdal
================================================================================

.. versionadded:: 3.11

.. only:: html

    Main "gdal" entry point.

.. Index:: gdal

Synopsis
--------

.. code-block::

    Usage: gdal <COMMAND>
    where <COMMAND> is one of:
      - convert:  Convert a dataset (shortcut for 'gdal raster convert' or 'gdal vector convert').
      - info:     Return information on a dataset (shortcut for 'gdal raster info' or 'gdal vector info').
      - pipeline: Execute a pipeline (shortcut for 'gdal vector pipeline').
      - raster:   Raster commands.
      - vector:   Vector commands.

    'gdal <FILENAME>' can also be used as a shortcut for 'gdal info <FILENAME>'.
    And 'gdal read <FILENAME> ! ...' as a shortcut for 'gdal pipeline <FILENAME> ! ...'.

Examples
--------

.. example::
   :title: Getting information on the file :file:`utm.tif` (with JSON output)

   .. code-block:: console

       $ gdal info utm.tif

.. example::
   :title: Converting file :file:`utm.tif` to GeoPackage raster

   .. code-block:: console

       $ gdal convert utm.tif utm.gpkg

.. example::
   :title: Getting information on all available commands and subcommands as a JSON document.

   .. code-block:: console

       $ gdal --json-usage

.. example::
   :title: Getting list of all formats supported by the current GDAL build, as text

   .. code-block:: console

       $ gdal --formats

.. example::
   :title: Getting list of all formats supported by the current GDAL build, as JSON.

   .. code-block:: console

       $ gdal --formats --json
