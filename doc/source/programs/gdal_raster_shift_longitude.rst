.. _gdal_raster_shift_longitude:

.. program:: gdal_raster_shift_longitude

================================================================================
``gdal raster shift-longitude``
================================================================================

.. versionadded:: 3.14

.. only:: html

    Shift the longitude values of a raster dataset

.. Index:: gdal raster shift-longitude

Synopsis
--------

.. program-output:: gdal raster shift-longitude --help-doc

Description
-----------

This program shifts the longitude values of raster datasets in geographic coordinates, for example between (-180, 180) and (0, 360) representations of longitude.
No resampling is performed.
The result can be written in the :ref:`VRT (Virtual Dataset) <raster.vrt>` format.

All bands of each input file are added as separate output bands. If a subset of bands is desired, this command can be used in a raster pipeline with :ref:`gdal_raster_select`.

The longitude range of the output dataset may be greater than 360 degrees, if desired.
This may be useful when combining datasets whose pixel boundaries of extents are not coincident, for example when performing raster zonal statistics on polygons with an longitude range of (-180, 180) and a 1-degree raster dataset with a longitude range of (-0.5, 179.5).

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst


Program-Specific Options
------------------------

.. option:: --max-x

   Specifies the maximum longitude value that must be included in the output. If the provided
   value does not fall on a pixel boundary, the maximum longitude of the output dataset may be
   greater than this value.

.. option:: --min-x

   Specifies the minimum longitude value that must be included in the output. If the provided
   value does not fall on a pixel boundary, the minimum longitude of the output dataset may be
   less than this value.

.. option:: --output-nodata <value>

    Set the NoData value for the output dataset. The value set by this option
    is written in the ``NoDataValue`` element of each ``VRTRasterBand element``.
    If not specified, the NoData value of the input dataset (if any) will be used.


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Shift pixels in an ERA5 netCDF to cover the longitude range (-180, 180)

   The ERA5 reanalysis dataset covers the globe at a resolution of 0.25 degrees, with
   cell centers falling on integer degrees of longitude. Extracts of the dataset in
   netCDF format cover longitude range (-0.125, 359.875). To fully cover the longitude
   range (-180, 180), :program:`gdal raster shift-longitude` produces an output covering
   the longitude range (-180.125, 180.125), with the first column repeated in the last column.

   .. code-block:: bash

       gdal raster shift-longitude --min-x -180 --max-x 180 era5_t2m.nc era5_t2m.vrt
