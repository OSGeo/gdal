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

    gdalwarp [--help-general] [--formats]
        [-s_srs srs_def] [-t_srs srs_def] [-ct string] [-to "NAME=VALUE"]* [-novshiftgrid]
        [[-s_coord_epoch epoch] | [-t_coord_epoch epoch]]
        [-order n | -tps | -rpc | -geoloc] [-et err_threshold]
        [-refine_gcps tolerance [minimum_gcps]]
        [-te xmin ymin xmax ymax] [-te_srs srs_def]
        [-tr xres yres] [-tap] [-ts width height]
        [-ovr level|AUTO|AUTO-n|NONE] [-wo "NAME=VALUE"] [-ot Byte/Int16/...] [-wt Byte/Int16]
        [-srcnodata "value [value...]"] [-dstnodata "value [value...]"]
        [-srcalpha|-nosrcalpha] [-dstalpha]
        [-r resampling_method] [-wm memory_in_mb] [-multi] [-q]
        [-cutline datasource] [-cl layer] [-cwhere expression]
        [-csql statement] [-cblend dist_in_pixels] [-crop_to_cutline]
        [-if format]* [-of format] [-co "NAME=VALUE"]* [-overwrite]
        [-nomd] [-cvmd meta_conflict_value] [-setci] [-oo NAME=VALUE]*
        [-doo NAME=VALUE]*
        srcfile* dstfile

Description
-----------

The :program:`gdalwarp` utility is an image mosaicing, reprojection and warping
utility. The program can reproject to any supported projection,
and can also apply GCPs stored with the image if the image is "raw"
with control information.

.. program:: gdalwarp

.. option:: -s_srs <srs def>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

.. option:: -s_coord_epoch <epoch>

    .. versionadded:: 3.4

    Assign a coordinate epoch, linked with the source SRS. Useful when the
    source SRS is a dynamic CRS. Only taken into account if :option:`-s_srs`
    is used.

    Currently :option:`-s_coord_epoch` and :option:`-t_coord_epoch` are
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

    Currently :option:`-s_coord_epoch` and :option:`-t_coord_epoch` are
    mutually exclusive, due to lack of support for transformations between two dynamic CRS.

.. option:: -ct <string>

    A PROJ string (single step operation or multiple step string
    starting with +proj=pipeline), a WKT2 string describing a CoordinateOperation,
    or a urn:ogc:def:coordinateOperation:EPSG::XXXX URN overriding the default
    transformation from the source to the target CRS. It must take into account the
    axis order of the source and target CRS.

    .. versionadded:: 3.0

.. option:: -to <NAME=VALUE>

    Set a transformer option suitable to pass to :cpp:func:`GDALCreateGenImgProjTransformer2`.

.. option:: -novshiftgrid

    Disable the use of vertical
    datum shift grids when one of the source or target SRS has an explicit vertical
    datum, and the input dataset is a single band dataset.

    .. versionadded:: 2.2

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
    defaults to 0.125, unless, starting with GDAL 2.1, the RPC_DEM warping
    option is specified, in which case, an exact transformer, i.e.
    err_threshold=0, will be used).

.. option:: -refine_gcps <tolerance minimum_gcps>

    Refines the GCPs by automatically eliminating outliers.
    Outliers will be eliminated until minimum_gcps are left or when no outliers can be detected.
    The tolerance is passed to adjust when a GCP will be eliminated.
    Not that GCP refinement only works with polynomial interpolation.
    The tolerance is in pixel units if no projection is available, otherwise it is in SRS units.
    If minimum_gcps is not provided, the minimum GCPs according to the polynomial model is used.

.. option:: -te <xmin ymin xmax ymax>

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

.. option:: -tr <xres> <yres>

    Set output file resolution (in target georeferenced units).

    If not specified (or not deduced from -te and -ts), gdalwarp will generate
    an output raster with xres=yres, and that even when using gdalwarp in scenarios
    not involving reprojection.

.. option:: -tap

    (target aligned pixels) align the coordinates of the extent of the output
    file to the values of the :option:`-tr`, such that the aligned extent
    includes the minimum extent.

.. option:: -ts <width> <height>

    Set output file size in pixels and lines. If width or height is set to 0,
    the other dimension will be guessed from the computed resolution. Note that
    :option:`-ts` cannot be used with :option:`-tr`

.. option:: -ovr <level|AUTO|AUTO-n|NONE>

    To specify which overview level of source files must be used. The default choice,
    AUTO, will select the overview level whose resolution is the closest to the
    target resolution. Specify an integer value (0-based, i.e. 0=1st overview level)
    to select a particular level. Specify AUTO-n where n is an integer greater or
    equal to 1, to select an overview level below the AUTO one. Or specify NONE to
    force the base resolution to be used (can be useful if overviews have been
    generated with a low quality resampling method, and the warping is done using a
    higher quality resampling method).

.. option:: -wo `"NAME=VALUE"`

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

    ``mode``: mode resampling, selects the value which appears most often of all the sampled points.

    ``max``: maximum resampling, selects the maximum value from all non-NODATA contributing pixels.

    ``min``: minimum resampling, selects the minimum value from all non-NODATA contributing pixels.

    ``med``: median resampling, selects the median value of all non-NODATA contributing pixels.

    ``q1``: first quartile resampling, selects the first quartile value of all non-NODATA contributing pixels.

    ``q3``: third quartile resampling, selects the third quartile value of all non-NODATA contributing pixels.

    ``sum``: compute the weighted sum of all non-NODATA contributing pixels (since GDAL 3.1)

.. option:: -srcnodata <value [value...]>

    Set nodata masking values for input bands (different values can be supplied
    for each band). If more than one value is supplied all values should be quoted
    to keep them together as a single operating system argument.
    Masked values will not be used in interpolation.
    Use a value of ``None`` to ignore intrinsic nodata settings on the source dataset.

    When this option is set to a non-``None`` value, it causes the ``UNIFIED_SRC_NODATA``
    warping option (see :cpp:member:`GDALWarpOptions::papszWarpOptions`) to be
    set to ``YES``, if it is not explicitly set.

    If ``-srcnodata`` is not explicitly set, but the source dataset has nodata values,
    they will be taken into account, with ``UNIFIED_SRC_NODATA`` at ``PARTIAL``
    by default.

.. option:: -dstnodata <value [value...]>

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

.. option:: -cutline <datasource>

    Enable use of a blend cutline from the name OGR support datasource.

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

    Overwrite the target dataset if it already exists.

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

.. option:: -oo <NAME=VALUE>

    Dataset open option (format specific)

.. option:: -doo <NAME=VALUE>

    Output dataset open option (format specific)

    .. versionadded:: 2.1

.. option:: <srcfile>

    The source file name(s).

.. option:: <dstfile>

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

When doing vertical shift adjustments, the transformer option -to ERROR_ON_MISSING_VERT_SHIFT=YES
can be used to error out as soon as a vertical shift value is missing (instead of
0 being used).

Starting with GDAL 3.1, it is possible to use as output format a driver that
only supports the CreateCopy operation. This may internally imply creation of
a temporary file.

Examples
--------

- Basic transformation:

::

  gdalwarp -t_srs EPSG:4326 input.tif output.tif


- For instance, an eight bit spot scene stored in GeoTIFF with
  control points mapping the corners to lat/long could be warped to a UTM
  projection with a command like this:

::

    gdalwarp -t_srs '+proj=utm +zone=11 +datum=WGS84' -overwrite raw_spot.tif utm11.tif

- For instance, the second channel of an ASTER image stored in HDF with
  control points mapping the corners to lat/long could be warped to a UTM
  projection with a command like this:

    .. versionadded:: 2.2

::

    gdalwarp -overwrite HDF4_SDS:ASTER_L1B:"pg-PR1B0000-2002031402_100_001":2 pg-PR1B0000-2002031402_100_001_2.tif

- To apply a cutline on a un-georeferenced image and clip from pixel (220,60) to pixel (1160,690):

::

    gdalwarp -overwrite -to SRC_METHOD=NO_GEOTRANSFORM -to DST_METHOD=NO_GEOTRANSFORM -te 220 60 1160 690 -cutline cutline.csv in.png out.tif

where cutline.csv content is like:

::

    id,WKT
    1,"POLYGON((....))"

- To transform a DEM from geoid elevations (using EGM96) to WGS84 ellipsoidal heights:

    .. versionadded:: 2.2

::

    gdalwarp -overwrite in_dem.tif out_dem.tif -s_srs EPSG:4326+5773 -t_srs EPSG:4979


See also
--------

`Wiki page discussing options and behaviours of gdalwarp <http://trac.osgeo.org/gdal/wiki/UserDocs/GdalWarp>`_
