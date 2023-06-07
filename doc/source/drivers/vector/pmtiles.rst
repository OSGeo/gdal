.. _vector.pmtiles:

================================================================================
PMTiles -- ProtoMaps Tiles
================================================================================

.. versionadded:: 3.8

.. shortname:: PMTiles

This driver supports reading `PMTiles <https://github.com/protomaps/PMTiles>`__
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

.. supports_georeferencing::

.. supports_virtualio::

Opening options
---------------

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


See Also
--------

-  `PMTiles specification <https://github.com/protomaps/PMTiles>`__
-  :ref:`Mapbox Vector tiles driver <vector.mvt>`
-  :ref:`MBTiles driver <raster.mbtiles>`
