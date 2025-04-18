.. _gdal_convert:

================================================================================
``gdal convert``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert a dataset.

.. Index:: gdal convert

Acts as a shortcut for :ref:`gdal_raster_convert` or
:ref:`gdal_vector_convert` depending on the nature of the specified dataset.

Synopsis
--------

.. program-output:: gdal convert --help-doc

Examples
--------

.. example::
   :title: Converting file :file:`utm.tif` to GeoPackage raster

   .. code-block:: console

       $ gdal convert utm.tif utm.gpkg
