.. _gdal_raster_tri:

================================================================================
``gdal raster tri``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Generate a Terrain Ruggedness Index (TRI) map

.. Index:: gdal raster tri

Synopsis
--------

.. program-output:: gdal raster tri --help-doc

Description
-----------

:program:`gdal raster tri` generates a single-band raster with values computed
from the elevation. TRI stands for Terrain Ruggedness Index, which measures the
difference between a central pixel and its surrounding cells.

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

.. option:: --algorithm Riley|Wilson

    Select the algorithm to use:

    - ``Riley``, the default, uses the square root of the sum of the square of
      the difference between a central pixel and its surrounding cells.
      This is recommended for terrestrial use cases.
      It is implemented from Riley, S.J., De Gloria, S.D., Elliot, R. (1999): A Terrain Ruggedness that Quantifies Topographic Heterogeneity. Intermountain Journal of Science, Vol.5, No.1-4, pp.23-27

    - ``Wilson`` uses the mean difference between a central pixel and its
      surrounding cells. This is recommended for bathymetric use cases.
      It is implemented from Wilson et al. 2007, Marine Geodesy 30:3-35.

.. option:: -b, --band <BAND>

    Index (starting at 1) of the band to which the TRI must be computed.

.. option:: --no-edges

    Do not try to interpolate values at dataset edges or close to nodata values

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
   :title: Generates a Terrain Ruggedness Index (TRI) map from a DTED0 file.

   .. code-block:: bash

        $ gdal raster tri n43.dt0 out.tif --overwrite



.. below is an allow-list for spelling checker.

.. spelling:word-list::
    tri
