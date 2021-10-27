.. _gdal_rasterize:

================================================================================
gdal_rasterize
================================================================================

.. only:: html

    Burns vector geometries into a raster.

.. Index:: gdal_rasterize

Synopsis
--------

.. code-block::

    gdal_rasterize [-b band]* [-i] [-at]
        {[-burn value]* | [-a attribute_name] | [-3d]} [-add]
        [-l layername]* [-where expression] [-sql select_statement]
        [-dialect dialect] [-of format] [-a_srs srs_def] [-to NAME=VALUE]*
        [-co "NAME=VALUE"]* [-a_nodata value] [-init value]*
        [-te xmin ymin xmax ymax] [-tr xres yres] [-tap] [-ts width height]
        [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/
                CInt16/CInt32/CFloat32/CFloat64}]
        [-optim {[AUTO]/VECTOR/RASTER}] [-q]
        <src_datasource> <dst_filename>

Description
-----------

This program burns vector geometries (points, lines, and polygons) into the
raster band(s) of a raster image.  Vectors are read from OGR supported vector
formats.

Note that on the fly reprojection of vector data to the coordinate system of the
raster data is only supported since GDAL 2.1.0.

.. program:: gdal_rasterize

.. option:: -b <band>

    The band(s) to burn values into.  Multiple -b arguments may be used to burn
    into a list of bands.  The default is to burn into band 1.  Not used when
    creating a new raster.

.. option:: -i

    Invert rasterization.  Burn the fixed burn value, or the burn value associated
    with the first feature into all parts of the image *not* inside the
    provided polygon.

.. option:: -at

    Enables the ALL_TOUCHED rasterization option so that all pixels touched
    by lines or polygons will be updated, not just those on the line render path,
    or whose center point is within the polygon.  Defaults to disabled for normal
    rendering rules.

.. option:: -burn <value>

    A fixed value to burn into a band for all objects.  A list of :option:`-burn` options
    can be supplied, one per band being written to.

.. option:: -a <attribute_name>

    Identifies an attribute field on the features to be used for a burn-in value.
    The value will be burned into all output bands.

.. option:: -3d

    Indicates that a burn value should be extracted from the "Z" values of the
    feature. Works with points and lines (linear interpolation along each segment).
    For polygons, works properly only if the are flat (same Z value for all
    vertices).

.. option:: -add

    Instead of burning a new value, this adds the new value to the existing raster.
    Suitable for heatmaps for instance.

.. option:: -l <layername>

    Indicates the layer(s) from the datasource that will be used for input
    features.  May be specified multiple times, but at least one layer name or a
    :option:`-sql` option must be specified.

.. option:: -where <expression>

    An optional SQL WHERE style query expression to be applied to select features
    to burn in from the input layer(s).

.. option:: -sql <select_statement>

    An SQL statement to be evaluated against the datasource to produce a
    virtual layer of features to be burned in.

.. option:: -dialect <dialect>

    SQL dialect. In some cases can be used to use (unoptimized) OGR SQL instead of
    the native SQL of an RDBMS by passing OGRSQL. The
    "SQLITE" dialect can also be used with any datasource.

    .. versionadded:: 2.1

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

.. option:: -to NAME=VALUE

    set a transformer
    option suitable to pass to :cpp:func:`GDALCreateGenImgProjTransformer2`. This is
    used when converting geometries coordinates to target raster pixel space. For
    example this can be used to specify RPC related transformer options.

    .. versionadded:: 2.3

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

.. option:: -ts <width> <height>

    Set output file size in pixels and lines. Note that :option:`-ts` cannot be used with
    :option:`-tr`

.. option:: -ot <type>

    Force the output bands to be of the indicated data type. Defaults to ``Float64``

.. option:: -optim {[AUTO]/VECTOR/RASTER}}

    Force the algorithm used (results are identical). The raster mode is used in most cases and
    optimise read/write operations. The vector mode is useful with a decent amount of input
    features and optimise the CPU use. That mode have to be used with tiled images to be
    efficient. The auto mode (the default) will chose the algorithm based on input and output
    properties.

    .. versionadded:: 2.3

.. option:: -q

    Suppress progress monitor and other non-error output.

.. option:: <src_datasource>

    Any OGR supported readable datasource.

.. option:: <dst_filename>

    The GDAL supported output file.  Must support update mode access.
    This file will be created (or overwritten if it already exists):option:`-of`,
    :option:`-a_nodata`, :option:`-init`, :option:`-a_srs`, :option:`-co`, :option:`-te`,
    :option:`-tr`, :option:`-tap`, :option:`-ts`, or :option:`-ot` options are used.

The program create a new target raster image when any of the :option:`-of`,
:option:`-a_nodata`, :option:`-init`, :option:`-a_srs`, :option:`-co`, :option:`-te`,
:option:`-tr`, :option:`-tap`, :option:`-ts`, or :option:`-ot` options are used.
The resolution or size must be specified using the :option:`-tr` or :option:`-ts` option for all new
rasters.  The target raster will be overwritten if it already exists and any of
these creation-related options are used.

C API
-----

This utility is also callable from C with :cpp:func:`GDALRasterize`.

.. versionadded:: 2.1

Example
-------

The following would burn all polygons from mask.shp into the RGB TIFF
file work.tif with the color red (RGB = 255,0,0).

::

    gdal_rasterize -b 1 -b 2 -b 3 -burn 255 -burn 0 -burn 0 -l mask mask.shp work.tif


The following would burn all "class A" buildings into the output elevation
file, pulling the top elevation from the ROOF_H attribute.

::

    gdal_rasterize -a ROOF_H -where 'class="A"' -l footprints footprints.shp city_dem.tif

The following would burn all polygons from footprint.shp into a new 1000x1000
rgb TIFF as the color red.  Note that :option:`-b` is not used; the order of the :option:`-burn`
options determines the bands of the output raster.

::

    gdal_rasterize -burn 255 -burn 0 -burn 0 -ot Byte -ts 1000 1000 -l footprints footprints.shp mask.tif
