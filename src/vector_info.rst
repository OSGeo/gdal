.. _vector_info:

================================================================================
Getting information from vector datasets
================================================================================

Utility
-------

`gdal vector info <https://gdal.org/en/stable/programs/gdal_vector_info.html>`__


Getting vector layer names
--------------------------

::

    # run from the workshop data directory

    $ gdal vector info timisoara.osm.pbf --summary

Output:

::

    INFO: Open of `timisoara.osm.pbf'
          using driver `OSM' successful.
    1: points (Point)
    2: lines (Line String)
    3: multilinestrings (Multi Line String)
    4: multipolygons (Multi Polygon)
    5: other_relations (Geometry Collection)

Getting structure of vector layers
----------------------------------

::

    $ gdal vector info timisoara.osm.pbf

Output:

::

    INFO: Open of `timisoara.osm.pbf'
          using driver `OSM' successful.

    Layer name: points
    Geometry: Point
    Feature Count: -1
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
    Data axis to CRS axis mapping: 2,1
    osm_id: String (0.0)
    name: String (0.0)
    barrier: String (0.0)
    highway: String (0.0)
    ref: String (0.0)
    address: String (0.0)
    is_in: String (0.0)
    place: String (0.0)
    man_made: String (0.0)
    other_tags: String (0.0)

    Layer name: lines
    Geometry: Line String
    Feature Count: -1
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
    Data axis to CRS axis mapping: 2,1
    osm_id: String (0.0)
    name: String (0.0)
    highway: String (0.0)
    waterway: String (0.0)
    aerialway: String (0.0)
    barrier: String (0.0)
    man_made: String (0.0)
    railway: String (0.0)
    z_order: Integer (0.0)
    other_tags: String (0.0)

    Layer name: multilinestrings
    Geometry: Multi Line String
    Feature Count: -1
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
    Data axis to CRS axis mapping: 2,1
    osm_id: String (0.0)
    name: String (0.0)
    type: String (0.0)
    other_tags: String (0.0)

    Layer name: multipolygons
    Geometry: Multi Polygon
    Feature Count: -1
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
    Data axis to CRS axis mapping: 2,1
    osm_id: String (0.0)
    osm_way_id: String (0.0)
    name: String (0.0)
    type: String (0.0)
    aeroway: String (0.0)
    amenity: String (0.0)
    admin_level: String (0.0)
    barrier: String (0.0)
    boundary: String (0.0)
    building: String (0.0)
    craft: String (0.0)
    geological: String (0.0)
    historic: String (0.0)
    land_area: String (0.0)
    landuse: String (0.0)
    leisure: String (0.0)
    man_made: String (0.0)
    military: String (0.0)
    natural: String (0.0)
    office: String (0.0)
    place: String (0.0)
    shop: String (0.0)
    sport: String (0.0)
    tourism: String (0.0)
    other_tags: String (0.0)

    Layer name: other_relations
    Geometry: Geometry Collection
    Feature Count: -1
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
    Data axis to CRS axis mapping: 2,1
    osm_id: String (0.0)
    name: String (0.0)
    type: String (0.0)
    other_tags: String (0.0)

Listing features
----------------

Listing all features from the ``points`` layer

::

    $ gdal vector info timisoara.osm.pbf --input-layer points --features


Output:

::

    INFO: Open of `timisoara.osm.pbf'
          using driver `OSM' successful.

    Layer name: points
    Geometry: Point
    Feature Count: -1
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
    Data axis to CRS axis mapping: 2,1
    osm_id: String (0.0)
    name: String (0.0)
    barrier: String (0.0)
    highway: String (0.0)
    ref: String (0.0)
    address: String (0.0)
    is_in: String (0.0)
    place: String (0.0)
    man_made: String (0.0)
    other_tags: String (0.0)
    OGRFeature(points):25478311
      osm_id (String) = 25478311
      highway (String) = traffic_signals
      POINT (21.2694902 45.7290217)

    OGRFeature(points):25478391
      osm_id (String) = 25478391
      name (String) = Elbromplast
      other_tags (String) = "bus"=>"yes","public_transport"=>"stop_position"
      POINT (21.2698833 45.7331868)

    [ ... snip ... ]


Applying an attribute filter, and limiting the output
-----------------------------------------------------

You can apply an attribute filter by specifying a SQL WHERE filtering clause
with the ``--where`` option. By default, this uses the `OGR SQL dialect <https://gdal.org/en/stable/user/ogr_sql_dialect.html>`__
which is a subset of the full SQL-92 standard.

::

    $ gdal vector info timisoara.osm.pbf --input-layer points --features --where "other_tags LIKE '%restaurant%'" --limit 5


Output:

::

    INFO: Open of `timisoara.osm.pbf'
          using driver `OSM' successful.

    Layer name: points
    Geometry: Point
    Feature Count: -1
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
    Data axis to CRS axis mapping: 2,1
    osm_id: String (0.0)
    name: String (0.0)
    barrier: String (0.0)
    highway: String (0.0)
    ref: String (0.0)
    address: String (0.0)
    is_in: String (0.0)
    place: String (0.0)
    man_made: String (0.0)
    other_tags: String (0.0)
    OGRFeature(points):304555464
      osm_id (String) = 304555464
      name (String) = Stejarul
      other_tags (String) = "amenity"=>"restaurant"
      POINT (21.4248647 45.7521308)

    OGRFeature(points):380526548
      osm_id (String) = 380526548
      name (String) = Restaurant Tinecz
      other_tags (String) = "addr:city"=>"Timișoara","addr:housenumber"=>"51","addr:street"=>"Calea Aradului","amenity"=>"restaurant","cuisine"=>"regional","opening_hours"=>"Mo-Su 11:00-23:00","phone"=>"0736 660 787","website"=>"https://www.restauranttinecz.ro/"
      POINT (21.2221875 45.7710081)

    OGRFeature(points):380526549
      osm_id (String) = 380526549
      name (String) = Iris
      other_tags (String) = "amenity"=>"restaurant","cuisine"=>"regional"
      POINT (21.2147065 45.7746345)

    OGRFeature(points):477200233
      osm_id (String) = 477200233
      name (String) = Pizzeria Rebecca
      other_tags (String) = "amenity"=>"restaurant"
      POINT (21.2240089 45.7754256)

    OGRFeature(points):477234572
      osm_id (String) = 477234572
      name (String) = Pizzeria Amedeea
      other_tags (String) = "amenity"=>"restaurant"
      POINT (21.2310551 45.7757755)


JSON output / open options
--------------------------

A number of GDAL raster and vector drivers have "open options", which control
how the driver behaves. They are documented in each driver's documentation page.
For OSM, at https://gdal.org/en/stable/drivers/vector/osm.html#open-options

The ``other_tags`` field is exposed using the `PostgreSQL HSTORE <https://www.postgresql.org/docs/current/hstore.html>`__
key/value data type which predates the introduction of JSON. But we may query
it to be exposed as JSON with ``--open-option TAGS_FORMAT=JSON``, and also request
features to be exposed as GeoJSON with ``--format json``.

::

    $ gdal vector info timisoara.osm.pbf --input-layer points --features --where "other_tags LIKE '%restaurant%'" --limit 1 --format json --open-option TAGS_FORMAT=JSON


.. code-block:: json


    {
      "description":"timisoara.osm.pbf",
      "driverShortName":"OSM",
      "driverLongName":"OpenStreetMap XML and PBF",
      "layers":[
        {
          "name":"points",
          "metadata":{},
          "geometryFields":[
            {
              "name":"",
              "type":"Point",
              "nullable":true,
              "coordinateSystem":{
                "wkt":"GEOGCRS[\"WGS 84\",\n    DATUM[\"World Geodetic System 1984\",\n        ELLIPSOID[\"WGS 84\",6378137,298.257223563,\n            LENGTHUNIT[\"metre\",1]]],\n    PRIMEM[\"Greenwich\",0,\n        ANGLEUNIT[\"degree\",0.0174532925199433]],\n    CS[ellipsoidal,2],\n        AXIS[\"geodetic latitude (Lat)\",north,\n            ORDER[1],\n            ANGLEUNIT[\"degree\",0.0174532925199433]],\n        AXIS[\"geodetic longitude (Lon)\",east,\n            ORDER[2],\n            ANGLEUNIT[\"degree\",0.0174532925199433]],\n    ID[\"EPSG\",4326]]",
                "projjson":{
                  "$schema":"https://proj.org/schemas/v0.7/projjson.schema.json",
                  "type":"GeographicCRS",
                  "name":"WGS 84",
                  "datum":{
                    "type":"GeodeticReferenceFrame",
                    "name":"World Geodetic System 1984",
                    "ellipsoid":{
                      "name":"WGS 84",
                      "semi_major_axis":6378137,
                      "inverse_flattening":298.257223563
                    }
                  },
                  "coordinate_system":{
                    "subtype":"ellipsoidal",
                    "axis":[
                      {
                        "name":"Geodetic latitude",
                        "abbreviation":"Lat",
                        "direction":"north",
                        "unit":"degree"
                      },
                      {
                        "name":"Geodetic longitude",
                        "abbreviation":"Lon",
                        "direction":"east",
                        "unit":"degree"
                      }
                    ]
                  },
                  "id":{
                    "authority":"EPSG",
                    "code":4326
                  }
                },
                "dataAxisToSRSAxisMapping":[
                  2,
                  1
                ]
              }
            }
          ],
          "featureCount":-1,
          "fields":[
            {
              "name":"osm_id",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"name",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"barrier",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"highway",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"ref",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"address",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"is_in",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"place",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"man_made",
              "type":"String",
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"other_tags",
              "type":"String",
              "subType":"JSON",
              "nullable":true,
              "uniqueConstraint":false
            }
          ],
          "features":[
            {
              "type":"Feature",
              "properties":{
                "osm_id":"304555464",
                "name":"Stejarul",
                "other_tags":{
                  "amenity":"restaurant"
                }
              },
              "fid":304555464,
              "geometry":{
                "type":"Point",
                "coordinates":[
                  21.4248647,
                  45.7521308
                ]
              }
            }
          ]
        }
      ],
      "metadata":{},
      "domains":{}
    }

Exercise
--------

Find the 5 closest bars from Timișoara center (45.7558° N, 21.2322° E)

.. collapse:: (hint)

    .. hint::

        - use option ``--sql``
        - use SQLite dialect (``--dialect SQLite``)
        - the name of the geometry column when using a non-RDBMS data set, such as OSM, and with SQLite dialect, is ``geometry``
        - use Spatialite `ST_Point <https://www.gaia-gis.it/gaia-sins/spatialite-sql-5.1.0.html#p0>`__ function
        - use Spatialite `ST_Distance <https://www.gaia-gis.it/gaia-sins/spatialite-sql-5.1.0.html#p13>`__ function

    ==> :ref:`solution_vector_info`.
