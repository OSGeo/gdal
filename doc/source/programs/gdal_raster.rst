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
      - calc:      Perform raster algebra.
      - clip:      Clip a raster dataset.
      - contour:   Creates a vector contour from a raster elevation model (DEM).
      - convert:   Convert a raster dataset.
      - edit:      Edit a raster dataset.
      - info:      Return information on a raster dataset.
      - mosaic:    Build a mosaic, either virtual (VRT) or materialized.
      - overview:  Manage overviews of a raster dataset.
      - pipeline:  Process a raster dataset.
      - reproject: Reproject a raster dataset.
      - resize:    Resize a raster dataset.
      - stack:     Combine together input bands into a multi-band output, either virtual (VRT) or materialized.


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
- :ref:`gdal_raster_stack_subcommand`

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
