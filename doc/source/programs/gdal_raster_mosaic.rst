.. _gdal_raster_mosaic:

================================================================================
``gdal raster mosaic``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Build a mosaic, either virtual (VRT) or materialized

.. Index:: gdal raster mosaic

Synopsis
--------

.. program-output:: gdal raster mosaic --help-doc

Description
-----------

This program builds a mosaic of a list of input GDAL datasets, that can be
either a virtual mosaic in the :ref:`VRT (Virtual Dataset) <raster.vrt>` format,
or in a more conventional raster format such as GeoTIFF.

The list of input GDAL datasets can be specified at the end
of the command line or put in a text file (one filename per line) for very long lists.
Wildcards '*', '?' or '['] of :cpp:func:`VSIGlob` can be used, even on files located
on network file systems such as /vsis3/, /vsigs/, /vsiaz/, etc.

:program:`gdal raster mosaic` does some checks to ensure that all files that will be put
in the resulting file have similar characteristics: number of bands, projection, color
interpretation, etc. If not, files that do not match the common characteristics will be skipped.

If the inputs spatially overlap, the order of the input list is used to determine priority.
Files that are listed at the end are the ones
from which the content will be fetched. Note that nodata will be taken into account
to potentially fetch data from lower-priority datasets, but currently, alpha channel
is not taken into account to do alpha compositing (so a source with alpha=0
appearing on top of another source will override its content). This might be
changed in later versions.

The following options are available:

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: -b <band>

    Select an input <band> to be processed. Bands are numbered from 1.
    If input bands not set all bands will be added to the output.
    Multiple :option:`-b` switches may be used to select a set of input bands.

.. option:: --resolution {<xres,yres>|same|highest|lowest|average}

    In case the resolution of all input files is not the same, the :option:`--resolution` flag
    enables the user to control the way the output resolution is computed.

    `same`, the default, checks that all source rasters have the same resolution and errors out when this is not the case.

    `highest` will pick the smallest values of pixel dimensions within the set of source rasters.

    `lowest` will pick the largest values of pixel dimensions within the set of source rasters.

    `average` will compute an average of pixel dimensions within the set of source rasters.

    `common` determines the greatest common divisor of the source pixel dimensions, e.g. 0.2 for source pixel dimensions of 0.4 and 0.6.

    <xres>,<yres>. The values must be expressed in georeferenced units.
    Both must be positive values.


.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Set georeferenced extents of output file. The values must be expressed in georeferenced units.
    If not specified, the extent of the output is the minimum bounding box of the set of source rasters.
    Pixels within the extent of the output but not covered by a source raster will be read as valid
    pixels with a value of zero unless a NODATA value is specified using :option:`--dstnodata`
    or an alpha mask band is added with :option:`--addalpha`.

.. option:: --target-aligned-pixels

    (target aligned pixels) align
    the coordinates of the extent of the output file to the values of the :option:`--resolution`,
    such that the aligned extent includes the minimum extent.
    Alignment means that xmin / resx, ymin / resy, xmax / resx and ymax / resy are integer values.

.. option:: --srcnodata <value>[,<value>]...

    Set nodata values for input bands (different values can be supplied for each band).
    If the option is not specified, the intrinsic nodata settings on the source datasets
    will be used (if they exist). The value set by this option is written in the NODATA element
    of each ``ComplexSource`` element.

.. option:: --dstnodata <value>[,<value>]...

    Set nodata values at the output band level (different values can be supplied for each band).  If more
    than one value is supplied, all values should be quoted to keep them together
    as a single operating system argument (:example:`dstnodata`). If the option is not specified,
    intrinsic nodata settings on the first dataset will be used (if they exist). The value set by this option
    is written in the ``NoDataValue`` element of each ``VRTRasterBand element``. Use a value of
    `None` to ignore intrinsic nodata settings on the source datasets.

.. option:: --addalpha

    Adds an alpha mask band to the output when the source raster have none. Mainly useful for RGB sources (or grey-level sources).
    The alpha band is filled on-the-fly with the value 0 in areas without any source raster, and with value
    255 in areas with source raster. The effect is that a RGBA viewer will render
    the areas without source rasters as transparent and areas with source rasters as opaque.

.. option:: --hidenodata

    Even if any band contains nodata value, giving this option makes the output band
    not report the NoData. Useful when you want to control the background color of
    the dataset. By using along with the -addalpha option, you can prepare a
    dataset which doesn't report nodata value but is transparent in areas with no
    data.

GDALG output (on-the-fly / streamed dataset)
--------------------------------------------

This program supports serializing the command line as a JSON file using the ``GDALG`` output format.
The resulting file can then be opened as a raster dataset using the
:ref:`raster.gdalg` driver, and apply the specified pipeline in a on-the-fly /
streamed way.

Examples
--------

.. example::
   :title: Make a virtual mosaic with blue background colour (RGB: 0 0 255)
   :id: dstnodata

   .. code-block:: bash

       gdal raster mosaic --hidenodata --dstnodata=0,0,255 doq/*.tif doq_index.vrt


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    mosaic

