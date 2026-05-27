.. _gdal_mdim_reproject:

.. program:: gdal_mdim_reproject

================================================================================
``gdal mdim reproject``
================================================================================

.. versionadded:: 3.14

.. only:: html

    Reproject a multidimensional dataset.

.. Index:: gdal mdim reproject

Synopsis
--------

.. program-output:: gdal mdim reproject --help-doc

Description
-----------

:program:`gdal mdim reproject` can be used to reproject a multidimensional dataset.
It reprojects all variables with 2 or more dimensions, and keep others unmodified.

Reprojection is done on-demand, using :cpp:func:`GDALMDArray::GetResampled`.

:program:`gdal mdim reproject` can be used as a step of a :ref:`gdal_mdim_pipeline`.

Program-Specific Options
------------------------

.. option:: --output-crs, -d, <OUTPUT-CRS>

    Set output spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

.. option:: -r, --resampling <RESAMPLING>

    Resampling method to use. Available methods are:

    ``nearest``: nearest neighbour resampling (default, fastest algorithm, worst interpolation quality).

    ``bilinear``: bilinear resampling.

    ``cubic``: cubic resampling.

    ``cubicspline``: cubic spline resampling.

    ``lanczos``: Lanczos windowed sinc resampling.

    ``average``: average resampling, computes the weighted average of all non-NODATA contributing pixels.

    ``rms`` root mean square / quadratic mean of all non-NODATA contributing pixels

    ``mode``: mode resampling, selects the value which appears most often of all the sampled points. In the case of ties, the first value identified as the mode will be selected.

    ``max``: maximum resampling, selects the maximum value from all non-NODATA contributing pixels.

    ``min``: minimum resampling, selects the minimum value from all non-NODATA contributing pixels.

    ``med``: median resampling, selects the median value of all non-NODATA contributing pixels.

    ``q1``: first quartile resampling, selects the first quartile value of all non-NODATA contributing pixels.

    ``q3``: third quartile resampling, selects the third quartile value of all non-NODATA contributing pixels.

    ``sum``: compute the weighted sum of all non-NODATA contributing pixels


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_mdim_create_copy.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/quiet.rst

Examples
--------

.. example::
   :title: Reproject a netCDF file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N") and write it as Zarr

   .. code-block:: bash

        $ gdal mdim reproject -r cubic --output-crs=EPSG:32632 in.nc out.zarr --overwrite
