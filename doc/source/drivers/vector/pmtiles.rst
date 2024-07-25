.. _vector.pmtiles:

================================================================================
PMTiles -- ProtoMaps Tiles
================================================================================

.. versionadded:: 3.8

.. shortname:: PMTiles

.. built_in_by_default::

This driver supports reading and writing `PMTiles <https://github.com/protomaps/PMTiles>`__
datasets containing vector tiles, encoded in the MapVector Tiles (MVT) format.

PMTiles is a single-file archive format for tiled data. A PMTiles archive can
be hosted on a commodity storage platform such as S3, and enables low-cost,
zero-maintenance map applications that are "serverless" - free of a custom tile
backend or third party provider.

This driver is compatible with all GDAL
:ref:`network-based virtual file systems <network_based_file_systems>`

The SRS is always the `Pseudo-Mercator <https://en.wikipedia.org/wiki/Web_Mercator_projection>`__
(a.k.a Google Mercator) projection, EPSG:3857.

The supported datasets must contain a JSON metadata document
following the
`MBTiles specification <https://github.com/mapbox/mbtiles-spec/blob/master/1.3/spec.md#vector-tileset-metadata>`__,
containing at least the ``vector_layers`` array.

Note that the driver will make no effort of stitching together geometries for
linear or polygonal features that overlap several tiles. An application that
wishes to eliminate those interrupts could potentially use the CLIP=NO open
option to get larger boundaries, and use appropriate clipping graphic primitives
to hide those discontinuities.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Opening options
---------------

|about-open-options|
The following open options are available:

-  .. oo:: CLIP
     :choices: YES, NO
     :default: YES

     Whether to clip geometries of vector features to tile extent.

     Vector tiles are generally produced with a buffer that provides overlaps
     between adjacent tiles, and can be used to display them properly. When
     using vector tiles as a vector layer source, like in OGR vector model,
     this padding is undesirable, hence the default behavior of clipping.

-  .. oo:: ZOOM_LEVEL
     :choices: <integer>

     Zoom level to use. Must be between the minimum and maximum zoom levels
     reported in the metadata. Defaults to the maximum zoom level available in
     the dataset.

-  .. oo:: ZOOM_LEVEL_AUTO
     :choices: YES, NO
     :default: NO

     Whether to auto-select the zoom level for vector layers according to the
     spatial filter extent.
     Only for display purpose.

-  .. oo:: JSON_FIELD
     :choices: YES, NO
     :default: NO

     Whether tile attributes should be serialized in a single ``json`` field
     as JSON. This may be useful if tiles may have different attribute schemas.

Creation issues
---------------

Tiles are generated with WebMercator (EPSG:3857) projection.
Several layers can be written. It is possible to decide at which zoom
level ranges a given layer is written.

Part of the conversion is multi-threaded by default, using as many
threads as there are cores. The number of threads used can be controlled
with the :config:`GDAL_NUM_THREADS` configuration option.

The driver implements also a direct translation mode when using :program:`ogr2ogr`
with a MBTiles vector dataset as input and a PMTiles output dataset, without
any argument: ``ogr2ogr out.pmtiles in.mbtiles``. In that mode, existing MVT
tiles from the MBTiles files are used as such, contrary to the general writing
mode that will involve computing them by discretizing geometry coordinates.

Dataset creation options
------------------------

|about-dataset-creation-options|
The following dataset creation options are supported:

-  .. co:: NAME

      Tileset name. Defaults to the basename of the
      output file/directory. Used to fill metadata records.

-  .. co:: DESCRIPTION

      A description of the tileset. Used to fill metadata records.

-  .. co:: TYPE
      :choices: overlay, baselayer

      Layer type. Used to fill metadata records.

-  .. co:: MINZOOM
      :choices: <integer>
      :default: 0

      Minimum zoom level at which tiles are generated.

-  .. co:: MAXZOOM
      :choices: <integer>
      :default: 5

       Maximum zoom level at which tiles are
       generated. Maximum supported value is 22.

-  .. co:: CONF
      :choices: <json>, <filename>

      Layer configuration as a JSON serialized string.
      Or filename containing the configuration as JSON.

-  .. co:: SIMPLIFICATION
      :choices: float

      Simplification factor for linear or
      polygonal geometries. The unit is the integer unit of tiles after
      quantification of geometry coordinates to tile coordinates. Applies
      to all zoom levels, unless :co:`SIMPLIFICATION_MAX_ZOOM` is also defined.

-  .. co:: SIMPLIFICATION_MAX_ZOOM
      :choices: <float>

      Simplification factor for linear
      or polygonal geometries, that applies only for the maximum zoom
      level.

-  .. co:: EXTENT
      :choices: <positive integer>
      :default: 4096

      Number of units in a tile. The
      greater, the more accurate geometry coordinates (at the expense of
      tile byte size).

-  .. co:: BUFFER
      :choices: <positive integer>

      Number of units for geometry
      buffering. This value corresponds to a buffer around each side of a
      tile into which geometries are fetched and clipped. This is used for
      proper rendering of geometries that spread over tile boundaries by
      some rendering clients. Defaults to 80 if :co:`EXTENT=4096`.

-  .. co:: MAX_SIZE
      :choices: <integer>
      :default: 500000

      Maximum size of a tile in bytes (after
      compression). If a tile is greater than this
      threshold, features will be written with reduced precision, or
      discarded.

-  .. co:: MAX_FEATURES
      :choices: <integer>
      :default: 200000

      Maximum number of features per tile.

Layer configuration
-------------------

The above mentioned CONF dataset creation option can be set to a string
whose value is a JSON serialized document such as the below one:

.. code-block:: json

           {
               "boundaries_lod0": {
                   "target_name": "boundaries",
                   "description": "Country boundaries",
                   "minzoom": 0,
                   "maxzoom": 2
               },
               "boundaries_lod1": {
                   "target_name": "boundaries",
                   "minzoom": 3,
                   "maxzoom": 5
               }
           }

*boundaries_lod0* and *boundaries_lod1* are the name of the OGR layers
that are created into the target MVT dataset. They are mapped to the MVT
target layer *boundaries*.

It is also possible to get the same behavior with the below layer
creation options, although that is not convenient in the ogr2ogr use
case.

Layer creation options
----------------------

|about-layer-creation-options|
The following layer creation options are supported:

-  .. lco:: MINZOOM
      :choices: <integer>

      Minimum zoom level at which tiles are
      generated. Defaults to the dataset creation option :co:`MINZOOM` value.

-  .. lco:: MAXZOOM
      :choices: <integer>

      Maximum zoom level at which tiles are
      generated. Defaults to the dataset creation option :co:`MAXZOOM` value.
      Maximum supported value is 22.

-  .. lco:: NAME

      Target layer name. Defaults to the layer name, but
      can be overridden so that several OGR layers map to a single target
      layer. The typical use case is to have different OGR layers for
      mutually exclusive zoom level ranges.

-  .. lco:: DESCRIPTION

      A description of the layer.

/vsipmtiles/ virtual file system
--------------------------------

The /vsipmtiles/ virtual file system offers a view of the content of a PMTiles
dataset has a file hierarchy, with the following structure:

::

    /pmtiles_header.json: JSON view of the PMTiles header
    /metadata.json: JSON metadata document stored in the dataset
    /{z}/: Directory with tiles for zoom level z
    /{z}/{x}/: Directory with tiles for zoom level z and x
    /{z}/{x}/{y}.{ext}: Tile data

The :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_ls.py`
and :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_cp.py`
sample utilities can be used to explore and extract data from a PMTiles
dataset

Listing the content of a dataset:

.. code-block:: shell

    python gdal_ls.py -lr "/vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles"

outputs:

::

    -r--r--r--  1 unknown unknown          809 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/pmtiles_header.json
    -r--r--r--  1 unknown unknown         1872 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/metadata.json
    dr-xr-xr-x  1 unknown unknown            0 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/0/
    dr-xr-xr-x  1 unknown unknown            0 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/0/0/
    -r--r--r--  1 unknown unknown          588 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/0/0/0.mvt
    dr-xr-xr-x  1 unknown unknown            0 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/1/
    dr-xr-xr-x  1 unknown unknown            0 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/1/1/
    -r--r--r--  1 unknown unknown          590 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/1/1/0.mvt
    [ ... snip ... ]
    -r--r--r--  1 unknown unknown          771 2023-05-29 09:06 /vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/14/8707/5974.mvt


Displaying the metadata JSON file:

.. code-block:: shell

    python swig/python/gdal-utils/osgeo_utils/samples/gdal_cp.py "/vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/metadata.json" /vsistdout/ | jq .

outputs:

.. code-block:: json

    {
      "attribution": "<a href=\"https://protomaps.com\" target=\"_blank\">Protomaps</a> © <a href=\"https://www.openstreetmap.org\" target=\"_blank\"> OpenStreetMap</a>",
      "name": "protomaps 2023-01-18T07:49:39Z",
      "type": "baselayer",
      "vector_layers": [
        {
          "fields": {},
          "id": "earth"
        },
        {
          "fields": {
            "boundary": "string",
            "landuse": "string",
            "leisure": "string",
            "name": "string",
            "natural": "string"
          },
          "id": "natural"
        },
        { "... snip ...": {} },
        {
          "fields": {
            "pmap:min_admin_level": "number"
          },
          "id": "boundaries"
        },
        {
          "fields": {},
          "id": "mask"
        }
      ]
    }


Extracting all content in a local directory:

.. code-block:: shell

    python swig/python/gdal-utils/osgeo_utils/samples/gdal_cp.py -r "/vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles" out_pmtiles

Examples
--------
-  Simple translation of a single shapefile into PMTiles. Dataset creation options (dsco) MINZOOM and MAXZOOM specifies tile zoom levels.
   ::

      ogr2ogr -dsco MINZOOM=10 -dsco MAXZOOM=20 -f "PMTiles" filename.pmtiles my_shapes.shp

-  Merge all PostgreSQL/PostGIS tables in a schema into a single PMTiles file. PostgreSQL table names are used as layer names. Dataset creation options (dsco) MINZOOM and MAXZOOM specifies tile zoom levels.
   ::

      ogr2ogr -dsco MINZOOM=0 -dsco MAXZOOM=22 -f "PMTiles" filename.pmtiles "PG:host=my_host port=my_port dbname=my_database user=my_user password=my_password schemas=my_schema"


See Also
--------

-  `PMTiles specification <https://github.com/protomaps/PMTiles>`__
-  :ref:`Mapbox Vector tiles driver <vector.mvt>`
-  :ref:`MBTiles driver <raster.mbtiles>`
