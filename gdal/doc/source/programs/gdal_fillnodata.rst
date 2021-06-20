.. _gdal_fillnodata:

================================================================================
gdal_fillnodata.py
================================================================================

.. only:: html

    Fill raster regions by interpolation from edges.

.. Index:: gdal_fillnodata

Synopsis
--------

.. code-block::

    gdal_fillnodata.py [-q] [-md max_distance] [-si smooth_iterations]
                    [-o name=value] [-b band]
                    srcfile [-nomask] [-mask filename] [-of format] [dstfile]

Description
-----------

:program:`gdal_fillnodata.py` script fills selection regions (usually
nodata areas) by interpolating from valid pixels around the edges of the area.

Additional details on the algorithm are available in the
:cpp:func:`GDALFillNodata` docs.

.. option:: -q

    The script runs in quiet mode. The progress monitor is suppressed and
    routine messages are not displayed.

.. option:: -md max_distance

    The maximum distance (in pixels) that the algorithm will search out for
    values to interpolate. The default is 100 pixels.

.. option:: -si smooth_iterations

    The number of 3x3 average filter smoothing iterations to run after the
    interpolation to dampen artifacts. The default is zero smoothing iterations.

.. option:: -o name=value

    Specify a special argument to the algorithm. Currently none are supported.

.. option:: -b band

    The band to operate on, by default the first band is operated on.

.. option:: srcfile

    The source raster file used to identify target pixels.
    Only one band is used.

.. option:: -nomask

    Do not use the default validity mask for the input band (such as nodata,
    or alpha masks).

.. option:: -mask filename

    Use the first band of the specified file as a validity mask (zero is
    invalid, non-zero is valid).

.. option:: dstfile

    The new file to create with the interpolated result.
    If not provided, the source band is updated in place.

.. option:: -of format

    Select the output format. The default is :ref:`raster.gtiff`.
    Use the short format name.
