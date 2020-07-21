.. _gdal_viewshed:

================================================================================
gdal_viewshed
================================================================================

.. only:: html

    .. versionadded:: 3.1.0

    Calculates a viewshed raster from an input raster DEM using method defined in [Wang2000]_ for a user defined point.

.. Index:: gdal_viewshed

Synopsis
--------

.. code-block::

   gdal_viewshed [-b <band>]
                 [-a_nodata <value>] [-f <formatname>]
                 [-oz <observer_height>] [-tz <target_height>] [-md <max_distance>]
                 -ox <observer_x> -oy <observer_y>
                 [-vv <visibility>] [-iv <invisibility>]
                 [-ov <out_of_range>] [-cc <curvature_coef>]
                 [[-co NAME=VALUE] ...]
                 [-q] [-om <output mode>]
                 <src_filename> <dst_filename>

Description
-----------

By default the :program:`gdal_viewshed` generates a binary visibility raster from one band
of the input raster elevation model (DEM). The output raster will be of type
Byte. With the -mode flag can also return a minimum visible height raster of type Float64.

.. note::
    The algorithm as implemented currently will only output meaningful results
    if the georeferencing is in a projected coordinate reference system.

.. program:: gdal_viewshed

.. include:: options/co.rst

.. option:: -b <band>

   Select an input band **band** containing the DEM data. Bands are numbered from 1.
   Only a single band can be used. Only the part of the raster within the specified 
   maximum distance around the observer point is processed.

.. option:: -a_nodata <value>

   The value to be set for the cells in the output raster that have no data.

   .. note::
        Currently, no special processing of input cells at a nodata
        value is done (which may result in erroneous results).

.. option:: -ox <value>

   The X position of the observer (in SRS units).

.. option:: -oy <value>

   The Y position of the observer (in SRS units).

.. option:: -oz <value>

   The height of the observer above the DEM surface in the height unit of the DEM. Default: 2

.. option:: -tz <value>

   The height of the target above the DEM surface in the height unit of the DEM. Default: 0

.. option:: -md <value>

   Maximum distance from observer to compute visibiliy.
   It is also used to clamp the extent of the output raster.

.. option:: -cc <value>

   Coefficient to consider the effect of the curvature and refraction.
   The height of the DEM is corrected according to the following formula:

   .. math::

      Height_{Corrected}=Height_{DEM}-{CurvCoeff}\frac{{TargetDistance}^2}{SphereDiameter}

   For atmospheric refraction we can use 0.85714

.. option:: -iv <value>

   Pixel value to set for invisible areas. Default: 0

.. option:: -ov <value>

   Pixel value to set for the cells that fall outside of the range specified by 
   the observer location and the maximum distance. Default: 0

.. option:: -vv <value>

   Pixel value to set for visible areas. Default: 255

.. option:: -om <output mode>

  Sets what information the output contains.

  Possible values: VISIBLE, DEM, GROUND
 
  VISIBLE returns a raster of type Byte containing visible locations.
 
  DEM and GROUND will return a raster of type Float64 containing the minimum target
  height for target to be visible from the DEM surface or ground level respectively.
  Flags -tz, -iv and -vv will be ignored.

  Default VISIBLE

C API
-----

Functionality of this utility can be done from C with :cpp:func:`GDALViewshedGenerate`.

Example
-------

Compute the visibility of an elevation raster data source with defaults


.. figure:: ../../images/gdal_viewshed.png

   A computed visibility for two separate `-ox` and `-oy` points on a DEM.

.. code-block::

    gdal_viewshed -md 500 -ox -10147017 -oy 5108065 source.tif destination.tif




.. [Wang2000] Generating Viewsheds without Using Sightlines. Wang, Jianjun,
   Robinson, Gary J., and White, Kevin. Photogrammetric Engineering and Remote
   Sensing. p81. https://www.asprs.org/wp-content/uploads/pers/2000journal/january/2000_jan_87-90.pdf
