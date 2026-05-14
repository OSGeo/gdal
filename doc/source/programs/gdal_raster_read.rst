.. _gdal_raster_read:

================================================================================
``gdal raster pipeline read``
================================================================================

.. versionadded:: 3.11

.. only:: html

   Read a raster dataset (pipeline only)

.. Index:: gdal raster pipeline read

Description
-----------

The ``read`` operation is for use in a :ref:`gdal_pipeline` only, and reads a single input
raster dataset. This is the first step of a pipeline.

Synopsis
--------

.. program-output:: gdal raster pipeline --help-doc=read

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

Examples
--------

.. example::
   :title: Read a GeoTIFF file

   .. code-block:: bash

        $ gdal raster pipeline read input.tif ! ... [other commands here] ...
