.. _gdal_raster_command:

================================================================================
"gdal raster" command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Entry point for raster commands

.. Index:: gdal raster

Synopsis
--------

.. code-block::

    Usage: gdal raster <SUBCOMMAND>
    where <SUBCOMMAND> is one of:
      - convert: Convert a raster dataset.
      - info:    Return information on a raster dataset.
      - pipeline:  Process a raster dataset.
      - reproject: Reproject a raster dataset.

Available sub-commands
----------------------

- :ref:`gdal_raster_info_subcommand`
- :ref:`gdal_raster_convert_subcommand`
- :ref:`gdal_raster_pipeline_subcommand`
- :ref:`gdal_raster_reproject_subcommand`

Examples
--------

.. example::
   :title: Getting information on the file :file:`utm.tif` (with JSON output)

   .. code-block:: console

       $ gdal raster info utm.tif

.. example::
   :title: Converting file :file:`utm.tif` to GeoPackage raster

   .. code-block:: console

       $ gdal raster convert utm.tif utm.gpkg
