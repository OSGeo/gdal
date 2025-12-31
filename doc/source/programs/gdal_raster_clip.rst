.. _gdal_raster_clip:

================================================================================
``gdal raster clip``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Clip a raster dataset.

.. Index:: gdal raster clip

Synopsis
--------

.. program-output:: gdal raster clip --help-doc

Description
-----------

:program:`gdal raster clip` can be used to clip a raster dataset using
georeferenced coordinates.

Either :option:`--bbox` or :option:`--like` must be specified.

The output dataset is in the same SRS as the input one, and the original
resolution is preserved. Bounds are rounded to match whole pixel locations
(i.e. there is no resampling involved)

``clip`` can also be used as a step of :ref:`gdal_raster_pipeline`.

Program-Specific Options
------------------------

.. option:: --add-alpha

    Adds an alpha mask band to the destination when the source raster has none.

.. option:: --allow-bbox-outside-source

    If set, allows the bounds indicated by :option:`--bbox` to cover an extent that is greater
    than the input dataset. Output pixels from areas beyond the input extent will be set to
    zero or the NoData value of the input dataset.

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Bounds to which to clip the dataset. They are assumed to be in the CRS of
    the input dataset, unless :option:`--bbox-crs` is specified.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    The bounds are expanded if necessary to match input pixel boundaries.
    By default, :program:`gdal raster clip` will produce an error if the bounds indicated
    by :option:`--bbox` are greater than the extents of input dataset. This check can be
    bypassed using :option:`--allow-bbox-outside-source`.

.. option:: --bbox-crs <CRS>

    CRS in which the <xmin>,<ymin>,<xmax>,<ymax> values of :option:`--bbox`
    are expressed. If not specified, it is assumed to be the CRS of the input
    dataset.
    Note that specifying :option:`--bbox-crs` does not cause the raster to be reprojected.
    Instead, the bounds are reprojected from the bbox-crs to the CRS of the
    input dataset.

.. option:: --geometry <WKT_or_GeoJSON>

    Geometry as a WKT or GeoJSON string of a polygon (or multipolygon) to which
    to clip the dataset.
    Raster areas within the bounding box of the geometry but not inside the
    geometry itself will be set to the nodata value of the raster, or 0 if there
    is none. All pixels overlapping the geometry will be selected.
    If the input geometry is GeoJSON, its CRS is assumed to be WGS84, unless there is
    a CRS defined in the GeoJSON geometry or :option:`--geometry-crs` is specified.
    If the input geometry is WKT, its CRS is assumed to be the one of the input dataset,
    unless :option:`--geometry-crs` is specified.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    Mutually exclusive with :option:`--bbox` and :option:`--like`.

.. option:: --geometry-crs <CRS>

    CRS in which the coordinates values of :option:`--geometry`
    are expressed. If not specified, it is assumed to be the CRS of the input
    dataset.
    The bounds are reprojected from the geometry-crs to the CRS of the
    input dataset.

.. option:: --like <DATASET>

    Vector or raster dataset to use as a template for bounds.
    If the specified dataset is a raster, its rectangular bounds are used as
    the clipping geometry.
    If the specified dataset is a vector dataset, its polygonal geometries
    are unioned together to form the clipping geometry. If several layers are present,
    :option:`--like-sql` or :option:`--like-layer` must be specified.
    Raster areas within the bounding box of the geometry but not inside the
    geometry itself will be set to the nodata value of the raster, or 0 if there
    is none.
    Mutually exclusive with :option:`--bbox` and :option:`--geometry`.

.. option:: --like-layer <LAYER-NAME>

    Select the named layer from the vector clip dataset.
    Mutually exclusive with :option:`--like-sql`

.. option:: --like-sql <SELECT-STATEMENT>

    Select desired geometries from the vector clip dataset using an SQL query.
    e.g ``SELECT geom FROM my_layer WHERE country = 'France'``.
    The SQL dialect used will be the default one of the ``like`` dataset (OGR SQL
    for Shapefile, SQLite for GeoPackage, PostgreSQL for PostGIS, etc.).
    Mutually exclusive with :option:`--like-layer` and :option:`--like-where`

.. option:: --like-where <WHERE-EXPRESSION>

    Restrict desired geometries from vector clip dataset layer based on an attribute query.
    e.g ``country = 'France'``.

.. option:: --only-bbox

    For :option:`--geometry` and :option:`--like`, only consider the bounding box
    of the geometry.

.. option:: --window <column>,<line>,<width>,<height>

    .. versionadded:: 3.12

    Selects a subwindow from the source image for copying based on pixel/line
    location. Pixel/line offsets (``column`` and ``line``) are measured from the
    left and top of the image.
    If the subwindow extends beyond the bounds of the source dataset,
    output pixels will be written with a value of zero, unless a NoData value is
    defined either the source dataset.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Clip a GeoTIFF file to the bounding box from longitude 2, latitude 49, to longitude 3, latitude 50 in WGS 84

   .. code-block:: bash

        $ gdal raster clip --bbox=2,49,3,50 --bbox-crs=EPSG:4326 in.tif out.tif --overwrite

.. example::
   :title: Clip a GeoTIFF file using the bounds of :file:`reference.tif`

   .. code-block:: bash

        $ gdal raster clip --like=reference.tif in.tif out.tif --overwrite

.. example::
   :title: Clip a GeoTIFF file from raster column 1000 and line 2000, for a width of 500 pixels and a height of 600 pixels

   .. code-block:: bash

        $ gdal raster clip --window=1000,2000,500,600 in.tif out.tif --overwrite
