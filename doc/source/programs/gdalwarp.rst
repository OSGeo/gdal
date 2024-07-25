.. _gdalwarp:

================================================================================
gdalwarp
================================================================================

.. only:: html

    Image reprojection and warping utility

.. Index:: gdalwarp

Synopsis
--------

.. code-block::

   gdalwarp [--help] [--long-usage] [--help-general]
            [--quiet] [-overwrite] [-of <output_format>] [-co <NAME>=<VALUE>]...
            [-s_srs <srs_def>] [-t_srs <srs_def>]
            [[-srcalpha]|[-nosrcalpha]]
            [-dstalpha] [-tr <xres> <yres>|square] [-ts <width> <height>]
            [-te <xmin> <ymin> <max> <ymaX]
            [-te_srs <srs_def>]
            [-r near|bilinear|cubic|cubicspline|lanczos|average|rms|mode|min|max|med|q1|q3|sum]
            [-ot Byte|Int8|[U]Int{16|32|64}|CInt{16|32}|[C]Float{32|64}]
            <src_dataset_name>... <dst_dataset_name>

   Advanced options:
            [-wo <NAME>=<VALUE>]... [-multi]
            [-s_coord_epoch <epoch>] [-t_coord_epoch <epoch>] [-ct <string>]
            [[-tps]|[-rpc]|[-geoloc]]
            [-order <1|2|3>] [-refine_gcps <tolerance> [<minimum_gcps>]]
            [-to <NAME>=<VALUE>]...
            [-et <err_threshold>] [-wm <memory_in_mb>]
            [-srcnodata "<value>[ <value>]..."]
            [-dstnodata "<value>[ <value>]..."] [-tap]
            [-wt Byte|Int8|[U]Int{16|32|64}|CInt{16|32}|[C]Float{32|64}]
            [-cutline <datasource>|<WKT>] [-cutline_srs <srs_def>]
            [-cwhere <expression>]
            [[-cl <layername>]|[-csql <query>]]
            [-cblend <distance>] [-crop_to_cutline]
            [-nomd] [-cvmd <meta_conflict_value>] [-setci]
            [-oo <NAME>=<VALUE>]... [-doo <NAME>=<VALUE>]...
            [-ovr <level>|AUTO|AUTO-<n>|NONE]
            [[-vshift]|[-novshiftgrid]]
            [-if <format>]... [-srcband <band>]... [-dstband <band>]...


Description
-----------

The :program:`gdalwarp` utility is an image mosaicing, reprojection and warping
utility. The program can reproject to any supported projection,
and can also apply GCPs stored with the image if the image is "raw"
with control information.

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

    .. note:: this option was named ``-novshiftgrid`` in GDAL 2.2 to 3.3.

    .. versionadded:: 3.4

.. option:: -order <n>

    order of polynomial used for warping (1 to 3). The default is to select
    a polynomial order based on the number of GCPs.

.. option:: -tps

    Force use of thin plate spline transformer based on available GCPs.

.. option:: -rpc

    Force use of RPCs.

.. option:: -geoloc

    Force use of Geolocation Arrays.

.. option:: -et <err_threshold>

    Error threshold for transformation approximation (in pixel units -
    defaults to 0.125, unless, starting with GDAL 2.1, the RPC_DEM transformer
    option is specified, in which case, an exact transformer, i.e.
    err_threshold=0, will be used).

.. option:: -refine_gcps <tolerance> [<minimum_gcps>]

    Refines the GCPs by automatically eliminating outliers.
    Outliers will be eliminated until minimum_gcps are left or when no outliers can be detected.
    The tolerance is passed to adjust when a GCP will be eliminated.
    Not that GCP refinement only works with polynomial interpolation.
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
    xmax / resx and ymax / resy are integer values.

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
        significant outliers due to wavelet compression works. It might thus be useful in
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

    .. versionadded:: 2.2

.. option:: -dstalpha

    Create an output alpha band to identify nodata (unset/transparent) pixels.

.. option:: -wm <memory_in_mb>

    Set the amount of memory that the
    warp API is allowed to use for caching. The value is interpreted as being
    in megabytes if the value is less than 10000. For values >=10000, this is
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

    .. versionadded:: 2.1

.. option:: <src_dataset_name>

    The source file name(s).

.. option:: <dst_dataset_name>

    The destination file name.


Mosaicing into an existing output file is supported if the output file
already exists. The spatial extent of the existing file will not
be modified to accommodate new data, so you may have to remove it in that case, or
use the -overwrite option.

Polygon cutlines may be used as a mask to restrict the area of the
destination file that may be updated, including blending.  If the OGR
layer containing the cutline features has no explicit SRS, the cutline
features must be in the SRS of the destination file. When writing to a
not yet existing target dataset, its extent will be the one of the
original raster unless -te or -crop_to_cutline are specified.

Starting with GDAL 3.1, it is possible to use as output format a driver that
only supports the CreateCopy operation. This may internally imply creation of
a temporary file.

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
  target pixel is considered as nodata.
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
transformations with a permitted error of 0.125 pixels. The approximator
basically transforms three points on a scanline: the start, end and middle.
Then it compares the linear approximation of the center based on the end points
to the real thing and checks the error. If the error is less than the error
threshold then the remaining points are approximated (in two chunks utilizing
the center point). If the error exceeds the threshold, the scanline is split
into two sections, and the approximator is recursively applied to each section
until the error is less than the threshold or all points have been exactly
computed.

The error threshold (in pixels) can be controlled with the gdalwarp
:option:`-et` switch. If you want to compare a true pixel-by-pixel reprojection
use :option:`-et 0` which disables this approximator entirely.

Vertical transformation
-----------------------

While gdalwarp can essentially perform coordinate transformations in the 2D
space, it can perform as well vertical transformations. This is automatically
enabled when the 2 following conditions are met:

- at least one of the source or target CRS has an explicit vertical CRS
  (as part of a compound CRS) or is a 3D (generally geographic) CRS,
- and the raster has a single band.

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

    .. versionadded:: 2.2

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

    .. versionadded:: 2.2

.. code-block:: bash

    gdalwarp -overwrite in_dem.tif out_dem.tif -s_srs EPSG:4326+5773 -t_srs EPSG:4979


C API
-----

This utility is also callable from C with :cpp:func:`GDALWarp`.


See also
--------

.. only:: not man

    `Wiki page discussing options and behaviours of gdalwarp <https://trac.osgeo.org/gdal/wiki/UserDocs/GdalWarp>`_

.. only:: man

    Wiki page discussing options and behaviours of gdalwarp: https://trac.osgeo.org/gdal/wiki/UserDocs/GdalWarp
