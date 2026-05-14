.. _gdal_raster_write:

================================================================================
``gdal raster pipeline write``
================================================================================

.. versionadded:: 3.11

.. only:: html

   Write a raster dataset (pipeline only)

.. Index:: gdal raster pipeline write

Description
-----------

The ``write`` operation is for use in a :ref:`gdal_pipeline` only, and writes a
raster dataset. This is the last step of a pipeline.

To write a temporary dataset in the middle of a pipeline, use :ref:`gdal_raster_materialize`.

Synopsis
--------

.. program-output:: gdal raster pipeline --help-doc=write

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/append_raster.rst

Examples
--------

.. example::
   :title: Write a GeoTIFF file

   .. code-block:: bash

        $ gdal raster pipeline ... [other commands here] ... ! write out.tif --overwrite
