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

.. program-output:: gdal raster --help-doc

Available sub-commands
----------------------

- :ref:`gdal_raster_info_subcommand`
- :ref:`gdal_raster_calc_subcommand`
- :ref:`gdal_raster_clip_subcommand`
- :ref:`gdal_raster_convert_subcommand`
- :ref:`gdal_raster_mosaic_subcommand`
- :ref:`gdal_raster_overview_subcommand`
- :ref:`gdal_raster_pipeline_subcommand`
- :ref:`gdal_raster_reproject_subcommand`
- :ref:`gdal_raster_resize_subcommand`
- :ref:`gdal_raster_scale_subcommand`
- :ref:`gdal_raster_select_subcommand`
- :ref:`gdal_raster_stack_subcommand`
- :ref:`gdal_raster_unscale_subcommand`

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
