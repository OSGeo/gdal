.. _gdal_raster_buildvrt_subcommand:

================================================================================
"gdal raster buildvrt" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Build a virtual dataset (VRT).

.. Index:: gdal raster buildvrt

Synopsis
--------

.. code-block::

    Usage: gdal raster buildvrt [OPTIONS] <INPUTS> <OUTPUT>

    Build a virtual dataset (VRT).

    Positional arguments:
      -i, --input <INPUTS>                                Input raster datasets (or specify a @<filename> to point to a file containing filenames) [1.. values]
      -o, --output <OUTPUT>                               Output raster dataset (created by algorithm) [required]

    Common Options:
      -h, --help                                          Display help message and exit
      --version                                           Display GDAL version and exit
      --json-usage                                        Display usage as JSON document and exit
      --drivers                                           Display driver list as JSON document and exit
      --progress                                          Display progress bar

    Options:
      --co, --creation-option <KEY>=<VALUE>               Creation option [may be repeated]
      -b, --band <BAND>                                   Specify input band(s) number. [may be repeated]
      --separate                                          Place each input file into a separate band.
      --overwrite                                         Whether overwriting existing output is allowed
      --resolution <xres>,<yres>|average|highest|lowest>  Target resolution (in destination CRS units)
      --bbox <xmin,ymin,xmax,ymax>                        Target bounding box (in destination CRS units)
      --target-aligned-pixels                             Round target extent to target resolution
      --srcnodata <SRCNODATA>                             Set nodata values for input bands. [1.. values]
      --vrtnodata <VRTNODATA>                             Set nodata values at the VRT band level. [1.. values]
      --hidenodata                                        Makes the VRT band not report the NoData.
      --addalpha                                          Adds an alpha mask band to the VRT when the source raster have none.


Description
-----------

This program builds a :ref:`VRT (Virtual Dataset) <raster.vrt>` that is a mosaic of a list of
input GDAL datasets. The list of input GDAL datasets can be specified at the end
of the command line or put in a text file (one filename per line) for very long lists.
Wildcards '*', '?' or '['] of :cpp:func:`VSIGlob` can be used, even on files located
on network file systems such as /vsis3/, /vsigs/, /vsiaz/, etc.

With :option:`--separate`, each input goes into a separate band in the VRT dataset. Otherwise,
the files are considered as source rasters of a larger mosaic and the VRT file has the same number of
bands as the input files.

If one GDAL dataset is made of several subdatasets and has 0 raster bands,
all the subdatasets will be added to the VRT rather than the dataset itself.

:program:`gdal raster buildvrt` does some checks to ensure that all files that will be put
in the resulting VRT have similar characteristics: number of bands, projection, color
interpretation, etc. If not, files that do not match the common characteristics will be skipped.
(This is only true in the default mode, and not when using the :option:`--separate` option)

If the inputs spatially overlap, the order of the input list is used to determine priority.
Files that are listed at the end are the ones
from which the content will be fetched. Note that nodata will be taken into account
to potentially fetch data from lower-priority datasets, but currently, alpha channel
is not taken into account to do alpha compositing (so a source with alpha=0
appearing on top of another source will override its content). This might be
changed in later versions.

The following options are available:

.. option:: -b <band>

    Select an input <band> to be processed. Bands are numbered from 1.
    If input bands not set all bands will be added to the VRT.
    Multiple :option:`-b` switches may be used to select a set of input bands.

.. option:: --separate

    Place each input file into a separate band. See :example:`separate`.
    Contrary to the default mode, it is not
    required that all bands have the same datatype.

    All bands of each input file are added as separate
    VRT bands, unless :option:`-b` is specified to select a subset of them.

.. option:: --resolution {<xres,yres>|highest|lowest|average}

    In case the resolution of all input files is not the same, the :option:`--resolution` flag
    enables the user to control the way the output resolution is computed.

    `highest` will pick the smallest values of pixel dimensions within the set of source rasters.

    `lowest` will pick the largest values of pixel dimensions within the set of source rasters.

    `average` is the default and will compute an average of pixel dimensions within the set of source rasters.

    <xres>,<yres>. The values must be expressed in georeferenced units.
    Both must be positive values.

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Set georeferenced extents of VRT file. The values must be expressed in georeferenced units.
    If not specified, the extent of the VRT is the minimum bounding box of the set of source rasters.
    Pixels within the extent of the VRT but not covered by a source raster will be read as valid
    pixels with a value of zero unless a NODATA value is specified using :option:`--vrtnodata`
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

.. option:: --vrtnodata <value>[,<value>]...

    Set nodata values at the VRT band level (different values can be supplied for each band).  If more
    than one value is supplied, all values should be quoted to keep them together
    as a single operating system argument (:example:`vrtnodata`). If the option is not specified,
    intrinsic nodata settings on the first dataset will be used (if they exist). The value set by this option
    is written in the ``NoDataValue`` element of each ``VRTRasterBand element``. Use a value of
    `None` to ignore intrinsic nodata settings on the source datasets.

.. option:: --addalpha

    Adds an alpha mask band to the VRT when the source raster have none. Mainly useful for RGB sources (or grey-level sources).
    The alpha band is filled on-the-fly with the value 0 in areas without any source raster, and with value
    255 in areas with source raster. The effect is that a RGBA viewer will render
    the areas without source rasters as transparent and areas with source rasters as opaque.
    This option is not compatible with :option:`--separate`.

.. option:: --hidenodata

    Even if any band contains nodata value, giving this option makes the VRT band
    not report the NoData. Useful when you want to control the background color of
    the dataset. By using along with the -addalpha option, you can prepare a
    dataset which doesn't report nodata value but is transparent in areas with no
    data.


Examples
--------

.. example::
   :title: Make a RGB virtual mosaic from 3 single-band input files
   :id: separate

   .. code-block:: bash

       gdal raster biuldvrt --separate red.tif green.tif blue.tif rgb.vrt

.. example::
   :title: Make a virtual mosaic with blue background colour (RGB: 0 0 255)
   :id: vrtnodata

   .. code-block:: bash

       gdal raster buildvrt --hidenodata --vrtnodata=0,0,255 doq/*.tif doq_index.vrt


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    buildvrt

