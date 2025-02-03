.. _gdal_vector_clip_subcommand:

================================================================================
"gdal vector clip" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Clip a vector dataset.

.. Index:: gdal vector clip

Synopsis
--------

.. code-block::

    Usage: gdal vector clip [OPTIONS] <INPUT> <OUTPUT>

    Clip a vector dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input vector dataset [required]
      -o, --output <OUTPUT>                                Output vector dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --version                                            Display GDAL version and exit
      --json-usage                                         Display usage as JSON document and exit
      --drivers                                            Display driver list as JSON document and exit
      --config <KEY>=<VALUE>                               Configuration option [may be repeated]
      --progress                                           Display progress bar

    Options:
      -l, --layer, --input-layer <INPUT-LAYER>             Input layer name(s) [may be repeated]
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --lco, --layer-creation-option <KEY>=<VALUE>         Layer creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --update                                             Whether to open existing dataset in update mode
      --overwrite-layer                                    Whether overwriting existing layer is allowed
      --append                                             Whether appending to existing layer is allowed
      --output-layer <OUTPUT-LAYER>                        Output layer name
      --bbox <BBOX>                                        Clipping bounding box as xmin,ymin,xmax,ymax
                                                           Mutually exclusive with --geometry, --like
      --bbox-crs <BBOX-CRS>                                CRS of clipping bounding box
      --geometry <GEOMETRY>                                Clipping geometry (WKT or GeoJSON)
                                                           Mutually exclusive with --bbox, --like
      --geometry-crs <GEOMETRY-CRS>                        CRS of clipping geometry
      --like <DATASET>                                     Dataset to use as a template for bounds
                                                           Mutually exclusive with --bbox, --geometry
      --like-sql <SELECT-STATEMENT>                        SELECT statement to run on the 'like' dataset
      --like-layer <LAYER-NAME>                            Name of the layer of the 'like' dataset
      --like-where <WHERE-EXPRESSION>                      Expression for a WHERE SQL clause to run on the 'like' dataset


    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]



Description
-----------

:program:`gdal vector clip` can be used to clip a vector dataset using
georeferenced coordinates.

Either :option:`--bbox`, :option:`--geometry` or :option:`--like` must be specified.

``clip`` can also be used as a step of :ref:`gdal_vector_pipeline_subcommand`.

Clipping sometimes results in geometries that are of a type not compatible
with the geometry type. This program splits multi-geometries or geometry
collections into their parts for layers that are of a single geometry type
(such as point, linestring, polygon) and only keeps the parts that are of that
type. It also promotes single geometry types (e.g. polygons) to multi
geometry types (e.g. multi-polygons). If the user needs to preserve any type of
intersection, the layer must use the ``wkbUnknown`` (any geometry) type.

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/overwrite.rst

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
    Not that specifying --bbox-crs does not involve doing vector reprojection.
    Instead, the bounds are reprojected from the bbox-crs to the CRS of the
    input dataset.

.. option:: --geometry <WKT_or_GeoJSON>

    Geometry as a WKT or GeoJSON string to which to clip the dataset.
    It is assumed to be in the CRS of
    the input dataset, unless :option:`--geometry-crs` is specified.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    Mutually exclusive with :option:`--bbox` and :option:`--like`.

.. option:: --geometry-crs <CRS>

    CRS in which the coordinates values of :option:`--geometry`
    are expressed. If not specified, it is assumed to be the CRS of the input
    dataset.
    Not that specifying --geometry-crs does not involve doing vector reprojection.
    Instead, the bounds are reprojected from the geometry-crs to the CRS of the
    input dataset.

.. option:: --like <DATASET>

    Vector or raster dataset to use as a template for bounds.
    If the specified dataset is a raster, its rectangular bounds are use as
    the clipping geometry.
    If the specified dataset is a vector dataset, its polygonal geometries
    are unioned together to form the clipping geometry (beware that the result
    union might not be perfect if there are gaps between individual polygon
    features). If several layers are present,
    :option:`--like-sql` or :option:`--like-layer` must be specified.
    Mutually exclusive with :option:`--bbox` and :option:`--geometry`.

.. option:: --like-sql <SELECT-STATEMENT>

    Select desired geometries from the vector clip dataset using an SQL query.
    e.g ``SELECT geom FROM my_layer WHERE country = 'France'``.
    Mutually exclusive with :option:`--like-layer` and :option:`--like-where`

.. option:: --like-layer <LAYER-NAME>

    Select the named layer from the vector clip dataset.
    Mutually exclusive with :option:`--like-sql`

.. option:: --like-where <WHERE-EXPRESSION>

    Restrict desired geometries from vector clip dataset layer based on an attribute query.
    e.g ``country = 'France'``.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Clip a GeoPackage file to the bounding box from longitude 2, latitude 49, to longitude 3, latitude 50 in WGS 84

   .. code-block:: bash

        $ gdal vector clip --bbox=2,49,3,50 --bbox-crs=EPSG:4326 in.gpkg out.gpkg --overwrite
