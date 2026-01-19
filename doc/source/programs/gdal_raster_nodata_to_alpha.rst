.. _gdal_raster_nodata_to_alpha:

================================================================================
``gdal raster nodata-to-alpha``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Replace nodata value(s) with an alpha band.

.. Index:: gdal raster nodata-to-alpha

Synopsis
--------

.. program-output:: gdal raster nodata-to-alpha --help-doc

Description
-----------

:program:`gdal raster nodata-to-alpha` adds an alpha band based on the nodata
value metadata set on the bands of the input dataset.

If there is no explicit nodata value on the input dataset, it is possible to
add it with the :option:`--nodata` option.

This program is a no-op and can be safely used if the input dataset has no
nodata value and :option:`--nodata` is not specified.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst


Program-Specific Options
------------------------

.. option::  --nodata <NODATA>

   Override nodata value of input band(s) (numeric value, 'nan', 'inf', '-inf').
   Either one value can be specified or as many values as they are input bands.
   When multiple values are specified, a pixel is considered as invalid only
   if it matches the nodata values on all bands.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/ot.rst

Examples
--------

.. example::

   .. code-block:: console

       $ gdal raster nodata-to-alpha input_with_nodata.tif output_with_alpha_band.tif
