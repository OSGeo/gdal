.. _gdal_vector_clip:

================================================================================
``gdal vector clip``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Clip a vector dataset.

.. Index:: gdal vector clip

Synopsis
--------

.. program-output:: gdal vector clip --help-doc

Description
-----------

:program:`gdal vector clip` can be used to clip a vector dataset using
georeferenced coordinates.

Either :option:`--bbox`, :option:`--geometry` or :option:`--like` must be specified.

``clip`` can also be used as a step of :ref:`gdal_vector_pipeline`.

Clipping sometimes results in geometries that are of a type not compatible
with the geometry type. This program splits multi-geometries or geometry
collections into their parts for layers that are of a single geometry type
(such as point, linestring, polygon) and only keeps the parts that are of that
type. It also promotes single geometry types (e.g. polygons) to multi
geometry types (e.g. multi-polygons). If the user needs to preserve any type of
intersection, the layer must use the ``wkbUnknown`` (any geometry) type.

Program-Specific Options
------------------------

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Bounds to which to clip the dataset. They are assumed to be in the CRS of
    the input dataset, unless :option:`--bbox-crs` is specified.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    Mutually exclusive with :option:`--like` and :option:`--geometry`.

.. option:: --bbox-crs <CRS>

    CRS in which the <xmin>,<ymin>,<xmax>,<ymax> values of :option:`--bbox`
    are expressed. If not specified, it is assumed to be the CRS of the input
    dataset.
    Note that specifying --bbox-crs does not involve doing vector reprojection.
    Instead, the bounds are reprojected from the bbox-crs to the CRS of the
    input dataset.

.. option:: --geometry <WKT_or_GeoJSON>

    Geometry as a WKT or GeoJSON string to which to clip the dataset.
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
    Note that specifying --geometry-crs does not involve doing vector reprojection.
    Instead, the bounds are reprojected from the geometry-crs to the CRS of the
    input dataset.

.. option:: --like <DATASET>

    Vector or raster dataset to use as a template for bounds.
    If the specified dataset is a raster, its rectangular bounds are used as
    the clipping geometry.
    If the specified dataset is a vector dataset, its polygonal geometries
    are unioned together to form the clipping geometry (beware that the result
    union might not be perfect if there are gaps between individual polygon
    features). If several layers are present,
    :option:`--like-sql` or :option:`--like-layer` must be specified.
    Mutually exclusive with :option:`--bbox` and :option:`--geometry`.

.. option:: --like-layer <LAYER-NAME>

    Select the named layer from the vector clip dataset.
    Mutually exclusive with :option:`--like-sql`

.. option:: --like-sql <SELECT-STATEMENT>

    Select desired geometries from the vector clip dataset using an SQL query.
    e.g ``SELECT geom FROM my_layer WHERE country = 'France'``.
    Mutually exclusive with :option:`--like-layer` and :option:`--like-where`

.. option:: --like-where <WHERE-EXPRESSION>

    Restrict desired geometries from vector clip dataset layer based on an attribute query.
    e.g ``country = 'France'``.


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_layer.rst

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/input_layer.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_layer.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst


Examples
--------

.. example::
   :title: Clip a GeoPackage file to the bounding box from longitude 2, latitude 49, to longitude 3, latitude 50 in WGS 84

   .. code-block:: bash

        $ gdal vector clip --bbox=2,49,3,50 --bbox-crs=EPSG:4326 in.gpkg out.gpkg --overwrite
