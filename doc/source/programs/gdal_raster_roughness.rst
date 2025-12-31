.. _gdal_raster_roughness:

================================================================================
``gdal raster roughness``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Generate a roughness map

.. Index:: gdal raster roughness

Synopsis
--------

.. program-output:: gdal raster roughness --help-doc

Description
-----------

:program:`gdal raster roughness` generates a single-band raster with values
computed from the elevation. Roughness is the largest inter-cell difference
of a central pixel and its surrounding cell, as defined in Wilson et al. (2007, Marine Geodesy 30:3-35).

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

The roughness value -9999 is used as the nodata value.

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

    Index (starting at 1) of the band to which the roughness must be computed.

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
   :title: Generates a roughness map from a DTED0 file.

   .. code-block:: bash

        $ gdal raster roughness n43.dt0 out.tif --overwrite
