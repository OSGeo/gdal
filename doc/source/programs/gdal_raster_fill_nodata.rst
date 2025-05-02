.. _gdal_raster_fill_nodata:

================================================================================
``gdal raster fill-nodata``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Fill nodata raster regions by interpolation from edges

.. Index:: gdal raster fill-nodata

Synopsis
--------

.. program-output:: gdal raster fill-nodata --help-doc

Description
-----------

:program:`gdal raster fill-nodata` fills nodata areas by interpolating
from valid pixels around the edges of the area.

The following options are available:

.. include:: gdal_options/of_raster_create.rst

.. option:: --output-layer <OUTPUT-LAYER>

    Output layer name.

.. include:: gdal_options/overwrite.rst

.. option:: -b <BAND>

    Select an input <BAND> to be processed. Bands are numbered from 1.
    Default is the first band of the input dataset.

.. option:: -max-distance <MAX_DISTANCE>

    Specifies the maximum distance (in pixels) that the algorithm will search
    out for values to interpolate. Default is 100 pixels.

.. option:: --smoothing-iterations <SMOOTHING_ITERATIONS>

    Specifies the number of smoothing iterations to apply to the filled raster.
    This can help to reduce artifacts in the filled areas.
    Default is 0 iterations.

.. option:: --strategy <STRATEGY>

    Select the interpolation <STRATEGY> to use.
    By default, pixels are interpolated using an inverse distance
    weighting (`invdist`). It is also possible to choose a nearest
    neighbour (`nearest`) strategy.

.. option:: --mask <MASK>

    Use the first band of the specified file as a
    validity mask (zero is invalid, non-zero is valid).

Examples
--------

.. example::
   :title: Fill nodata areas in a raster

   The command specifies to use the second band of the input raster, 50 px max distance,
   3 smoothing iterations and the `nearest` strategy for interpolation.
   The output will be saved in `output.tif`.

   .. code-block:: bash

    gdal raster fill-nodata -b 2 --max-distance 50 --smoothing-iterations 3 \
        --strategy nearest --mask mask.tif \
        input.tif output.tif
