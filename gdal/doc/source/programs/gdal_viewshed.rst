.. _gdal_viewshed:

================================================================================
gdal_viewshed
================================================================================

.. only:: html

    Calculates viewshed using method defined in [Wang2000]_ for a user defined
    point.

.. Index:: gdal_viewshed

Synopsis
--------

.. code-block::

   gdal_viewshed [-b <band>] [-inodata]
                 [-snodata n] [-f <formatname>]
                 [-oz <observer_height>] [-tz <target_height>] [-md <max_distance>]
                 [-ox <observer_x>] [-oy <observer_y>]
                 [-vv <visibility>] [-iv <invisibility>]
                 [-ov <out_of_range>] [-cc <curvature_coef>]
                 [[-co NAME=VALUE] ...]
                 [-q]
                 <src_filename> <dst_filename>

Description
-----------

The :program:`gdal_viewshed` generates a 0-1 visibility raster band from the input
raster elevation model (DEM).



.. program:: gdal_viewshed

.. versionadded:: 3.1.0

.. include:: options/co.rst

.. option:: -b <band>

   Select an input band **band** for output. Bands are numbered from 1.
   Only a single band can be used.

.. option:: -a_nodata <value>

   Assign a specified nodata value to output band.

.. option:: -ox <value>

   Observer X (in SRS units).

.. option:: -oy <value>

   Observer Y (in SRS units).

.. option:: -oz <value>

   Observer height.

.. option:: -tz <value>

   Target height.

.. option:: -md <value>

   Maximum distance from observer to compute visibiliy.

.. option:: -cc <value>

   Curvature coefficient as described in [Wang2000]_. Default: 0

.. option:: -iv <value>

   Pixel value to set for invisibility. Default: -1

.. option:: -ov <value>

   Pixel value to set for out-of-range. Default: 0

.. option:: -vv <value>

   Pixel value to set for visibilty. Default: 255

C API
-----

Functionality of this utility can be done from C with :cpp:func:`GDALViewshedGenerate`.

Example
-------

Compute the visibility of an elevation raster data source with defaults


.. figure:: ../../images/gdal_viewshed.png

   A computed visibility for two separate `-ox` and `-oy` points on a DEM.

.. code-block::

    gdal_viewshed -md 500 -x "-10147017".0 -y "5108065" source.tif destination.tif




.. [Wang2000] Generating Viewsheds without Using Sightlines. Wang, Jianjun,
   Robinson, Gary J., and White, Kevin. Photogrammetric Engineering and Remote
   Sensing. p81. https://www.asprs.org/wp-content/uploads/pers/2000journal/january/2000_jan_87-90.pdf
