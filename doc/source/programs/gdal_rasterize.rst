.. _gdal_rasterize:

================================================================================
gdal_rasterize
================================================================================

.. only:: html

    Burns vector geometries into a raster

.. Index:: gdal_rasterize

Synopsis
--------

.. program-output:: gdal_rasterize --help-doc

Description
-----------

This program burns vector geometries (points, lines, and polygons) into the
raster band(s) of a raster image.  Vectors are read from OGR supported vector
formats. If the output raster already exists, the affected pixels are updated
in-place.

On the fly reprojection of vector data to the coordinate system of the
raster data is supported.

.. tip:: Equivalent in new "gdal" command line interface:

    See :ref:`gdal_vector_rasterize`.


.. program:: gdal_rasterize

.. include:: options/help_and_help_general.rst

.. option:: -b <band>

    The band(s) to burn values into.  Multiple -b arguments may be used to burn
    into a list of bands.  The default is to burn into band 1.  Not used when
    creating a new raster.

.. option:: -i

    Invert rasterization.  Burn the fixed burn value, or the burn value associated
    with the first feature into all parts of the image *not* inside the
    provided polygon.

    .. note::

        When the vector features contain a polygon nested within another polygon
        (like an island in a lake), GDAL must be built against GEOS to get
        correct results.

.. option:: -at

    Enables the ALL_TOUCHED rasterization option so that all pixels touched
    by lines or polygons will be updated, not just those on the line render path,
    or whose center point is within the polygon (behavior is unspecified when the
    polygon is just touching the pixel center). Defaults to disabled for normal
    rendering rules.

    .. note::

        When this option is enabled, the order of the input features (lines or polygons)
        can affect the results. When two features touch each other, the last one (i.e. topmost)
        will determine the burned pixel value at the edge.
        You may wish to use the :option:`-sql` option to reorder the features (ORDER BY)
        to achieve a more predictable result.

.. option:: -burn <value>

    A fixed value to burn into a band for all objects.  A list of :option:`-burn` options
    can be supplied, one per band being written to.

.. option:: -a <attribute_name>

    Identifies an attribute field on the features to be used for a burn-in value.
    The value will be burned into all output bands.

.. option:: -3d

    Indicates that a burn value should be extracted from the "Z" values of the
    feature. Works with points and lines (linear interpolation along each segment).
    For polygons, works properly only if they are flat (same Z value for all
    vertices).

.. option:: -add

    Instead of burning a new value, this adds the new value to the existing raster.
    Suitable for heatmaps for instance.

.. option:: -l <layername>

    Indicates the layer(s) from the datasource that will be used for input
    features.  May be specified multiple times, but at least one layer name or a
    :option:`-sql` option must be specified (not both).

.. option:: -where <expression>

    An optional SQL WHERE style query expression to be applied to select features
    to burn in from the input layer(s).

.. option:: -sql <select_statement>

    An SQL statement to be evaluated against the datasource to produce a
    virtual layer of features to be burned in.
    Starting with GDAL 3.7, the ``@filename`` syntax can be used to indicate
    that the content is in the pointed filename.

    .. note::

        This option will be ignored if the :option:`-l` option has been set as well.

.. option:: -dialect <dialect>

    SQL dialect. In some cases can be used to use (unoptimized) OGR SQL instead of
    the native SQL of an RDBMS by passing OGRSQL. The
    "SQLITE" dialect can also be used with any datasource.

.. include:: options/of.rst

.. option:: -a_nodata <value>

    Assign a specified nodata value to output bands.

.. option:: -init <value>

    Pre-initialize the output image bands with these values.  However, it is not
    marked as the nodata value in the output file.  If only one value is given, the
    same value is used in all the bands.

.. option:: -a_srs <srs_def>

    Override the projection for the output file. If not specified, the projection of
    the input vector file will be used if available. When using this option, no reprojection
    of features from the SRS of the input vector to the specified SRS of the output raster,
    so use only this option to correct an invalid source SRS.
    The <srs_def> may be any of the usual GDAL/OGR forms, complete WKT, PROJ.4,
    EPSG:n or a file containing the WKT.

.. option:: -to <NAME>=<VALUE>

    set a transformer
    option suitable to pass to :cpp:func:`GDALCreateGenImgProjTransformer2`. This is
    used when converting geometries coordinates to target raster pixel space. For
    example this can be used to specify RPC related transformer options.

.. include:: options/co.rst

.. option:: -te <xmin> <ymin> <xmax> <ymax>

    Set georeferenced extents. The values must be expressed in georeferenced units.
    If not specified, the extent of the output file will be the extent of the vector
    layers.

.. option:: -tr <xres> <yres>

    Set target resolution. The values must be expressed in georeferenced units.
    Both must be positive values.

.. option:: -tap

    (target aligned pixels) Align
    the coordinates of the extent of the output file to the values of the :option:`-tr`,
    such that the aligned extent includes the minimum extent.
    Alignment means that xmin / resx, ymin / resy, xmax / resx and ymax / resy are integer values.

.. option:: -ts <width> <height>

    Set output file size in pixels and lines. Note that :option:`-ts` cannot be used with
    :option:`-tr`
    If one of the two values is set to 0, it will be computed from the other value in order to
    preserve the aspect ratio.

.. option:: -ot <type>

    Force the output bands to be of the indicated data type. Defaults to ``Float64``,
    unless the attribute field to burn is of type ``Int64``, in which case ``Int64``
    is used for the output raster data type if the output driver supports it.

.. option:: -optim {AUTO|VECTOR|RASTER}

    Force the algorithm used (results are identical). Raster mode
    is used in most cases and  optimizes read/write  operations.  The
    vector mode is useful with a large amount of input features and
    optimizes CPU use, provided that the output image is tiled.
    Auto mode (the default) will choose the
    algorithm based on input and output properties.

.. option:: -oo <NAME>=<VALUE>

    .. versionadded:: 3.7

    Source dataset open option (format specific)

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: <src_datasource>

    Any OGR supported readable datasource.

.. option:: <dst_filename>

    The GDAL supported output file.  Must support update mode access.
    This file will be created if it does not already exist
    If the output raster already exists, the affected pixels are updated in-place.

The program creates a new target raster image when any of the :option:`-of`,
:option:`-a_nodata`, :option:`-init`, :option:`-a_srs`, :option:`-co`, :option:`-te`,
:option:`-tr`, :option:`-tap`, :option:`-ts`, or :option:`-ot` options are used.
The resolution or size must be specified using the :option:`-tr` or :option:`-ts` option for all new
rasters.  The target raster will be overwritten if it already exists and any of
these creation-related options are used.

C API
-----

This utility is also callable from C with :cpp:func:`GDALRasterize`.

Examples
--------

.. example::

   The following would burn all polygons from :file:`mask.shp` into the RGB TIFF
   file :file:`work.tif` with the color red (RGB = 255,0,0).

   .. code-block:: bash

       gdal_rasterize -b 1 -b 2 -b 3 -burn 255 -burn 0 -burn 0 -l mask mask.shp work.tif

.. example::

   The following would burn all "class A" buildings into the output elevation
   file, pulling the top elevation from the ROOF_H attribute.

   .. code-block:: bash

       gdal_rasterize -a ROOF_H -where "class='A'" -l footprints footprints.shp city_dem.tif

.. example::

   The following would burn all polygons from :file:`footprint.shp` into a new 1000x1000
   rgb TIFF as the color red.  Note that :option:`-b` is not used; the order of the :option:`-burn`
   options determines the bands of the output raster.

   .. code-block:: bash

       gdal_rasterize -burn 255 -burn 0 -burn 0 -ot Byte -ts 1000 1000 -l footprints footprints.shp mask.tif
