.. _gdal_raster_slope:

================================================================================
``gdal raster slope``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Generate a slope map

.. Index:: gdal raster slope

Synopsis
--------

.. program-output:: gdal raster slope --help-doc

Description
-----------

:program:`gdal raster slope` generates a slope map, from any
GDAL-supported elevation raster.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

It generates a 32-bit float raster with slope values. You have the option of
specifying the type of slope value you want: degrees or percent slope. In cases
where the horizontal units differ from the vertical units, you can also supply
scaling factors to account for differences between vertical and horizontal units.

The value `-9999` is used as the output nodata value. A nodata value in the target dataset
will be emitted if at least one pixel set to the nodata value is found in the
3x3 window centered around each source pixel. By default, the algorithm will
compute values at image edges or if a nodata value is found in the 3x3 window,
by interpolating missing values, unless :option:`--no-edges` is specified, in
which case a 1-pixel border around the image will be set with the nodata value.

In general, it assumes that x, y and z units are identical. However, if none of
:option:`--xscale` and :option:`--yscale` are specified, and the CRS is a
geographic or projected CRS, it will automatically determine the
appropriate ratio from the units of the CRS, as well as the potential value of
the units of the raster band (as returned by :cpp:func:`GDALRasterBand::GetUnitType`, if it
is metre, foot international or US survey foot). Note that for geographic CRS,
the result for source datasets at high latitudes may be incorrect, and prior
reprojection to a polar projection might be needed using :ref:`gdal_raster_reproject`.

If x (east-west) and y (north-south) units are identical, but z (elevation) units
are different, the :option:`--xscale` and :option:`--yscale` can be used to set
the ratio of vertical units to horizontal.
For geographic CRS near the equator, where units of latitude and units of
longitude are similar, elevation (z) units can be converted to be compatible
by using scale=370400 (if elevation is in feet) or scale=111120 (if elevation is in
meters).  For locations not near the equator, the :option:`--xscale` value can be taken as
the :option:`--yscale` value multiplied by the cosine of the mean latitude of the raster.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst


Program-Specific Options
------------------------

.. option:: -b, --band <BAND>

    Index (starting at 1) of the band to which the slope must be computed.

.. option:: --gradient-alg Horn|ZevenbergenThorne

    Algorithm used to compute terrain gradient. The default is ``Horn``.
    The literature suggests Zevenbergen & Thorne to be more suited to smooth
    landscapes, whereas Horn's formula to perform better on rougher terrain.

.. option:: --no-edges

    Do not try to interpolate values at dataset edges or close to nodata values


.. option:: --unit degree|percent

    Unit in which to express slopes. Defaults to ``degree``.

.. option:: --xscale <scale>

    Ratio of vertical units to horizontal X axis units. If the horizontal unit of the source DEM is degrees (e.g Lat/Long WGS84 projection), you can use scale=111120 if the vertical units are meters (or scale=370400 if they are in feet).

    If none of :option:`--xscale` and :option:`--yscale` are specified, and the
    CRS is a geographic or projected CRS,
    :program:`gdal raster slope` will automatically determine the appropriate ratio from
    the units of the CRS, as well as the potential value of the units of the
    raster band (as returned by :cpp:func:`GDALRasterBand::GetUnitType`, if it
    is metre, foot international or US survey foot). Note that for geographic CRS,
    the result for source datasets at high latitudes may be incorrect, and prior
    reprojection to a polar projection might be needed.

    If :option:`--xscale` is specified, :option:`--yscale` must also be specified.

.. option:: --yscale <scale>

    Ratio of vertical units to horizontal Y axis units. If the horizontal unit of the source DEM is degrees (e.g Lat/Long WGS84 projection), you can use scale=111120 if the vertical units are meters (or scale=370400 if they are in feet)

    If none of :option:`--xscale` and :option:`--yscale` are specified, and the
    CRS is a geographic or projected CRS,
    :program:`gdal raster slope` will automatically determine the appropriate ratio from
    the units of the CRS, as well as the potential value of the units of the
    raster band (as returned by :cpp:func:`GDALRasterBand::GetUnitType`, if it
    is metre, foot international or US survey foot). Note that for geographic CRS,
    the result for source datasets at high latitudes may be incorrect, and prior
    reprojection to a polar projection might be needed.

    If :option:`--yscale` is specified, :option:`--xscale` must also be specified.


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
   :title: Generates a slope map from a DTED0 file.

   .. code-block:: bash

        $ gdal raster slope n43.dt0 out.tif --overwrite
