.. _gdal_raster_hillshade_subcommand:

================================================================================
"gdal raster hillshade" sub-command
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

:program:`gdal raster hillshade` generate a shaded relief map, from any
GDAL-supported elevation raster.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline_subcommand`

It outputs an 8-bit raster with a nice shaded relief effect. It is very useful
for visualizing the terrain. You can optionally specify the azimuth and altitude
of the light source, a vertical exaggeration factor and a scaling factor to
account for differences between vertical and horizontal units.

The value 0 is used as the output nodata value.

In general, it assumes that x, y and z units are identical. However, if none of
:option:`--xscale` and :option:`--yscale` are specified, and the CRS is a
geographic or projected CRS, it will automatically determine the
appropriate ratio from the units of the CRS, as well as the potential value of
the units of the raster band (as returned by :cpp:func:`GDALRasterBand::GetUnitsType`, if it
is metre, foot international or US survey foot). Note that for geographic CRS,
the result for source datasets at high latitudes may be incorrect, and prior
reprojection to a polar projection might be needed using :ref:`gdal_raster_reproject_subcommand`.

If x (east-west) and y (north-south) units are identical, but z (elevation) units
are different, the :option:`-xscale` and :option:`-yscale` can be used to set
the ratio of vertical units to horizontal.
For geographic CRS near the equator, where units of latitude and units of
longitude are similar, elevation (z) units can be converted to be compatible
by using scale=370400 (if elevation is in feet) or scale=111120 (if elevation is in
meters).  For locations not near the equator, the xscale value can be taken as
the yscale value multiplied by the cosinus of the mean latitude of the raster.

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Generates a shaded relief map frmo a DTED0 file, using a vertical exaggeration factor of 30.

   .. code-block:: bash

        $ gdal raster hillshade --zfactor=30 n43.dt0 out.tif --overwrite
