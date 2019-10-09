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

   gdal_viewshed [-b <band>] [-a <attribute_name>] [-3d] [-inodata]
                 [-snodata n] [-f <formatname>] [-tr <target_raster_filename>]
                 [-oz <observer_height>] [-tz <target_height>] [-md <max_distance>]
                 [-ox <observer_x> -oy <observer_y>]
                 [[-dsco NAME=VALUE] ...] [[-lco NAME=VALUE] ...]
                 [-nln <outlayername>] [-q]
                 <src_filename> <dst_filename>

Description
-----------

The :program:`gdal_viewshed` generates a 0-1 visibility raster band from the input
raster elevation model (DEM).

.. versionadded:: 3.1.0

   Compute visibility raster band for an input raster.

TODO document options.

C API
-----

Functionality of this utility can be done from C with :cpp:func:`GDALViewshedGenerate`.

Example
-------


.. code-block::

    gdal_viewshed -md 500 -x "-10147017".0 -y "5108065" source.tif destination.tif
