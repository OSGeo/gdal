.. _gdal_raster_viewshed:

================================================================================
``gdal raster viewshed``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Compute the viewshed of a raster dataset.

.. Index:: gdal raster viewshed

Synopsis
--------

.. program-output:: gdal raster viewshed --help-doc

Description
-----------

:program:`gdal raster viewshed` creates a binary visibility raster from one band
of the input raster elevation model (DEM). The output raster will be of type
Byte. Using the ``DEM`` or ``ground`` values of :option:`--mode` can also
create a minimum visible height raster of type Float64.

It uses the method defined in [Wang2000]_ for a user defined point.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`
(since GDAL 3.12)

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. versionadded:: 3.12

.. include:: gdal_cli_include/gdalg_raster_compatible_non_natively_streamable.rst


Program-Specific Options
------------------------

.. option:: -b, --band <band>

   Select an input band containing the DEM data. Bands are numbered from 1.
   Only a single band can be used. Only the part of the raster within the specified
   maximum distance around the observer point is processed.

.. option:: --curvature-coefficient <value>

   Coefficient to consider the effect of the curvature and refraction.
   When calculating visibility between two points (i.e. Line Of Sight or Viewshed),
   The magnitude of this effect varies with atmospheric conditions and depends on the wavelength.

   Different applications for calculating visibility use different interchangeable notation to describe this phenomena:
   Refraction Coefficient, Curvature Coefficient, and Sphere Diameter Factor.
   gdal_viewshed uses the Curvature Coefficient notation.

   .. math::

     {CurvCoeff}=1-{RefractionCoeff}

   Changes in air density curve the light downward causing an observer to see further and the earth to appear less curved,
   as if the sphere (earth) diameter is larger then it actually is.
   The ratio between that imaginary sphere diameter and the actual sphere diameter is given by the formula:

   .. math::
     {SphereDiameterFactor}=1/{CurvCoeff}=1/(1-{RefractionCoeff})

   For visible light, the standard atmospheric refraction coefficient that is generally used is 1/7.
   Thus the default value (since GDAL 3.4) for CurvCoeff that gdal_viewshed uses is 0.85714 (=~ 1-1/7)
   for Earth CRS. Starting with GDAL 3.6, for non-Earth CRS (those whole semi-major axis differs
   by more than 5% with the one of WGS 84), CurvCoeff default value is 1.0, to account for
   the no refraction use case.

   The height of the DEM is corrected according to the following formula:

   .. math::

      Height_{Corrected}=Height_{DEM}-{CurvCoeff}\frac{{TargetDistance}^2}{SphereDiameter}

   Typical coefficient values are given in the table below (use Curvature Coeff value for the cc option)

   ================  ==================  ===================  =====================
   Use Case          Refraction Coeff    **Curvature Coeff**  Sphere Diameter Factor
   No Refraction     0                   1                    1
   Visible Light     1/7                 6/7 (=~0.85714)      7/6 (=~1.1666)
   Radio Waves       0.25 ~ 0.325        0.75 ~ 0.675         1.33 ~ 1.48
   Flat Earth        1                   0                    inf
   ================  ==================  ===================  =====================


.. option:: --dst-nodata <value>

   The value to be set for the cells in the output raster that have no data.

   .. note::
        Currently, no special processing of input cells at a nodata
        value is done (which may result in erroneous results).

.. option:: --end-angle <value>

   .. versionadded:: 3.12

   End angle for visibility. Measured clockwise from 0 North, in degree.
   (Not supported in cumulative mode)

.. option:: -z, --height <HEIGHT>

   Observer height

.. option:: --high-pitch <value>

   .. versionadded:: 3.12

   High angle for visibility. Measured up from 0 horizontal, in degree.
   Input values above the high pitch are marked out of range.
   Must be greater than '--low-pitch'.
   (Not supported in cumulative mode)

.. option:: --invisible-value <value>

   Pixel value to set for invisible areas. (Not supported in cumulative mode) Default: 0

.. option:: --low-pitch <value>

   .. versionadded:: 3.12

   Low angle for visibility. Measured up from 0 horizontal, in degree.
   Input cell values below the pitch are are clamped to be no lower
   than the intersection of the angle.  Must be less than '--high-pitch'.
   (Not supported in cumulative mode)

.. option:: --max-distance <value>

   Maximum distance from observer to compute visibility.
   It is also used to clamp the extent of the output raster.
   (Not supported in cumulative mode)

.. option:: --min-distance <value>

   .. versionadded:: 3.12

   Minimum distance from observer to compute visibility.
   Must be less than '--max-distance'
   (Not supported in cumulative mode)

.. option:: --mode normal|DEM|ground|cumulative

   Sets what information the output contains.

   - ``normal`` (the default) returns a raster of type Byte containing visible locations.

   - ``DEM`` and ``ground`` return a raster of type Float64 containing the minimum target
     height for the target to be visible from the DEM surface or ground level respectively.
     That is to say, if the minimum target height for the target to be visible at a
     point is ``h`` and the value of the input raster at that point is ``E``,
     for ``DEM``, ``E + h`` will be the output value.
     For ``ground``, ``h`` will be output value.
     Options ``--target-height``, ``--invisible-value`` and ``--visible-value`` will
     be ignored.

   - ``cumulative`` creates an eight bit raster the same size as the input raster
     where each cell represents the relative observability from a grid of observer points.
     See the :option:`--observer-spacing` option.

.. option:: -j, --num-threads <value>

   Number of jobs to run at once. (only supported in cumulative mode).
   Default: 3

.. option:: --observer-spacing <value>

   Cell spacing between observers (only supported in cumulative mode).
   Default: 10

.. option:: --out-of-range-value <value>

   Pixel value to set for the cells that fall outside of the range specified by
   the observer location and the maximum distance. (Not supported in cumulative mode) Default: 0

.. option:: -p, --pos, --position <X,Y> or <X,Y,H>

   The X,Y or X,Y,H(Height) position of the observer (in SRS units for X and Y,
   and in the height unit of the DEM for H).
   If the coordinate is outside of the raster, all space between the observer
   and the raster is assumed not to occlude visibility of the raster. (Not supported in cumulative mode.)
   If H is not specified, it defaults to 2.

.. option:: --start-angle <value>

   .. versionadded:: 3.12

   Start angle for visibility. Measured clockwise from 0 North, in degree.
   (Not supported in cumulative mode)

.. option:: --target-height <value>

   The height of the target above the DEM surface in the height unit of the DEM. Default: 0

.. option:: --visible-value <value>

   Pixel value to set for visible areas. (Not supported in cumulative mode) Default: 255

  .. versionadded:: 3.13

.. option:: --sd-filename <value>

   Filename of raster containing standard deviations of the input raster values. The raster
   always comes from band 1 and the size must match that of the input raster.

.. option:: --maybe-visible-value <value>

   Pixel value to set for visible areas. (Not supported in cumulative mode) Default: 255


Standard Options
----------------

.. collapse:: details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::

   Screenshot of 2 combined viewshed analysis, with the yellow pixels showing the area that is
   visible from the both observation locations (the green dots), while the small green area is
   only visible from one location.


   .. figure:: ../../images/gdal_viewshed.png


   Create a viewshed raster with a radius of 500 for a person standing at location (-10147017, 5108065).

   .. code-block:: bash

       gdal raster viewshed --max-distance=500 --pos=-10147017,5108065 source.tif destination.tif
