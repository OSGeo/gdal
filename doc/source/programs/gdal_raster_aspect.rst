.. _gdal_raster_aspect:

================================================================================
``gdal raster aspect``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Generate an aspect map

.. Index:: gdal raster aspect

Synopsis
--------

.. program-output:: gdal raster aspect --help-doc

Description
-----------

:program:`gdal raster aspect` generates an aspect map, from any
GDAL-supported elevation raster.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

It outputs a 32-bit float raster with values between 0° and 360° representing
the azimuth that slopes are facing. The definition of the azimuth is such that:
- 0° means that the slope is facing the North,
- 90° it's facing the East,
- 180° it's facing the South
- and 270° it's facing the West (provided that the top of your input raster is north oriented).

The aspect value -9999 is used as the nodata value to indicate undefined aspect in flat areas with slope=0.

A nodata value in the target dataset will also be emitted if at least one pixel set to the nodata value is found in the
3x3 window centered around each source pixel. By default, the algorithm will
compute values at image edges or if a nodata value is found in the 3x3 window,
by interpolating missing values, unless :option:`--no-edges` is specified, in
which case a 1-pixel border around the image will be set with the nodata value.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst


Program-Specific Options
------------------------

.. option:: -b, --band <BAND>

    Index (starting at 1) of the band to which the aspect must be computed.

.. option:: --convention azimuth|trigonometric-angle

    Convention for output angles.

    Defaults to ``azimuth``, that is to say:
    - 0° means that the slope is facing the North,
    - 90° it's facing the East,
    - 180° it's facing the South
    - and 270° it's facing the West (provided that the top of your input raster is north oriented).

    If set to ``trigonometric-angle``,
    - 0° means that the slope is facing the East,
    - 90° it's facing the North,
    - 180° it's facing the West
    - and 270° it's facing the South

.. option:: --gradient-alg Horn|ZevenbergenThorne

    Algorithm used to compute terrain gradient. The default is ``Horn``.
    The literature suggests Zevenbergen & Thorne to be more suited to smooth
    landscapes, whereas Horn's formula to perform better on rougher terrain.

.. option:: --no-edges

    Do not try to interpolate values at dataset edges or close to nodata values

.. option:: --zero-for-flat

   Whether to output zero for flat areas. By default, flat areas where the slope
   is null will be assigned a nodata value (-9999). When setting this option,
   they are set to 0.

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
   :title: Generates an aspect map from a DTED0 file.

   .. code-block:: bash

        $ gdal raster aspect n43.dt0 out.tif --overwrite
