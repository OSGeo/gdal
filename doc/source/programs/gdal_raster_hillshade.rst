.. _gdal_raster_hillshade:

================================================================================
``gdal raster hillshade``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Generate a shaded relief map

.. Index:: gdal raster hillshade

Synopsis
--------

.. program-output:: gdal raster hillshade --help-doc

Description
-----------

:program:`gdal raster hillshade` generates a shaded relief map, from any
GDAL-supported elevation raster.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

It generates an 8-bit raster with a nice shaded relief effect. It is very useful
for visualizing the terrain. You can optionally specify the azimuth and altitude
of the light source, a vertical exaggeration factor and scaling factors to
account for differences between vertical and horizontal units.

The value 0 is used as the output nodata value. A nodata value in the target dataset
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

.. option:: --altitude <ALTITUDE>

    Altitude of the light, in degrees. 90 if the light comes from above the
    DEM, 0 if it is raking light. The default value is 45 degree.

    This option is mutually exclusive with ``--variant=Igor``.

.. option:: --azimuth <AZIMUTH>

    Azimuth of the light, in degrees. 0 if it comes from the top of the raster,
    90 from the east, ... The default value, 315, should rarely be changed as it
    is the value generally used to generate shaded maps.

    This option is mutually exclusive with ``--variant=multidirectional``.

.. option:: -b, --band <BAND>

    Index (starting at 1) of the band to which the hillshade must be computed.

.. option:: --gradient-alg Horn|ZevenbergenThorne

    Algorithm used to compute terrain gradient. The default is ``Horn``.
    The literature suggests Zevenbergen & Thorne to be more suited to smooth
    landscapes, whereas Horn's formula to perform better on rougher terrain.

.. option:: --no-edges

    Do not try to interpolate values at dataset edges or close to nodata values

.. option:: --variant regular|combined|multidirectional|Igor

    Variant of the hillshading algorithm:

    - ``regular``: the hillshade values combines the computed slope with the
      azimuth and altitude of the illumination according to:

        .. math::
            {Hillshade} = 1 + 254.0 * ((\sin(altitude) * cos(slope)) + (cos(altitude) * sin(slope) * cos(azimuth - \frac{\pi}{2} - aspect)))

    - ``combined``: combined shading, a combination of slope and oblique shading.
    - ``multidirectional``: multidirectional shading, a combination of hillshading
      illuminated from 225 deg, 270 deg, 315 deg, and 360 deg azimuth.
      Applies the formula of http://pubs.usgs.gov/of/1992/of92-422/of92-422.pdf
    - ``Igor``: shading which tries to minimize effects on other map features
      beneath. Igor's hillshading uses formula from Maperitive:
      http://maperitive.net/docs/Commands/GenerateReliefImageIgor.html


.. option:: --xscale <scale>

    .. versionadded:: 3.11

    Ratio of vertical units to horizontal X axis units. If the horizontal unit of the source DEM is degrees (e.g Lat/Long WGS84 projection), you can use scale=111120 if the vertical units are meters (or scale=370400 if they are in feet).

    If none of :option:`--xscale` and :option:`--yscale` are specified, and the
    CRS is a geographic or projected CRS,
    :program:`gdal raster hillshade` will automatically determine the appropriate ratio from
    the units of the CRS, as well as the potential value of the units of the
    raster band (as returned by :cpp:func:`GDALRasterBand::GetUnitType`, if it
    is metre, foot international or US survey foot). Note that for geographic CRS,
    the result for source datasets at high latitudes may be incorrect, and prior
    reprojection to a polar projection might be needed.

    If :option:`--xscale` is specified, :option:`--yscale` must also be specified.

.. option:: --yscale <scale>

    .. versionadded:: 3.11

    Ratio of vertical units to horizontal Y axis units. If the horizontal unit of the source DEM is degrees (e.g Lat/Long WGS84 projection), you can use scale=111120 if the vertical units are meters (or scale=370400 if they are in feet)

    If none of :option:`--xscale` and :option:`--yscale` are specified, and the
    CRS is a geographic or projected CRS,
    :program:`gdal raster hillshade` will automatically determine the appropriate ratio from
    the units of the CRS, as well as the potential value of the units of the
    raster band (as returned by :cpp:func:`GDALRasterBand::GetUnitType`, if it
    is metre, foot international or US survey foot). Note that for geographic CRS,
    the result for source datasets at high latitudes may be incorrect, and prior
    reprojection to a polar projection might be needed.

    If :option:`--yscale` is specified, :option:`--xscale` must also be specified.


.. option:: -z, --zfactor <ZFACTOR>

    Vertical exaggeration used to pre-multiply the elevations

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
   :title: Generates a shaded relief map from a DTED0 file, using a vertical exaggeration factor of 30.

   .. code-block:: bash

        $ gdal raster hillshade --zfactor=30 n43.dt0 out.tif --overwrite

.. example::
   :title: Combine the output of shaded relief map and hypsometric rendering on a DEM to create a colorized shaded relief map.

   .. code-block:: bash

        $ gdal pipeline read n43.tif ! \
                        color-map --color-map color_file.txt ! \
                        color-merge --grayscale \
                            [ read n43.tif ! hillshade -z 30 ] ! \
                        write out.tif --overwrite
