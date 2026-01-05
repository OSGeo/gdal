.. _gdal_raster_reproject:

================================================================================
``gdal raster reproject``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Reproject a raster dataset.

.. Index:: gdal raster reproject

Synopsis
--------

.. program-output:: gdal raster reproject --help-doc

Description
-----------

:program:`gdal raster reproject` can be used to reproject a raster dataset.
The program can reproject to any supported projection.

First, :program:`gdal raster reproject` must determine the extent and resolution of the
output, if these have not been specified using :option:`--bbox` and :option:`--resolution`.
These are determined by transforming a sample of points from the source CRS to
the destination CRS. Details of the procedure can be found in the documentation
for :cpp:func:`GDALSuggestedWarpOutput`. If multiple inputs are provided to
:program:`gdal raster reproject`, the output extent will be calculated to cover all of them,
at a resolution consistent with the highest-resolution input.

Once the dimensions of the output image have been determined,
:program:`gdal raster reproject` divides the output image into chunks that can be processed
independently.
:program:`gdal raster reproject` then iterates over scanlines in these chunks, and for each
output pixel determines a rectangular region of source pixels that contribute
to the value of the output pixel. The dimensions of this rectangular region
are typically determined by estimating the relative scales of the source and
destination raster, but can be manually specified (see documentation of the
``XSCALE`` parameter in :cpp:member:`GDALWarpOptions::papszWarpOptions`).
Because the source region is a simple rectangle, it is not possible for an
output pixel to be associated with source pixels from both sides of the
antimeridian or pole (when transforming from geographic coordinates).

The rectangular region of source pixels is then provided to a function that
performs the resampling algorithm selected with :option:`--resampling`. Depending on the
resampling algorithm and relative scales of the source and destination rasters,
source pixels may be weighted either according to the approximate fraction of
the source pixel that is covered by the destination pixel (e.g., "mean" and
"sum" resampling), or by horizontal and vertical Cartesian distances between
the center of the source pixel and the center of the target pixel (e.g.,
bilinear or cubic spline resampling). In the latter case, the relative weight
of an individual source pixel is determined by the product of the weights
determined for its row and column; the diagonal Cartesian distance is not
calculated.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

Program-Specific Options
------------------------

.. option:: --add-alpha

    Create an output alpha band to identify nodata (unset/transparent) pixels.
    Value 0 is used for fully transparent pixels. The maximum value for the alpha
    band, for fully opaque pixels, depends on the data type and the presence of
    the NBITS band metadata item. If it is present, the maximum value is 2^NBITS-1.
    Otherwise, if NBITS is not set and the alpha band is of type UInt16
    (resp. Int16), 65535 (resp. 32767) is used. Otherwise, 255 is used. The
    maximum value can also be overridden with ``--wo DST_ALPHA_MAX=<value>``.

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Set georeferenced extents of output file to be created (in target SRS by
    default, or in the SRS specified with :option:`--bbox-crs`)

.. option:: --bbox-crs <BBOX-CRS>

    Specifies the SRS in which to interpret the coordinates given with :option:`--bbox`.

    .. include:: options/srs_def_gdalwarp.rst

    This must not be confused with :option:`--dst-crs` which is the target SRS of the output
    dataset. :option:`--bbox-crs` is a convenience e.g. when knowing the output coordinates in a
    geodetic long/lat SRS, but still wanting a result in a projected coordinate system.

.. option:: -d, --dst-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

.. option:: --dst-nodata <DSTNODATA>

    Set nodata values for output bands (different values can be supplied for each band).
    If more than one value is supplied all values should be quoted to keep them together
    as a single operating system argument.  New files will be initialized to this
    value and if possible the nodata value will be recorded in the output
    file. Use a value of ``None`` to ensure that nodata is not defined.
    If this argument is not used then nodata values will be copied from the source dataset.
    Note that a number of output formats, including GeoTIFF, do not support
    different per-band nodata values, but a single one for all bands.

.. option:: --et, --error-threshold <ERROR-THRESHOLD>

    Error threshold for transformation approximation, expressed as a number of
    source pixels. Defaults to 0.125 pixels unless the ``RPC_DEM`` transformer
    option is specified, in which case an exact transformer, i.e.
    ``--error-threshold=0``, will be used.

.. option:: -j, --num-threads <value>

    .. versionadded:: 3.12

    Number of jobs to run at once.
    Default: number of CPUs detected.

.. include:: gdal_options/warp_resampling.rst

.. option:: --resolution <xres>,<yres>

    Set output file resolution (in target georeferenced units).

    If not specified (or not deduced from :option:`--size`), the program will, in the
    general case, generate an output raster with xres=yres.

    If neither :option:`--resolution` nor :option:`--size` are specified,
    that no reprojection is involved (including taking into account geolocation arrays
    or RPC), the resolution of the source file(s) will be preserved (in previous
    version, an output raster with xres=yres was always generated).

    Mutually exclusive with :option:`--size`.

.. option:: --size <width>,<height>

    Set output file size in pixels and lines. If width or height is set to 0,
    the other dimension will be guessed from the computed resolution.

    Mutually exclusive with :option:`--resolution`.

.. option:: -s, --src-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

.. option:: --src-nodata <SRCNODATA>

    Set nodata masking values for input bands (different values can be supplied
    for each band). If more than one value is supplied all values should be quoted
    to keep them together as a single operating system argument.
    Masked values will not be used in interpolation (details given in :ref:`gdalwarp_nodata`)

    Use a value of ``None`` to ignore intrinsic nodata settings on the source dataset.

    When this option is set to a non-``None`` value, it causes the ``UNIFIED_SRC_NODATA``
    warping option (see :cpp:member:`GDALWarpOptions::papszWarpOptions`) to be
    set to ``YES``, if it is not explicitly set.

    If ``--src-nodata`` is not explicitly set, but the source dataset has nodata values,
    they will be taken into account, with ``UNIFIED_SRC_NODATA`` at ``PARTIAL``
    by default.

.. option:: --target-aligned-pixels

    Align the coordinates of the extent of the output
    file to the values of the :option:`--resolution`, such that the aligned extent
    includes the minimum extent (edges lines/columns that are detected as
    blank, before actual warping, will be removed).
    Alignment means that xmin / resx, ymin / resy,
    xmax / resx and ymax / resy are integer values.

.. option:: --to, --transform-option <NAME>=<VALUE>

    Set a transformer option suitable to pass to :cpp:func:`GDALCreateGenImgProjTransformer2`.
    See :cpp:func:`GDALCreateRPCTransformerV2()` for RPC specific options.

    To match the gdalwarp -rpc option, use --to METHOD=RPC

.. option:: --wo, --warp-option <NAME>=<VALUE>

    Set a warp option.  The :cpp:member:`GDALWarpOptions::papszWarpOptions` docs show all options.
    Multiple options may be listed.


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_raster.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst


Nodata / source validity mask handling
--------------------------------------

Invalid values in source pixels, either identified through a nodata value
metadata set on the source band, a mask band, an alpha band (for an alpha band,
a value of 0 means invalid. Other values are used for blending values) or the use of
:option:`--src-nodata` will not be used in interpolation.
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

Approximate transformation
--------------------------

By default :program:`gdal raster reproject` uses a linear approximator for the
transformations with a permitted error of 0.125 pixels in the source dataset.
The approximator precisely transforms three points per output scanline (the
start, middle, and end) from a row and column in the output dataset to a
row and column in the source dataset.
It then compares a linear approximation of the center point coordinates to the
precisely transformed value.
If the sum of the horizontal and vertical errors is less than the error
threshold then the remaining source points are approximated using linear
interpolation between the start and middle point, and between the middle and
end point.
If the error exceeds the threshold, the scanline is split into two sections and
the approximator is recursively applied to each section until the error is less
than the threshold or all points have been exactly computed.

The error threshold (in source dataset pixels) can be controlled with the
:option:`--error-threshold` switch. If you want to compare a true pixel-by-pixel reprojection
use ``--error-threshold=0`` which disables this approximator entirely.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_raster_compatible.rst

Examples
--------

.. example::
   :title: Reproject a GeoTIFF file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N")

   .. code-block:: bash

        $ gdal raster reproject --dst-crs=EPSG:32632 in.tif out.tif --overwrite
