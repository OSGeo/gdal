.. _gdal_translate:

================================================================================
gdal_translate
================================================================================

.. only:: html

    Converts raster data between different formats.

.. Index:: gdal_translate

Synopsis
--------

.. code-block::


    gdal_translate [--help-general]
        [-ot {Byte/Int16/UInt16/UInt32/Int32/UInt64/Int64/Float32/Float64/
                CInt16/CInt32/CFloat32/CFloat64}] [-strict]
        [-if format]* [-of format]
        [-b band]* [-mask band] [-expand {gray|rgb|rgba}]
        [-outsize xsize[%]|0 ysize[%]|0] [-tr xres yres]
        [-r {nearest,bilinear,cubic,cubicspline,lanczos,average,rms,mode}]
        [-unscale] [-scale[_bn] [src_min src_max [dst_min dst_max]]]* [-exponent[_bn] exp_val]*
        [-srcwin xoff yoff xsize ysize] [-epo] [-eco]
        [-projwin ulx uly lrx lry] [-projwin_srs srs_def]
        [-a_srs srs_def] [-a_coord_epoch <epoch>]
        [-a_ullr ulx uly lrx lry] [-a_nodata value]
        [-a_scale value] [-a_offset value]
        [-nogcp] [-gcp pixel line easting northing [elevation]]*
        |-colorinterp{_bn} {red|green|blue|alpha|gray|undefined}]
        |-colorinterp {red|green|blue|alpha|gray|undefined},...]
        [-mo "META-TAG=VALUE"]* [-q] [-sds]
        [-co "NAME=VALUE"]* [-stats] [-norat] [-noxmp]
        [-oo NAME=VALUE]*
        src_dataset dst_dataset

Description
-----------

The :program:`gdal_translate` utility can be used to convert raster data between
different formats, potentially performing some operations like subsettings,
resampling, and rescaling pixels in the process.

.. program:: gdal_translate

.. include:: options/ot.rst

.. option:: -strict

    Don't be forgiving of mismatches and lost data when translating to the
    output format.

.. include:: options/if.rst

.. include:: options/of.rst

.. option:: -b <band>

    Select an input band **band** for output. Bands are numbered from 1.
    Multiple :option:`-b` switches may be used to select a set of input bands
    to write to the output file, or to reorder bands. **band** can also be set
    to "mask,1" (or just "mask") to mean the mask band of the first band of the
    input dataset.

.. option:: -mask <band>

    Select an input band **band** to create output dataset mask band. Bands are
    numbered from 1. **band** can be set to "none" to avoid copying the global
    mask of the input dataset if it exists. Otherwise it is copied by default
    ("auto"), unless the mask is an alpha channel, or if it is explicitly used
    to be a regular band of the output dataset ("-b mask"). **band** can also
    be set to "mask,1" (or just "mask") to mean the mask band of the 1st band
    of the input dataset.

.. option:: -expand gray|rgb|rgba

    To expose a dataset with 1 band with a color table as a dataset with
    3 (RGB) or 4 (RGBA) bands. Useful for output drivers such as JPEG,
    JPEG2000, MrSID, ECW that don't support color indexed datasets. The 'gray'
    value enables to expand a dataset with a color table that only contains
    gray levels to a gray indexed dataset.

.. option:: -outsize <xsize>[%]|0 <ysize>[%]|0

    Set the size of the output file.  Outsize is in pixels and lines unless '%'
    is attached in which case it is as a fraction of the input image size.
    If one of the 2 values is set to 0, its value will be determined from the
    other one, while maintaining the aspect ratio of the source dataset.

.. option:: -tr <xres> <yres>

    set target resolution. The values must be expressed in georeferenced units.
    Both must be positive values. This is mutually exclusive with
    :option:`-outsize` and :option:`-a_ullr`.

.. option:: -r {nearest (default),bilinear,cubic,cubicspline,lanczos,average,rms,mode}

    Select a resampling algorithm.

    ``nearest`` applies a nearest neighbour (simple sampling) resampler

    ``average`` computes the average of all non-NODATA contributing pixels. Starting with GDAL 3.1, this is a weighted average taking into account properly the weight of source pixels not contributing fully to the target pixel.

    ``rms`` computes the root mean squared / quadratic mean of all non-NODATA contributing pixels (GDAL >= 3.3)

    ``bilinear`` applies a bilinear convolution kernel.

    ``cubic`` applies a cubic convolution kernel.

    ``cubicspline`` applies a B-Spline convolution kernel.

    ``lanczos`` applies a Lanczos windowed sinc convolution kernel.

    ``mode`` selects the value which appears most often of all the sampled points.

.. option:: -scale [src_min src_max [dst_min dst_max]]

    Rescale the input pixels values from the range **src_min** to **src_max**
    to the range **dst_min** to **dst_max**. If omitted the output range is 0
    to 255.  If omitted the input range is automatically computed from the
    source data. Note that these values are only used to compute a scale and
    offset to apply to the input raster values. In particular, src_min and
    src_max are not used to clip input values.
    -scale can be repeated several times (if specified only once,
    it also applies to all bands of the output dataset), so as to specify per
    band parameters. It is also possible to use the "-scale_bn" syntax where bn
    is a band number (e.g. "-scale_2" for the 2nd band of the output dataset)
    to specify the parameters of one or several specific bands.

.. option:: -exponent <exp_val>

    To apply non-linear scaling with a power function. exp_val is the exponent
    of the power function (must be positive). This option must be used with the
    -scale option. If specified only once, -exponent applies to all bands of
    the output image. It can be repeated several times so as to specify per
    band parameters. It is also possible to use the "-exponent_bn" syntax where
    bn is a band number (e.g. "-exponent_2" for the 2nd band of the output
    dataset) to specify the parameters of one or several specific bands.

.. option:: -unscale

    Apply the scale/offset metadata for the bands to convert scaled values to
    unscaled values.  It is also often necessary to reset the output datatype
    with the :option:`-ot` switch.

.. option:: -srcwin <xoff> <yoff> <xsize> <ysize>

    Selects a subwindow from the source image for copying based on pixel/line location.

.. option:: -projwin <ulx> <uly> <lrx> <lry>

    Selects a subwindow from the source image for copying
    (like :option:`-srcwin`) but with the corners given in georeferenced
    coordinates (by default expressed in the SRS of the dataset. Can be
    changed with :option:`-projwin_srs`).

    .. note::

        In GDAL 2.1.0 and 2.1.1, using -projwin with coordinates not aligned
        with pixels will result in a sub-pixel shift. This has been corrected
        in later versions. When selecting non-nearest neighbour resampling,
        starting with GDAL 2.1.0, sub-pixel accuracy is however used to get
        better results.

.. option:: -projwin_srs <srs_def>

    Specifies the SRS in which to interpret the coordinates given with
    :option:`-projwin`. The <srs_def> may be any of the usual GDAL/OGR forms,
    complete WKT, PROJ.4, EPSG:n or a file containing the WKT.

    .. warning::
        This does not cause reprojection of the dataset to the specified SRS.

.. option:: -epo

    (Error when Partially Outside) If this option is set, :option:`-srcwin` or
    :option:`-projwin` values that falls partially outside the source raster
    extent will be considered as an error. The default behavior is to accept
    such requests, when they were considered as an error before.

.. option:: -eco

    (Error when Completely Outside) Same as :option:`-epo`, except that the
    criterion for erroring out is when the request falls completely outside
    the source raster extent.

.. option:: -a_srs <srs_def>

    Override the projection for the output file.

    .. include:: options/srs_def.rst

    .. note:: No reprojection is done.

.. option:: -a_coord_epoch <epoch>

    .. versionadded:: 3.4

    Assign a coordinate epoch, linked with the output SRS. Useful when the
    output SRS is a dynamic CRS.

.. option:: -a_scale <value>

    Set band scaling value (no modification of pixel values is done)

    .. versionadded:: 2.3

.. option:: -a_offset<value>

    Set band offset value (no modification of pixel values is done)

    .. versionadded:: 2.3

.. option:: -a_ullr <ulx> <uly> <lrx> <lry>

    Assign/override the georeferenced bounds of the output file.  This assigns
    georeferenced bounds to the output file, ignoring what would have been
    derived from the source file. So this does not cause reprojection to the
    specified SRS.

.. option:: -a_nodata <value>

    Assign a specified nodata value to output bands. It can
    be set to ``none`` to avoid setting a nodata value to the output file if
    one exists for the source file. Note that, if the input dataset has a
    nodata value, this does not cause pixel values that are equal to that nodata
    value to be changed to the value specified with this option.

.. option:: -colorinterp_X <red|green|blue|alpha|gray|undefined>

    Override the color interpretation of band X (where X is a valid band number,
    starting at 1)

    .. versionadded:: 2.3

.. option:: -colorinterp <red|green|blue|alpha|gray|undefined[,red|green|blue|alpha|gray|undefined]*>

    Override the color interpretation of all specified bands. For
    example -colorinterp red,green,blue,alpha for a 4 band output dataset.

    .. versionadded:: 2.3

.. option:: -mo META-TAG=VALUE

    Passes a metadata key and value to set on the output dataset if possible.

.. include:: options/co.rst

.. option:: -nogcp

    Do not copy the GCPs in the source dataset to the output dataset.

.. option:: -gcp <pixel> <line> <easting> <northing> <elevation>

    Add the indicated ground control point to the output dataset.  This option
    may be provided multiple times to provide a set of GCPs.

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: -sds

    Copy all subdatasets of this file to individual output files.  Use with
    formats like HDF that have subdatasets.

.. option:: -stats

    Force (re)computation of statistics.

.. option:: -norat

    Do not copy source RAT into destination dataset.

.. option:: -noxmp

    Do not copy the XMP metadata in the source dataset to the output dataset when driver is able to copy it.

    .. versionadded:: 3.2

.. option:: -oo NAME=VALUE

    Dataset open option (format specific)

.. option:: <src_dataset>

    The source dataset name. It can be either file name, URL of data source or
    subdataset name for multi-dataset files.

.. option:: <dst_dataset>

    The destination file name.

C API
-----

This utility is also callable from C with :cpp:func:`GDALTranslate`.

.. versionadded:: 2.1

Examples
--------

::

    gdal_translate -of GTiff -co "TILED=YES" utm.tif utm_tiled.tif


To create a JPEG-compressed TIFF with internal mask from a RGBA dataset

::

    gdal_translate rgba.tif withmask.tif -b 1 -b 2 -b 3 -mask 4 -co COMPRESS=JPEG -co PHOTOMETRIC=YCBCR --config GDAL_TIFF_INTERNAL_MASK YES


To create a RGBA dataset from a RGB dataset with a mask

::

    gdal_translate withmask.tif rgba.tif -b 1 -b 2 -b 3 -b mask

