.. _gdalwarp:

================================================================================
gdalwarp
================================================================================

.. only:: html

    Image reprojection and warping utility

.. Index:: gdalwarp

Synopsis
--------

.. program-output:: gdalwarp --help-doc

Description
-----------

The :program:`gdalwarp` utility is an image mosaicing, reprojection and warping
utility. The program can reproject to any supported projection,
and can also apply GCPs stored with the image if the image is "raw"
with control information.

.. tip:: Equivalent in new "gdal" command line interface:

    * :ref:`gdal_raster_reproject` for reprojection
    * :ref:`gdal_raster_update` to update the content of a raster with another one

.. program:: gdalwarp

.. include:: options/help_and_help_general.rst

.. option:: -b <n>

.. option:: -srcband <n>

    .. versionadded:: 3.7

    Specify an input band number to warp (between 1 and the number of bands
    of the source dataset).

    This option is used to warp a subset of the input bands. All input bands
    are used when it is not specified.

    This option may be repeated multiple times to select several input bands.
    The order in which bands are specified will be the order in which they
    appear in the output dataset (unless :option:`-dstband` is specified).

    The alpha band should not be specified in the list, as it will be
    automatically retrieved (unless :option:`-nosrcalpha` is specified).

    The following invocation will warp an input datasets with bands ordered as
    Blue, Green, Red, NearInfraRed in an output dataset with bands ordered as
    Red, Green, Blue.

    .. code-block:: bash

        gdalwarp in_bgrn.tif out_rgb.tif -b 3 -b 2 -b 1 -overwrite


.. option:: -dstband <n>

    .. versionadded:: 3.7

    Specify the output band number in which to warp. In practice, this option
    is only useful when updating an existing dataset, e.g to warp one band at
    at time.

    .. code-block:: bash

        gdal_create -if in_red.tif -bands 3 out_rgb.tif
        gdalwarp in_red.tif out_rgb.tif -srcband 1 -dstband 1
        gdalwarp in_green.tif out_rgb.tif -srcband 1 -dstband 2
        gdalwarp in_blue.tif out_rgb.tif -srcband 1 -dstband 3


    If :option:`-srcband` is specified, there must be as many occurrences of
    :option:`-dstband` as there are of :option:`-srcband`.

    The output alpha band should not be specified, as it will be automatically
    created if the input dataset has an alpha band, or if :option:`-dstalpha`
    is specified.

    If :option:`-dstband` is not specified, then
    ``-dstband 1 -dstband 2 ... -dstband N`` is assumed where N is the number
    of input bands (specified explicitly either with :option:`-srcband` or
    implicitly)

.. option:: -s_srs <srs def>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

.. option:: -s_coord_epoch <epoch>

    .. versionadded:: 3.4

    Assign a coordinate epoch, linked with the source SRS. Useful when the
    source SRS is a dynamic CRS. Only taken into account if :option:`-s_srs`
    is used.

    Before PROJ 9.4, :option:`-s_coord_epoch` and :option:`-t_coord_epoch` were
    mutually exclusive, due to lack of support for transformations between two dynamic CRS.

.. option:: -t_srs <srs_def>

    Set target spatial reference.

    A source SRS must be available for reprojection to occur. The source SRS
    will be by default the one found in the input dataset when it is available,
    or as overridden by the user with :option:`-s_srs`

    .. include:: options/srs_def_gdalwarp.rst

.. option:: -t_coord_epoch <epoch>

    .. versionadded:: 3.4

    Assign a coordinate epoch, linked with the target SRS. Useful when the
    target SRS is a dynamic CRS. Only taken into account if :option:`-t_srs`
    is used.

    Before PROJ 9.4, :option:`-s_coord_epoch` and :option:`-t_coord_epoch` were
    mutually exclusive, due to lack of support for transformations between two dynamic CRS.

.. option:: -ct <string>

    A PROJ string (single step operation or multiple step string
    starting with +proj=pipeline), a WKT2 string describing a CoordinateOperation,
    or a urn:ogc:def:coordinateOperation:EPSG::XXXX URN overriding the default
    transformation from the source to the target CRS.

    It must take into account the axis order of the source and target CRS, that
    is typically include a ``step proj=axisswap order=2,1`` at the beginning of
    the pipeline if the source CRS has northing/easting axis order, and/or at
    the end of the pipeline if the target CRS has northing/easting axis order.

    When creating a new output file, using :option:`-t_srs` is still necessary
    to have the target CRS written in the metadata of the output file,
    but the parameters of the CoordinateOperation will override those of the
    standard transformation.

    .. versionadded:: 3.0

.. option:: -to <NAME>=<VALUE>

    Set a transformer option suitable to pass to :cpp:func:`GDALCreateGenImgProjTransformer2`.
    See :cpp:func:`GDALCreateRPCTransformerV2()` for RPC specific options.

.. option:: -vshift

    Force the use of vertical shift. This option is generally not necessary,
    except when using an explicit coordinate transformation (:option:`-ct`),
    and not specifying an explicit source and target SRS.

    .. versionadded:: 3.4

.. option:: -novshift

    Disable the use of vertical shift when one of the source or target SRS has
    an explicit vertical datum, and the input dataset is a single band dataset.

    .. note:: this option was named ``-novshiftgrid`` until GDAL 3.3.

    .. versionadded:: 3.4

.. option:: -order <n>

    order of polynomial used for warping (1 to 3). The default is to select
    a polynomial order based on the number of GCPs.

.. option:: -tps

    Force use of Thin Plate Spline transformer based on available GCPs.

    .. warning::

        Using a Thin Plate Spline transformer with more than a few
        thousand GCPs requires significant RAM usage (at least ``numGCPs`` * ``numGCPs`` * 8 bytes)
        and processing time.


.. option:: -rpc

    Force use of RPCs.

.. option:: -geoloc

    Force use of Geolocation Arrays.

.. option:: -et <err_threshold>

    Error threshold for transformation approximation, expressed as a number of
    source pixels. Defaults to 0.125 pixels unless the ``RPC_DEM`` transformer
    option is specified, in which case an exact transformer, i.e.
    ``err_threshold=0``, will be used.

.. option:: -refine_gcps <tolerance> [<minimum_gcps>]

    Refines the GCPs by automatically eliminating outliers.
    Outliers will be eliminated until minimum_gcps are left or when no outliers can be detected.
    The tolerance is passed to adjust when a GCP will be eliminated.
    Note that GCP refinement only works with polynomial interpolation.
    The tolerance is in pixel units if no projection is available, otherwise it is in SRS units.
    If minimum_gcps is not provided, the minimum GCPs according to the polynomial model is used.

.. option:: -te <xmin> <ymin> <xmax> <ymax>

    Set georeferenced extents of output file to be created (in target SRS by
    default, or in the SRS specified with :option:`-te_srs`)

.. option:: -te_srs <srs_def>

    Specifies the SRS in
    which to interpret the coordinates given with -te. The <srs_def> may
    be any of the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file
    containing the WKT.
    This must not be confused with -t_srs which is the target SRS of the output
    dataset. :option:`-te_srs` is a convenience e.g. when knowing the output coordinates in a
    geodetic long/lat SRS, but still wanting a result in a projected coordinate system.

.. option:: -tr <xres> <yres> | -tr square

    Set output file resolution (in target georeferenced units).

    If not specified (or not deduced from -te and -ts), gdalwarp will, in the
    general case, generate an output raster with xres=yres.

    Starting with GDAL 3.7, if neither :option:`-tr` nor :option:`-ts` are specified,
    that no reprojection is involved (including taking into account geolocation arrays
    or RPC), the resolution of the source file(s) will be preserved (in previous
    version, an output raster with xres=yres was always generated).
    It is possible to ask square pixels to still be generated, by specifying
    ``square`` as the value for :option:`-tr`.

.. option:: -tap

    (target aligned pixels) align the coordinates of the extent of the output
    file to the values of the :option:`-tr`, such that the aligned extent
    includes the minimum extent (edges lines/columns that are detected as
    blank, before actual warping, will be removed starting with GDAL 3.8).
    Alignment means that xmin / resx, ymin / resy,
    xmax / resx and ymax / resy are integer values. It does not necessarily
    mean that the output grid aligns with the input grid.

.. option:: -ts <width> <height>

    Set output file size in pixels and lines. If width or height is set to 0,
    the other dimension will be guessed from the computed resolution. Note that
    :option:`-ts` cannot be used with :option:`-tr`

.. option:: -ovr <level>|AUTO|AUTO-<n>|NONE

    To specify which overview level of source files must be used. The default choice,
    AUTO, will select the overview level whose resolution is the closest to the
    target resolution. Specify an integer value (0-based, i.e. 0=1st overview level)
    to select a particular level. Specify AUTO-n where n is an integer greater or
    equal to 1, to select an overview level below the AUTO one. Or specify NONE to
    force the base resolution to be used (can be useful if overviews have been
    generated with a low quality resampling method, and the warping is done using a
    higher quality resampling method).

.. option:: -wo <NAME>=<VALUE>

    Set a warp option.  The :cpp:member:`GDALWarpOptions::papszWarpOptions` docs show all options.
    Multiple :option:`-wo` options may be listed.

.. include:: options/ot.rst

.. option:: -wt <type>

    Working pixel data type. The data type of pixels in the source image and
    destination image buffers.

.. option:: -r <resampling_method>

    Resampling method to use. Available methods are:

    ``near``: nearest neighbour resampling (default, fastest algorithm, worst interpolation quality).

    ``bilinear``: bilinear resampling.

    ``cubic``: cubic resampling.

    ``cubicspline``: cubic spline resampling.

    ``lanczos``: Lanczos windowed sinc resampling.

    ``average``: average resampling, computes the weighted average of all non-NODATA contributing pixels.

    ``rms`` root mean square / quadratic mean of all non-NODATA contributing pixels (GDAL >= 3.3)

    ``mode``: mode resampling, selects the value which appears most often of all the sampled points. In the case of ties, the first value identified as the mode will be selected.

    ``max``: maximum resampling, selects the maximum value from all non-NODATA contributing pixels.

    ``min``: minimum resampling, selects the minimum value from all non-NODATA contributing pixels.

    ``med``: median resampling, selects the median value of all non-NODATA contributing pixels.

    ``q1``: first quartile resampling, selects the first quartile value of all non-NODATA contributing pixels.

    ``q3``: third quartile resampling, selects the third quartile value of all non-NODATA contributing pixels.

    ``sum``: compute the weighted sum of all non-NODATA contributing pixels (since GDAL 3.1)

    .. note::

        When downsampling is performed (use of :option:`-tr` or :option:`-ts`), existing
        overviews (either internal/implicit or external ones) on the source image
        will be used by default by selecting the closest overview to the desired output
        resolution.
        The resampling method used to create those overviews is generally not the one you
        specify through the :option:`-r` option. Some formats, like JPEG2000, can contain
        significant outliers due to how wavelet compression works. It might thus be useful in
        those situations to use the :option:`-ovr` ``NONE`` option to prevent existing overviews to
        be used.

.. option:: -srcnodata "<value>[ <value>]..."

    Set nodata masking values for input bands (different values can be supplied
    for each band). If more than one value is supplied all values should be quoted
    to keep them together as a single operating system argument.
    Masked values will not be used in interpolation (details given in :ref:`gdalwarp_nodata`)

    Use a value of ``None`` to ignore intrinsic nodata settings on the source dataset.

    When this option is set to a non-``None`` value, it causes the ``UNIFIED_SRC_NODATA``
    warping option (see :cpp:member:`GDALWarpOptions::papszWarpOptions`) to be
    set to ``YES``, if it is not explicitly set.

    If ``-srcnodata`` is not explicitly set, but the source dataset has nodata values,
    they will be taken into account, with ``UNIFIED_SRC_NODATA`` at ``PARTIAL``
    by default.

.. option:: -dstnodata "<value>[ <value>]..."

    Set nodata values
    for output bands (different values can be supplied for each band).  If more
    than one value is supplied all values should be quoted to keep them together
    as a single operating system argument.  New files will be initialized to this
    value and if possible the nodata value will be recorded in the output
    file. Use a value of ``None`` to ensure that nodata is not defined.
    If this argument is not used then nodata values will be copied from the source dataset.

.. option:: -srcalpha

    Force the last band of a source image to be
    considered as a source alpha band.

.. option:: -nosrcalpha

    Prevent the alpha band of a source image to be
    considered as such (it will be warped as a regular band)

.. option:: -dstalpha

    Create an output alpha band to identify nodata (unset/transparent) pixels.

.. option:: -wm <memory_in_mb>

    Set the amount of memory that the
    warp API is allowed to use for caching.
    Defaults to 64 MB.
    Since GDAL 3.10, the value can be specified either as a fixed amount of
    memory (e.g., ``-wm 200MB``, ``-wm 1G``) or as a percentage of usable
    RAM (``-wm 10%``).
    In earlier versions, or if a unit is not specified, the value is interpreted as being
    in megabytes if the value is less than 10000. For values >=10000, it is
    interpreted as bytes.

    The warper will total up the memory required to hold the input and output
    image arrays and any auxiliary masking arrays and if they are larger than
    the "warp memory" allowed it will subdivide the chunk into smaller chunks
    and try again.

    If the -wm value is very small there is some extra overhead in doing many
    small chunks so setting it larger is better but it is a matter of
    diminishing returns.

.. option:: -multi

    Use multithreaded warping implementation.
    Two threads will be used to process chunks of image and perform
    input/output operation simultaneously. Note that computation is not
    multithreaded itself. To do that, you can use the :option:`-wo` NUM_THREADS=val/ALL_CPUS
    option, which can be combined with :option:`-multi`

.. option:: -q

    Be quiet.

.. include:: options/if.rst

.. include:: options/of.rst

.. include:: options/co.rst

.. option:: -cutline <datasource>|<WKT>

    Enable use of a blend cutline from the name of a vector dataset.
    Starting with GDAL 3.9, a WKT geometry string starting with POLYGON or
    MULTIPOLYGON can also be specified.

.. option:: -cutline_srs <srs_def>

    .. versionadded:: 3.9

    Sets or overrides the SRS of the cutline.

.. option:: -cl <layername>

    Select the named layer from the cutline datasource.

.. option:: -cwhere <expression>

    Restrict desired cutline features based on attribute query.

.. option:: -csql <query>

    Select cutline features using an SQL query instead of from a layer with :option:`-cl`.

.. option:: -cblend <distance>

    Set a blend distance to use to blend over cutlines (in pixels).

.. option:: -crop_to_cutline

    Crop the extent of the target dataset to the extent of the cutline.

.. option:: -overwrite

    Overwrite the target dataset if it already exists. Overwriting must be understood
    here as deleting and recreating the file from scratch. Note that if this option
    is *not* specified and the output file already exists, it will be updated in
    place.

.. option:: -nomd

    Do not copy metadata. Without this option, dataset and band metadata
    (as well as some band information) will be copied from the first source dataset.
    Items that differ between source datasets will be set to * (see :option:`-cvmd` option).

.. option:: -cvmd <meta_conflict_value>

    Value to set metadata items that conflict between source datasets
    (default is "*"). Use "" to remove conflicting items.

.. option:: -setci

    Set the color interpretation of the bands of the target dataset from
    the source dataset.

.. option:: -oo <NAME>=<VALUE>

    Dataset open option (format specific)

.. option:: -doo <NAME>=<VALUE>

    Output dataset open option (format specific)

.. option:: <src_dataset_name>

    The source file name(s).

.. option:: <dst_dataset_name>

    The destination file name.


Overview
--------

:program:`gdalwarp` transforms images between different coordinate reference
systems and spatial resolutions.

First, :program:`gdalwarp` must determine the extent and resolution of the
output, if these have not been specified using :option:`-te` and :option:`-tr`.
These are determined by transforming a sample of points from the source CRS to
the destination CRS. Details of the procedure can be found in the documentation
for :cpp:func:`GDALSuggestedWarpOutput`. If multiple inputs are provided to
:program:`gdalwarp`, the output extent will be calculated to cover all of them,
at a resolution consistent with the highest-resolution input.

Once the dimensions of the output image have been determined,
:program:`gdalwarp` divides the output image into chunks that can be processed
independently within the amount of memory specified by :option:`-wm`.
:program:`gdalwarp` then iterates over scanlines in these chunks, and for each
output pixel determines a rectangular region of source pixels that contribute
to the value of the output pixel. The dimensions of this rectangular region
are typically determined by estimating the relative scales of the source and
destination raster, but can be manually specified (see documentation of the
``XSCALE`` parameter in :cpp:member:`GDALWarpOptions::papszWarpOptions`).
Because the source region is a simple rectangle, it is not possible for an
output pixel to be associated with source pixels from both sides of the
antimeridian or pole (when transforming from geographic coordinates).

The rectangular region of source pixels is then provided to a function that
performs the resampling algorithm selected with :option:`-r`.  Depending on the
resampling algorithm and relative scales of the source and destination rasters,
source pixels may be weighted either according to the approximate fraction of
the source pixel that is covered by the destination pixel (e.g., "mean" and
"sum" resampling), or by horizontal and vertical Cartesian distances between
the center of the source pixel and the center of the target pixel (e.g.,
bilinear or cubic spline resampling). In the latter case, the relative weight
of an individual source pixel is determined by the product of the weights
determined for its row and column; the diagonal Cartesian distance is not
calculated.

Multiple input files
--------------------

When multiple inputs are provided to :program:`gdalwarp`, they are processed
independently in the order they are listed. This may introduce edge effects
near the boundaries of the input files, because output pixel values will be
derived from the final input only. To avoid this, non-overlapping input
files may first be combined into a :ref:`VRT file <raster.vrt>` (e.g., using
:ref:`gdalbuildvrt`). This will allow :program:`gdalwarp` to use pixels
from all inputs when calculating output pixel values.


Writing to an existing file
---------------------------

Mosaicing into an existing output file is supported if the output file already
exists. The spatial extent of the existing file will not be modified to
accommodate new data, so you may have to remove it in that case, or use the
:option:`-overwrite` option.

Polygon cutlines may be used as a mask to restrict the area of the destination
file that may be updated, including blending.  If the OGR layer containing the
cutline features has no explicit SRS, the cutline features are assumed to be in
the SRS of the destination file. When writing to a not yet existing target
dataset, its extent will be the one of the original raster unless :option:`-te`
or :option:`-crop_to_cutline` are specified.


.. _gdalwarp_nodata:

Nodata / source validity mask handling
--------------------------------------

Invalid values in source pixels, either identified through a nodata value
metadata set on the source band, a mask band, an alpha band or the use of
:option:`-srcnodata` will not be used in interpolation.
The details of how it is taken into account depends on the resampling kernel:

- for nearest resampling, for each target pixel, the coordinate of its center
  is projected back to source coordinates and the source pixel containing that
  coordinate is identified. If this source pixel is invalid, the target pixel
  is considered as nodata.

- for bilinear, cubic, cubicspline and lanczos, for each target pixel, the
  coordinate of its center is projected back to source coordinates and a
  corresponding source pixel is identified. If this source pixel is invalid, the
  target pixel is considered as nodata (in this case, valid pixels within the
  kernel radius would not be considered).
  Given that those resampling kernels have a non-null kernel radius, this source
  pixel is just one among other several source pixels, and it might be possible
  that there are invalid values in those other contributing source pixels.
  The weights used to take into account those invalid values will be set to zero
  to ignore them.

- for the other resampling methods, source pixels contributing to the target pixel
  are ignored if invalid. Only the valid ones are taken into account. If there are
  none, the target pixel is considered as nodata.

If using :option:`-srcnodata` for multiple images with different invalid
values, you need to either (a) pre-process them to have the same to-be-ignored
value, or (b) set the nodata flag for each file. Use (b) if you need to preserve
the original values for some reason, for example:

.. code-block:: bash

   # for this image we want to ignore black (0)
   gdalwarp -srcnodata 0 -dstnodata 0 orig-ignore-black.tif black-nodata.tif

   # and now we want to ignore white (0)
   gdalwarp -srcnodata 255 -dstnodata 255 orig-ignore-white.tif white-nodata.tif

   # and finally ignore a particular blue-grey (RGB 125 125 150)
   gdalwarp -srcnodata "125 125 150" -dstnodata "125 125 150" orig-ignore-grey.tif grey-nodata.tif

   # now we can mosaic them all and not worry about nodata parameters
   gdalwarp black-nodata.tif grey-nodata.tif white-nodata.tif final-mosaic.tif


Approximate transformation
--------------------------

By default :program:`gdalwarp` uses a linear approximator for the
transformations with a permitted error of 0.125 pixels in the source dataset.
For each processing chunk, the approximator precisely transforms three points
per output scanline (the start, middle, and end) from a row and column in the
output dataset to a row and column in the source dataset.
It then compares a linear approximation of the center point coordinates to the
precisely transformed value.
If the sum of the horizontal and vertical errors is less than the error
threshold then the remaining source points are approximated using linear
interpolation between the start and middle point, and between the middle and
end point.
If the error exceeds the threshold, the scanline is split into two sections and
the approximator is recursively applied to each section until the error is less
than the threshold or all points have been exactly computed.

Note that a processing chunk does not necessarily correspond to a whole destination
scanline. If the output dataset is tiled, its shape will typically be one or
several tiles. The ``OPTIMIZE_SIZE`` warping option and the value of the :option:`-wm`
option can also influence the shape of the processing chunk. Consequently, for
a single (non-zero) value of the error threshold,
the linear approximation might result in slightly different values in coordinate
transformation, but all of them within the permitted error.

The error threshold (in source dataset pixels) can be controlled with the gdalwarp
:option:`-et` switch. If you want to compare a true pixel-by-pixel reprojection
use :option:`-et 0` which disables this approximator entirely.

Vertical transformation
-----------------------

While gdalwarp is most commonly used to perform coordinate transformations in the 2D
space, it can also perform vertical transformations. Vertical transformations are
automatically performed when the following two conditions are met:

- at least one of the source or target CRS has an explicit vertical CRS
  (as part of a compound CRS) or is a 3D (generally geographic) CRS, and
- the raster has a single band

This mode can also be forced by using the :option:`-vshift` (this is
essentially useful when the CRS involved are not explicitly 3D, but a
transformation pipeline is specified with :option:`-ct`), or disabled with
:option:`-novshift`.

When a vertical transformation is involved, typically a shift value read in a
geoid grid will be applied. This may require such grid(s) to be installed, or
PROJ networking capabilities to be enabled. Consult `PROJ <https://proj.org>`__
documentation for more details. In addition to a shift, the raster values may
be multiplied by a factor to take into account vertical unit changes.
In priority, the value returned by :cpp:func:`GDALRasterBand::GetUnitType` is
used. The following values are currently recognized: ``m``, ``metre``, ``metre``,
``ft``, ``foot``, ``US survey foot``. If there is no defined unit type at the
band level, the vertical unit of the source CRS is used. The vertical unit of
the target CRS is also used to determine that conversion factor. The conversion
factor may be overridden by setting the ``MULT_FACTOR_VERTICAL_SHIFT`` warping
option with :option:`-wo`. For example ``-wo MULT_FACTOR_VERTICAL_SHIFT=1`` to
disable any vertical unit change.

Memory usage
------------

Adding RAM will almost certainly increase the speed of :program:`gdalwarp`.
That's not at all the same as saying that it is worth it, or that the speed
increase will be significant. Disks are the slowest part of the process.  By
default :program:`gdalwarp` won't take much advantage of RAM. Using the flag
:option:`-wm 500` will operate on 500MB chunks at a time which is better than
the default. The warp memory specified by :option:`-wm` is shared among all
threads, so it is especially beneficial to increase this value when running
:program:`gdalwarp` with :option:`-wo NUM_THREADS` (or its equivalent
:config:`GDAL_NUM_THREADS`) greater than 1.

Increasing the I/O block cache size may also help. This can be done by
setting the :config:`GDAL_CACHEMAX` configuration like:

.. code-block:: bash

   gdalwarp --config GDAL_CACHEMAX 500 -wm 500 ...

This uses 500MB of RAM for read/write caching, and 500MB of RAM for working
buffers during the warp. Beyond that it is doubtful more memory will make a
substantial difference.

Check CPU usage while :program:`gdalwarp` is running. If it is substantially
less than 100% then you know things are IO bound. Otherwise they are CPU bound.
The ``--debug`` option may also provide useful information. For instance, after
running the following:

.. code-block:: bash

   gdalwarp --debug on abc.tif def.tif

a message like the following will be output:

.. code-block::

  GDAL: 224 block reads on 32 block band 1 of utm.tif

In this case it is saying that band 1 of :file:`utm.tif` has 32 blocks, but
that 224 block reads were done, implying that lots of data was having to be
re-read, presumably because of a limited IO cache. You will also see messages
like:

.. code-block::

   GDAL: GDALWarpKernel()::GWKNearestNoMasksByte()
   Src=0,0,512x512 Dst=0,0,512x512

The Src/Dst windows show you the "chunk size" being used. In this case my whole
image which is very small. If you find things are being broken into a lot of
chunks increasing :option:`-wm` may help somewhat.

But far more important than memory are ensuring you are going through an
optimized path in the warper. If you ever see it reporting
``GDALWarpKernel()::GWKGeneralCase()`` you know things will be relatively slow.
Basically, the fastest situations are nearest neighbour resampling on 8bit data
without nodata or alpha masking in effect.


Compressed output
-----------------

In some cases, the output of :program:`gdalwarp` may be much larger than the
original, even if the same compression algorithm is used. By default,
:program:`gdalwarp` operates on chunks that are not necessarily aligned with
the boundaries of the blocks/tiles/strips of the output format, so this might
cause repeated compression/decompression of partial blocks, leading to lost
space in the output format.

The situation can be improved by using the ``OPTIMIZE_SIZE`` warping option
(:option:`-wo OPTIMIZE_SIZE=YES`), but note that depending on the source and
target projections, it might also significantly slow down the warping process.

Another possibility is to use :program:`gdalwarp` without compression and then
follow up with :program:`gdal_translate` with compression:

.. code-block:: bash

   gdalwarp infile tempfile.tif ...options...
   gdal_translate tempfile.tif outfile.tif -co compress=lzw ...etc.

Alternatively, you can use a VRT file as the output format of :program:`gdalwarp`. The
VRT file is just an XML file that will be created immediately. The
:program:`gdal_translate` operations will be of course a bit slower as it will do the
real warping operation.

.. code-block:: bash

   gdalwarp -of VRT infile tempfile.vrt ...options...
   gdal_translate tempfile.vrt outfile.tif -co compress=lzw ...etc.


Frequently Asked Questions
--------------------------

.. rubric:: Q1. Why does the quality of the output looks so bad (no anti-aliasing)?

Did you specify a resampling method, with :option:`-r`, other than the
default nearest neighbour?


.. rubric:: Q2. Why do I get slightly different results whether the output dataset is tiled or not?

This is related to the fact that an approximate coordinate transformation is
used by default to speed-up computation. If you want to get the same results
whether the output is tiled or not, set :option:`-et` to zero.
Note, however, that this will only work for relatively small images; other factors
can still result in different result. See following question (Q3).


.. rubric:: Q3. Why do I observe artifacts, that look like resolution changes and are aligned
   with rectangular areas of the output raster, when warping sufficiently large
   rasters, particularly in areas where the reprojection involves significant
   deformation and only with non-nearest resampling ?

The warping engine operates on rectangular areas of the output
dataset (generally aligned with tile boundaries for a compressed tile dataset).

During reprojection, a single source pixel does not generally correspond to a
single output pixel. The resampling method must therefore properly account for this
and compute a ratio between the number of source and target pixels in the
horizontal (X) and vertical (Y) directions. These ratios are computed per warping
chunk. This maximizes the local quality of the warping but has the downside of
creating visual discontinuities between warping chunks.

If you favor a seamless result, you may manually specify the
XSCALE and YSCALE warping options with :option:`-wo`.
The XSCALE (resp. YSCALE) value is the ratio expressing the resampling factor,
i.e. the number of destination pixels per source pixel, along the
horizontal (resp. vertical) axis. It equals to one for no resampling, is
below one for downsampling, and above one for upsampling.


Examples
--------

- Basic transformation:

.. code-block:: bash

  gdalwarp -t_srs EPSG:4326 input.tif output.tif


- For instance, an eight bit spot scene stored in GeoTIFF with
  control points mapping the corners to lat/long could be warped to a UTM
  projection with a command like this:

.. code-block:: bash

    gdalwarp -t_srs '+proj=utm +zone=11 +datum=WGS84' -overwrite raw_spot.tif utm11.tif

- For instance, the second channel of an ASTER image stored in HDF with
  control points mapping the corners to lat/long could be warped to a UTM
  projection with a command like this:

.. code-block:: bash

    gdalwarp -overwrite HDF4_SDS:ASTER_L1B:"pg-PR1B0000-2002031402_100_001":2 \
        pg-PR1B0000-2002031402_100_001_2.tif

- To apply a cutline on a un-georeferenced image and clip from pixel (220,60) to pixel (1160,690):

.. code-block:: bash

    gdalwarp -overwrite -to SRC_METHOD=NO_GEOTRANSFORM -to DST_METHOD=NO_GEOTRANSFORM \
        -te 220 60 1160 690 -cutline cutline.csv in.png out.tif

where cutline.csv content is like:

.. code-block::

    id,WKT
    1,"POLYGON((....))"

- To transform a DEM from geoid elevations (using EGM96) to WGS84 ellipsoidal heights:

.. code-block:: bash

    gdalwarp -overwrite in_dem.tif out_dem.tif -s_srs EPSG:4326+5773 -t_srs EPSG:4979


C API
-----

This utility is also callable from C with :cpp:func:`GDALWarp`.
