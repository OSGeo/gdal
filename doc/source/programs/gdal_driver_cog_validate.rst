.. _gdal_driver_cog_validate:

================================================================================
``gdal driver cog validate``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Validate if a TIFF file is a Cloud Optimized GeoTIFF

.. Index:: gdal driver cog validate

Synopsis
--------

.. program-output:: gdal driver cog validate --help-doc

Description
-----------

Validate if a TIFF file is a :ref:`Cloud Optimized GeoTIFF <raster.cog>` and emits a report.

.. note:: This program requires the GDAL Python bindings to be available.

Program-Specific Options
------------------------

.. option:: --full-check auto|yes|no

    Whether extensive checks, verifying leading and trailer bytes of strips or
    tiles, must be performed. In ``auto`` mode (the default),
    they are performed for local file, but not for remote files

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/quiet.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example:: Check if a remote file is a valid COG file.

   .. code-block:: bash

       gdal driver cog validate /vsicurl/https://example.com/some.tif
