.. _gdal_raster:

================================================================================
``gdal raster``
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

- :ref:`gdal_raster_info`
- :ref:`gdal_raster_aspect`
- :ref:`gdal_raster_blend`
- :ref:`gdal_raster_calc`
- :ref:`gdal_raster_clean_collar`
- :ref:`gdal_raster_clip`
- :ref:`gdal_raster_color_map`
- :ref:`gdal_raster_compare`
- :ref:`gdal_raster_convert`
- :ref:`gdal_raster_create`
- :ref:`gdal_raster_footprint`
- :ref:`gdal_raster_fill_nodata`
- :ref:`gdal_raster_hillshade`
- :ref:`gdal_raster_index`
- :ref:`gdal_raster_mosaic`
- :ref:`gdal_raster_neighbors`
- :ref:`gdal_raster_nodata_to_alpha`
- :ref:`gdal_raster_overview`
- :ref:`gdal_raster_pipeline`
- :ref:`gdal_raster_pixel_info`
- :ref:`gdal_raster_polygonize`
- :ref:`gdal_raster_proximity`
- :ref:`gdal_raster_reproject`
- :ref:`gdal_raster_resize`
- :ref:`gdal_raster_rgb_to_palette`
- :ref:`gdal_raster_roughness`
- :ref:`gdal_raster_scale`
- :ref:`gdal_raster_select`
- :ref:`gdal_raster_sieve`
- :ref:`gdal_raster_slope`
- :ref:`gdal_raster_stack`
- :ref:`gdal_raster_tile`
- :ref:`gdal_raster_tpi`
- :ref:`gdal_raster_tri`
- :ref:`gdal_raster_unscale`
- :ref:`gdal_raster_update`
- :ref:`gdal_raster_viewshed`

Examples
--------

.. example::
   :title: Getting information on the file :file:`utm.tif` (with text output)

   .. code-block:: console

       $ gdal raster info utm.tif

.. example::
   :title: Converting file :file:`utm.tif` to GeoPackage raster

   .. code-block:: console

       $ gdal raster convert utm.tif utm.gpkg

.. example::
   :title: Getting the list of raster drivers (with JSON output)

   .. code-block:: console

       $ gdal raster --drivers
