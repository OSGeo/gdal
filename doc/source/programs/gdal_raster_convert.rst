.. _gdal_raster_convert:

================================================================================
``gdal raster convert``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert a raster dataset.

.. Index:: gdal raster convert

Synopsis
--------

.. program-output:: gdal raster convert --help-doc

Description
-----------

:program:`gdal raster convert` can be used to convert raster data between
different formats.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst

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
