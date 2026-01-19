.. _gdal_raster_tpi:

================================================================================
``gdal raster tpi``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Generate a Topographic Position Index (TPI) map

.. Index:: gdal raster tpi

Synopsis
--------

.. program-output:: gdal raster tpi --help-doc

Description
-----------

:program:`gdal raster tpi` generates a single-band raster with values computed
from the elevation. TPI stands for Topographic Position Index, which is defined
as the difference between a central pixel and the mean of its surrounding cells
(see Wilson et al 2007, Marine Geodesy 30:3-35).

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

Value -9999 is used as the nodata value.

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

    Index (starting at 1) of the band to which the TPI must be computed.

.. option:: --no-edges

    Do not try to interpolate values at dataset edges or close to nodata values


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
   :title: Generates a Topographic Position Index (TPI) map from a DTED0 file.

   .. code-block:: bash

        $ gdal raster tpi n43.dt0 out.tif --overwrite



.. below is an allow-list for spelling checker.

.. spelling:word-list::
    tpi
