.. _gdal_polygonize:

================================================================================
gdal_polygonize.py
================================================================================

.. only:: html

    Produces a polygon feature layer from a raster.

.. Index:: gdal_polygonize

Synopsis
--------

.. code-block::

    gdal_polygonize.py [-8] [-nomask] [-mask filename] <raster_file> [-b band]
                       [-q] [-f ogr_format] <out_file> [layer] [fieldname]

Description
-----------
This utility creates vector polygons for all connected regions of pixels in
the raster sharing a common pixel value.  Each polygon is created with an
attribute indicating the pixel value of that polygon.  A raster mask
may also be provided to determine which pixels are eligible for processing.

The utility will create the output vector datasource if it does not already
exist, defaulting to GML format.

The utility is based on the ::cpp:func:`GDALPolygonize` function which has additional
details on the algorithm.

.. program:: gdal_polygonize

.. option:: -8

    Use 8 connectedness. Default is 4 connectedness.

.. option:: -nomask

    Do not use the default validity mask for the input band (such as nodata, or
    alpha masks).

.. option:: -mask <filename>

    Use the first band of the specified file as a validity mask (zero is invalid,
    non-zero is valid). If not specified, the default validity mask for the input
    band (such as nodata, or alpha masks) will be used (unless -nomask is specified)

.. option:: <raster_file>

    The source raster file from which polygons are derived.

.. option:: -b <band>

    The band on <raster_file> to build
    the polygons from. Starting with GDAL 2.2, the value can also be set to "mask",
    to indicate that the mask band of the first band must be used (or
    "mask,band_number" for the mask of a specified band)

.. option:: -f <ogr_format>

    Select the output format. Starting with
    GDAL 2.3, if not specified, the format is guessed from the extension (previously
    was GML). Use the short format name

.. option:: <out_file>

    The destination vector file to which the polygons will be written.

.. option:: <layer>

    The name of the layer created to hold the polygon features.

.. option:: <fieldname>

    The name of the field to create (defaults to "DN").

.. option:: -q

    The script runs in quiet mode.  The progress monitor is suppressed and routine
    messages are not displayed.
