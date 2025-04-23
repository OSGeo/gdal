.. _gdal_info:

================================================================================
``gdal info``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Get information on a dataset.

.. Index:: gdal info

Acts as a shortcut for :ref:`gdal_raster_info` or
:ref:`gdal_vector_info` depending on the nature of the specified dataset.

Synopsis
--------

.. program-output:: gdal info --help-doc

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
