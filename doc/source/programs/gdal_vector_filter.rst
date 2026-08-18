.. _gdal_vector_filter:

.. program:: gdal_vector_filter

================================================================================
``gdal vector filter``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Filter a vector dataset.

.. Index:: gdal vector filter

Synopsis
--------

.. program-output:: gdal vector filter --help-doc

Description
-----------

:program:`gdal vector filter` can be used to filter a vector dataset from
their spatial extent or a SQL WHERE clause.

``filter`` can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst


Program-Specific Options
------------------------

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Bounds to which to filter the dataset. They are assumed to be in the CRS of
    the input dataset.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    Note that filtering does not clip geometries to the bounding box.
    Mutually exclusive with :option:`--geometry`.

.. option:: --bbox-crs <CRS>

    .. versionadded:: 3.14

    CRS in which the <xmin>,<ymin>,<xmax>,<ymax> values of :option:`--bbox`
    are expressed. If not specified, it is assumed to be the CRS of each input
    layer. Note that in the general case, the reprojected bounding box will
    generally cover a larger area than the one specified in the bounding box CRS,
    since a rectangle does not generally reproject to a rectangle.

.. option:: --geometry <WKT_or_GeoJSON>

    .. versionadded:: 3.14

    Geometry as a WKT or GeoJSON string used to filter the dataset. All features intersecting the provided geometry will be retained.
    If the input geometry is GeoJSON, its CRS is assumed to be WGS84, unless there is
    a CRS defined in the GeoJSON geometry or :option:`--geometry-crs` is specified.
    If the input geometry is WKT, its CRS is assumed to be the one of the input dataset,
    unless :option:`--geometry-crs` is specified.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    Mutually exclusive with :option:`--bbox`.

.. option:: --geometry-crs <CRS>

    .. versionadded:: 3.14

    CRS in which the coordinates values of :option:`--geometry`
    are expressed. If not specified, it is assumed to be the CRS of the input
    dataset.
    Note that specifying :option:`--geometry-crs` does not cause vector reprojection
    of the output features. Instead, the vertices of the filter geometry provided by :option:`--geometry` are reprojected
    from the geometry-crs to the CRS of each layer of the input dataset.

.. option:: --update-extent

    Update layer extent to take into account the filter(s). Otherwise the layer
    extent will generally be the one of the source layer before applying the
    filter(s). Note that using this option requires doing a full scan of the
    filtered layers.

.. option:: --where <WHERE>|@<filename>

    Attribute query (like SQL WHERE).


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

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Select features from a GeoPackage file that intersect the bounding box from longitude 2, latitude 49, to longitude 3, latitude 50 in WGS 84

   .. code-block:: bash

        $ gdal vector filter --bbox=2,49,3,50 in.gpkg out.gpkg --overwrite

.. example::
   :title: Filter Shapefile features with an attribute query
   :id: gdal-vector-filter-where

   .. tabs::

      .. code-tab:: bash

        gdal vector pipeline \
            ! read in.shp \
            ! filter --where "CODE IS NULL AND NAME NOT LIKE 'TEMP%'" \
            ! write out.gpkg

      .. code-tab:: ps1

        gdal vector pipeline `
            ! read in.shp `
            ! filter --where "CODE IS NULL AND NAME NOT LIKE 'TEMP%'" `
            ! write out.gpkg
