.. _gdal_raster_reproject_subcommand:

================================================================================
"gdal raster reproject" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Reproject a raster dataset.

.. Index:: gdal raster reproject

Synopsis
--------

.. code-block::

    Usage: gdal raster reproject [OPTIONS] <INPUT> <OUTPUT>

    Reproject a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]
      -o, --output <OUTPUT>                                Output raster dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --version                                            Display GDAL version and exit
      --json-usage                                         Display usage as JSON document and exit
      --drivers                                            Display driver list as JSON document and exit
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      -s, --src-crs <SRC-CRS>                              Source CRS
      -d, --dst-crs <DST-CRS>                              Destination CRS
      -r, --resampling <RESAMPLING>                        Resampling method. RESAMPLING=nearest|bilinear|cubic|cubicspline|lanczos|average|rms|mode|min|max|med|q1|q3|sum (default: nearest)
      --resolution <xres>,<yres>                           Target resolution (in destination CRS units)
                                                           Mutually exclusive with --size
      --size <width>,<height>                              Target size in pixels
                                                           Mutually exclusive with --resolution
      --bbox <xmin>,<ymin>,<xmax>,<ymax>                   Target bounding box (in destination CRS units)
      --bbox-crs <BBOX-CRS>                                CRS of target bounding box
      --target-aligned-pixels                              Round target extent to target resolution

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]


Description
-----------

:program:`gdal raster reproject` can be used to reproject a raster dataset.
The program can reproject to any supported projection.

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: -s, --src-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

.. option:: -d, --dst-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: options/srs_def_gdalwarp.rst

.. option:: -r, --resampling <RESAMPLING>

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

        When downsampling is performed (use of :option:`--resolution` or :option:`--size`), existing
        overviews (either internal/implicit or external ones) on the source image
        will be used by default by selecting the closest overview to the desired output
        resolution.
        The resampling method used to create those overviews is generally not the one you
        specify through the :option:`-r` option.

.. option:: --resolution <xres>,<yres>

    Set output file resolution (in target georeferenced units).

    If not specified (or not deduced from -te and -ts), gdalwarp will, in the
    general case, generate an output raster with xres=yres.

    If neither :option:`--resolution` nor :option:`--size` are specified,
    that no reprojection is involved (including taking into account geolocation arrays
    or RPC), the resolution of the source file(s) will be preserved (in previous
    version, an output raster with xres=yres was always generated).

.. option:: --size <width>,<height>

    Set output file size in pixels and lines. If width or height is set to 0,
    the other dimension will be guessed from the computed resolution. Note that
    :option:`--size` cannot be used with :option:`--resolution`

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Set georeferenced extents of output file to be created (in target SRS by
    default, or in the SRS specified with :option:`--bbox-crs`)

.. option:: --bbox-crs <BBOX-CRS>

    Specifies the SRS in which to interpret the coordinates given with :option:`--bbox`.

    .. include:: options/srs_def_gdalwarp.rst

    This must not be confused with :option:`--dst-crs` which is the target SRS of the output
    dataset. :option:`--bbox-crs` is a convenience e.g. when knowing the output coordinates in a
    geodetic long/lat SRS, but still wanting a result in a projected coordinate system.

.. option:: --target-aligned-pixels

    Align the coordinates of the extent of the output
    file to the values of the :option:`--resolution`, such that the aligned extent
    includes the minimum extent (edges lines/columns that are detected as
    blank, before actual warping, will be removed).
    Alignment means that xmin / resx, ymin / resy,
    xmax / resx and ymax / resy are integer values.

Examples
--------

.. example::
   :title: Reproject a GeoTIFF file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N")

   .. code-block:: bash

        $ gdal raster reproject --dst-crs=EPSG:32632 in.tif out.tif --overwrite
