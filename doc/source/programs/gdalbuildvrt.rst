.. _gdalbuildvrt:

================================================================================
gdalbuildvrt
================================================================================

.. only:: html

    Builds a VRT from a list of datasets.

.. Index:: gdalbuildvrt

Synopsis
--------

.. code-block::

    gdalbuildvrt [--help] [--long-usage] [--help-general]
                 [--quiet]
                 [[-strict]|[-non_strict]]
                 [-tile_index <field_name>]
                 [-resolution user|average|common|highest|lowest|same]
                 [-tr <xres> <yes>] [-input_file_list <filename>]
                 [[-separate]|[-pixel-function <function>]]
                 [-pixel-function-arg <NAME>=<VALUE>]...
                 [-allow_projection_difference] [-sd <n>] [-tap]
                 [-te <xmin> <ymin> <xmax> <ymax>] [-addalpha] [-b <band>]...
                 [-hidenodata] [-overwrite]
                 [-srcnodata "<value>[ <value>]..."]
                 [-vrtnodata "<value>[ <value>]..."] [-a_srs <srs_def>]
                 [-r nearest|bilinear|cubic|cubicspline|lanczos|average|mode]
                 [-oo <NAME>=<VALUE>]... [-co <NAME>=<VALUE>]...
                 [-ignore_srcmaskband]
                 [-nodata_max_mask_threshold <threshold>]
                 <vrt_dataset_name> [<src_dataset_name>]...


Description
-----------

This program builds a :ref:`VRT (Virtual Dataset) <raster.vrt>` that is a mosaic of a list of
input GDAL datasets. The list of input GDAL datasets can be specified at the end
of the command line, put in a text file (one filename per line) for very long lists,
or it can be a MapServer tileindex (see the :ref:`gdaltindex` utility). If using a tile index, all
entries in the tile index will be added to the VRT.

.. note::

    Starting with GDAL 3.9, for virtual mosaics with a very large number of source rasters
    (hundreds of thousands of source rasters, or more), it is advised to use the
    :ref:`gdaltindex` utility to generate a tile index compatible with the
    :ref:`GTI <raster.gti>` driver.

.. tip:: Equivalent in new "gdal" command line interface:

    See :ref:`gdal_raster_mosaic` and :ref:`gdal_raster_stack`.

.. program:: gdalbuildvrt

With :option:`-separate`, each input goes into a separate band in the VRT dataset. Otherwise,
the files are considered as source rasters of a larger mosaic and the VRT file has the same number of
bands as the input files.

If one GDAL dataset is made of several subdatasets and has 0 raster bands,
all the subdatasets will be added to the VRT rather than the dataset itself.

:program:`gdalbuildvrt` does some checks to ensure that all files that will be put
in the resulting VRT have similar characteristics: number of bands, projection, color
interpretation, etc. If not, files that do not match the common characteristics will be skipped
unless :option:`-strict` is used.
(This is only true in the default mode, and not when using the :option:`-separate` option)

Starting with GDAL 3.12, a function (e.g., ``min``, ``mean``, ``median``) can
be specified (:option:`-pixel-function`) to calculate pixel values from
overlapping inputs. If no function is specified, or in earlier versions,
the order of the input list is used to determine priority.
Files that are listed at the end are the ones
from which the content will be fetched. Note that nodata will be taken into account
to potentially fetch data from lower-priority datasets, but currently, alpha channel
is not taken into account to do alpha compositing (so a source with alpha=0
appearing on top of another source will override its content). This might be
changed in later versions.

.. include:: options/help_and_help_general.rst

.. option:: -tileindex <field_name>

    Use the specified value as the tile index field, instead of the default
    value which is 'location'.

.. option:: -resolution {highest|lowest|average|user|same}

    In case the resolution of all input files is not the same, the :option:`-resolution` flag
    enables the user to control the way the output resolution is computed.

    `highest` will pick the smallest values of pixel dimensions within the set of source rasters.

    `lowest` will pick the largest values of pixel dimensions within the set of source rasters.

    `average` is the default and will compute an average of pixel dimensions within the set of source rasters.

    `user` must be used in combination with the :option:`-tr` option to specify the target resolution.

    `same` (added in GDAL 3.11) checks that all source rasters have the same resolution and errors out when this is not the case.

    `common` (added in GDAL 3.11) determines the greatest common divisor of the source pixel dimensions, e.g. 0.2 for source pixel dimensions of 0.4 and 0.6.

.. option:: -tr <xres> <yres>

    Set target resolution. The values must be expressed in georeferenced units.
    Both must be positive values. Specifying these values is of course incompatible with
    highest|lowest|average values for :option:`-resolution` option.


.. option:: -tap

    (target aligned pixels) align
    the coordinates of the extent of the output file to the values of the :option:`-tr`,
    such that the aligned extent includes the minimum extent.
    Alignment means that xmin / resx, ymin / resy, xmax / resx and ymax / resy are integer values.

.. option:: -te <xmin> <ymin> <xmax> <ymax>

    Set georeferenced extents of VRT file. The values must be expressed in georeferenced units.
    If not specified, the extent of the VRT is the minimum bounding box of the set of source rasters.
    Pixels within the extent of the VRT but not covered by a source raster will be read as valid
    pixels with a value of zero unless a NODATA value is specified using :option:`-vrtnodata`
    or an alpha mask band is added with :option:`-addalpha`.

.. option:: -addalpha

    Adds an alpha mask band to the VRT when the source raster have none. Mainly useful for RGB sources (or grey-level sources).
    The alpha band is filled on-the-fly with the value 0 in areas without any source raster, and with value
    255 in areas with source raster. The effect is that a RGBA viewer will render
    the areas without source rasters as transparent and areas with source rasters as opaque.
    This option is not compatible with :option:`-separate`.

.. option:: -hidenodata

    Even if any band contains nodata value, giving this option makes the VRT band
    not report the NoData. Useful when you want to control the background color of
    the dataset. By using along with the -addalpha option, you can prepare a
    dataset which doesn't report nodata value but is transparent in areas with no
    data.

.. option:: -srcnodata "<value>[ <value>]..."

    Set nodata values for input bands (different values can be supplied for each band). If
    more than one value is supplied all values should be quoted to keep them
    together as a single operating system argument. If the option is not specified, the
    intrinsic nodata settings on the source datasets will be used (if they exist). The value set by this option
    is written in the NODATA element of each ``ComplexSource`` element. Use a value of
    ``None`` to ignore intrinsic nodata settings on the source datasets.

.. option:: -ignore_srcmaskband

    .. versionadded:: 3.3

    Starting with GDAL 3.3, if a source has a mask band (internal/external mask
    band, or alpha band), a ``<ComplexSource>`` element is created by default with
    a ``<UseMaskBand>true</UseMaskBand>`` child element, to instruct the VRT driver
    to use the mask band of the source to mask pixels being composited. This is
    a generalization of the NODATA element.
    When specifying the :option:`-ignore_srcmaskband` option, the mask band of sources will
    not be taken into account, and in case of overlapping between sources, the
    last one will override previous ones in areas of overlap.

.. option:: -nodata_max_mask_threshold <threshold>

    .. versionadded:: 3.9

    Insert a ``<NoDataFromMaskSource>`` source, which replaces the value of the source
    with the value of :option:`-vrtnodata` (or 0 if not specified) when the value
    of the mask band of the source is less or equal to the threshold.
    This is typically used to transform a R,G,B,A image into a R,G,B one with a NoData value.

.. option:: -b <band>

    Select an input <band> to be processed. Bands are numbered from 1.
    If input bands not set all bands will be added to the VRT.
    Multiple :option:`-b` switches may be used to select a set of input bands.

.. option:: -sd <n>

    If the input dataset contains several subdatasets, use a subdataset with the
    specified number (starting from 1). This is an alternative to giving the full subdataset
    name as an input to the utility.

.. option:: -vrtnodata "<value>[ <value>]..."

    Set nodata values at the VRT band level (different values can be supplied for each band).  If more
    than one value is supplied, all values should be quoted to keep them together
    as a single operating system argument (:example:`vrtnodata`). If the option is not specified,
    intrinsic nodata settings on the first dataset will be used (if they exist). The value set by this option
    is written in the ``NoDataValue`` element of each ``VRTRasterBand element``. Use a value of
    `None` to ignore intrinsic nodata settings on the source datasets.

.. option:: -separate

    Place each input file into a separate band. See :example:`separate`.
    Contrary to the default mode, it is not
    required that all bands have the same datatype.
    This option is mutually exclusive with :option:`-pixel-function`.

    Starting with GDAL 3.8, all bands of each input file are added as separate
    VRT bands, unless :option:`-b` is specified to select a subset of them.
    Before GDAL 3.8, only the first band of each input file was placed into a
    new VRT band, and :option:`-b` was ignored.

.. option:: -pixel-function

    Specify a function name to calculate a value from overlapping inputs.
    For a list of available pixel functions, see :ref:`builtin_pixel_functions`.
    If no function is specified, values will be taken from the last overlapping input.
    This option is mutually exclusive with with :option:`-separate`.

    .. versionadded:: 3.12

.. option:: -pixel-function-arg

    Specify an argument to be provided to a pixel function, in the format
    ``<NAME>=<VALUE>``. Multiple arguments may be specified by repeating this
    option.

    .. versionadded:: 3.12

.. option:: -allow_projection_difference

    When this option is specified, the utility will create a VRT even if the input datasets do not have
    the same projection. Note: this does not mean that they will be reprojected. Their projection will
    just be ignored.

.. option:: -a_srs <srs_def>

    Override the projection for the output file.  The <srs_def> may be any of the usual GDAL/OGR forms,
    complete WKT, PROJ.4, EPSG:n or a file containing the WKT. No reprojection is done.

.. option:: -r {nearest|bilinear|cubic|cubicspline|lanczos|average|mode}

    Select a resampling algorithm. Nearest is the default

.. option:: -oo <NAME>=<VALUE>

    Dataset open option (format-specific)

.. option:: -co <NAME>=<VALUE>

    Specify a :ref:`VRT driver creation option <raster_vrt_creation_options>`.

    .. versionadded:: 3.10

.. option:: -input_file_list <filename>

    Specify a text file with an input filename on each line. See :example:`filelist`.

.. option:: -q

    Disable the progress bar on the console

.. option:: -overwrite

    Overwrite the VRT if it already exists.

.. option:: -strict

    Turn warnings as failures. This is mutually exclusive with :option:`-non_strict`, the latter which is the default.

    .. versionadded:: 3.4.2

.. option:: -non_strict

    Skip source datasets that have issues with warnings, and continue processing. This is the default.

    .. versionadded:: 3.4.2

.. option:: -write_absolute_path

    .. versionadded:: 3.12.0

    Enables writing the absolute path of the input datasets. By default, input
    filenames are written in a relative way with respect to the VRT filename (when possible).

Examples
--------

.. example::
   :title: Make a virtual mosaic from all TIFF files contained in a directory

   .. code-block:: bash

       gdalbuildvrt doq_index.vrt doq/*.tif

.. example::
   :title: Make a virtual mosaic from files whose name is specified in a text file
   :id: filelist

   .. code-block:: bash

    gdalbuildvrt -input_file_list my_list.txt doq_index.vrt

.. example::
   :title: Make a RGB virtual mosaic from 3 single-band input files
   :id: separate

   .. code-block:: bash

       gdalbuildvrt -separate rgb.vrt red.tif green.tif blue.tif

.. example::
   :title: Make a virtual mosaic with blue background colour (RGB: 0 0 255)
   :id: vrtnodata

   .. code-block:: bash

       gdalbuildvrt -hidenodata -vrtnodata "0 0 255" doq_index.vrt doq/*.tif

C API
-----

This utility is also callable from C with :cpp:func:`GDALBuildVRT`.
