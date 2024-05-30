.. _gdal_fillnodata:

================================================================================
gdal_fillnodata
================================================================================

.. only:: html

    Fill raster regions by interpolation from edges.

.. Index:: gdal_fillnodata

Synopsis
--------

.. code-block::

    gdal_fillnodata [--help] [--help-general] [-q] [-md <max_distance>]
               [-si <smoothing_iterations>] [-o <name>=<value> [<name>=<value> ...]]
               [-mask <filename>] [-interp {inv_dist,nearest}] [-b <band>]
               [-of <gdal_format>] [-co <name>=<value>]
               <src_file> <dst_file>


Description
-----------

:program:`gdal_fillnodata` fills selection regions (usually
nodata areas) by interpolating from valid pixels around the edges of the area.

Additional details on the algorithm are available in the
:cpp:func:`GDALFillNodata` docs.

.. note::

    gdal_fillnodata is a Python utility, and is only available if GDAL Python bindings are available.

.. include:: options/help_and_help_general.rst

.. option:: -q

    The script runs in quiet mode. The progress monitor is suppressed and
    routine messages are not displayed.

.. option:: -md <max_distance>

    The maximum distance (in pixels) that the algorithm will search out for
    values to interpolate. The default is 100 pixels.

.. option:: -si <smooth_iterations>

    The number of 3x3 average filter smoothing iterations to run after the
    interpolation to dampen artifacts. The default is zero smoothing iterations.

.. option:: -o <name>=<value>

    Specify a special argument to the algorithm. Currently none are supported.

.. option:: -b <band>

    The band to operate on, by default the first band is operated on.

.. option:: -mask <filename>

    Use the first band of the specified file as a validity mask (zero is
    invalid, non-zero is valid).

.. option:: -of <format>

    Select the output format. The default is :ref:`raster.gtiff`.
    Use the short format name.

.. option:: -interp {inv_dist,nearest}

    .. versionadded:: 3.9

    By default, pixels are interpolated using an inverse distance weighting
    (``inv_dist``). It is also possible to choose a nearest neighbour (``nearest``)
    strategy.

.. option:: <srcfile>

    The source raster file used to identify target pixels.
    Only one band is used.

.. option:: <dstfile>

    The new file to create with the interpolated result.
