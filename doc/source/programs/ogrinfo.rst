.. _ogrinfo:

================================================================================
ogrinfo
================================================================================

.. only:: html

    Lists information about an OGR-supported data source. With SQL statements
    it is also possible to edit data.

.. Index:: ogrinfo

Synopsis
--------

.. code-block::

    ogrinfo [--help] [--help-general]
            [-if <driver_name>] [-json] [-ro] [-q] [-where <restricted_where>|@f<ilename>]
            [-spat <xmin> <ymin> <xmax> <ymax>] [-geomfield <field>] [-fid <fid>]
            [-sql <statement>|@<filename>] [-dialect <sql_dialect>] [-al] [-rl]
            [-so|-features] [-limit <nb_features>] [-fields={YES|NO}]]
            [-geom={YES|NO|SUMMARY|WKT|ISO_WKT}] [-oo <NAME>=<VALUE>]...
            [-nomd] [-listmdd] [-mdd <domain>|all]...
            [-nocount] [-nogeomtype] [[-noextent] | [-extent3D]]
            [-wkt_format WKT1|WKT2|<other_values>]
            [-fielddomain <name>]
            <datasource_name> [<layer> [<layer> ...]]

Description
-----------

The :program:`ogrinfo` program lists various information about an OGR-supported data
source to stdout (the terminal). By executing SQL statements it is also possible to
edit data.

.. program:: ogrinfo

.. include:: options/help_and_help_general.rst

.. include:: options/if.rst

.. option:: -json

    Display the output in json format, conforming to the
    `ogrinfo_output.schema.json <https://github.com/OSGeo/gdal/blob/master/apps/data/ogrinfo_output.schema.json>`__
    schema.

    .. versionadded:: 3.7

.. option:: -ro

    Open the data source in read-only mode.

.. option:: -al

    List all layers (used instead of having to give layer names
    as arguments).
    In the default text output, this also enables listing
    all features, which can be disabled with :option:`-so`.
    In JSON output, -al is implicit, but listing of features must be
    explicitly enabled with :option:`-features`.

.. option:: -rl

    Enable random layer reading mode, i.e. iterate over features in the order
    they are found in the dataset, and not layer per layer. This can be
    significantly faster for some formats (for example OSM, GMLAS).
    -rl cannot be used with -sql.


    .. versionadded:: 2.2

.. option:: -so

    Summary Only: suppress listing of individual features and show only
    summary information like projection, schema, feature count and extents.

.. option:: -features

    Enable listing of features. This has the opposite effect of :option:`-so`.

    This option should be used with caution if using the library function
    :cpp:func:`GDALVectorInfo` and/or :option:`-json`, as the whole output of
    ogrinfo will be built in memory. Consequently, when used on a large
    collection of features, RAM may be exhausted.

    .. versionadded:: 3.7

.. option:: -limit <nb_features>

    .. versionadded:: 3.9

    Limit the number of features per layer.

.. option:: -q

    Quiet verbose reporting of various information, including coordinate
    system, layer schema, extents, and feature count.

.. option:: -where <restricted_where>

    An attribute query in a restricted form of the queries used in the SQL
    `WHERE` statement. Only features matching the attribute query will be
    reported. Starting with GDAL 2.1, the ``@<filename>`` syntax can be used to
    indicate that the content is in the pointed filename.

    Example of ``-where`` and quoting:

    .. code-block:: bash

        -where "\"Corner Point Identifier\" LIKE '%__00_00'"


.. option:: -sql <statement>|@<filename>

    Execute the indicated SQL statement and return the result. Starting with
    GDAL 2.1, the ``@<filename>`` syntax can be used to indicate that the content is
    in the pointed filename (e.g ``@my_select.txt`` where my_select.txt is a file
    in the current directory). Data can also be edited with SQL INSERT, UPDATE,
    DELETE, DROP TABLE, ALTER TABLE etc. Editing capabilities depend on the selected
    dialect with :option:`-dialect`.


.. option:: -dialect <dialect>

    SQL dialect. In some cases can be used to use (unoptimized) :ref:`ogr_sql_dialect` instead
    of the native SQL of an RDBMS by passing the ``OGRSQL`` dialect value.
    The :ref:`sql_sqlite_dialect` can be selected with the ``SQLITE``
    and ``INDIRECT_SQLITE`` dialect values, and this can be used with any datasource.

.. option:: -spat <xmin> <ymin> <xmax> <ymax>

    The area of interest. Only features within the rectangle will be reported.

.. option:: -geomfield <field>

    Name of the geometry field on which the spatial filter operates.

.. option:: -fid <fid>

    If provided, only the feature with this feature id will be reported.
    Operates exclusive of the spatial or attribute queries. Note: if you want
    to select several features based on their feature id, you can also use the
    fact the 'fid' is a special field recognized by OGR SQL. So, `-where "fid in (1,3,5)"`
    would select features 1, 3 and 5.

.. option:: -fields=YES|NO

    If set to ``NO``, the feature dump will not display field values. Default value
    is ``YES``.

.. option:: -fielddomain <domain_name>

    .. versionadded:: 3.3

    Display details about a field domain.

.. option:: -geom=YES|NO|SUMMARY|WKT|ISO_WKT

    If set to ``NO``, the feature dump will not display the geometry. If set to
    ``SUMMARY``, only a summary of the geometry will be displayed. If set to
    ``YES`` or ``ISO_WKT``, the geometry will be reported in full OGC WKT format.
    If set to ``WKT`` the geometry will be reported in legacy ``WKT``. Default
    value is ``YES``. (WKT and ``ISO_WKT`` are available starting with GDAL 2.1,
    which also changes the default to ISO_WKT)

.. option:: -oo <NAME>=<VALUE>

    Dataset open option (format-specific)

.. option:: -nomd

    Suppress metadata printing. Some datasets may contain a lot of metadata
    strings.

.. option:: -listmdd

    List all metadata domains available for the dataset.

.. option:: -mdd <domain>

    Report metadata for the specified domain. ``all`` can be used to report
    metadata in all domains.

.. option:: -nocount

    Suppress feature count printing.

.. option:: -noextent

    Suppress spatial extent printing.

.. option:: -extent3D

    .. versionadded:: 3.9

    Request a 3D extent to be reported (the default is 2D only). Note that this
    operation might be slower than requesting the 2D extent, depending on format
    and driver capabilities.

.. option:: -nogeomtype

    Suppress layer geometry type printing.

    .. versionadded:: 3.1

.. include:: options/formats_vector.rst

.. option:: -wkt_format <format>

    The WKT format used to display the SRS.
    Currently supported values for the ``format`` are:

    ``WKT1``

    ``WKT2`` (latest WKT version, currently *WKT2_2018*)

    ``WKT2_2015``

    ``WKT2_2018``

    .. versionadded:: 3.0.0

.. option:: <datasource_name>

    The data source to open. May be a filename, directory or other virtual
    name. See the OGR Vector Formats list for supported datasources.

.. option:: <layer>

    One or more layer names may be reported.  If no layer names are passed then
    ogrinfo will report a list of available layers (and their layer wide
    geometry type). If layer name(s) are given then their extents, coordinate
    system, feature count, geometry type, schema and all features matching
    query parameters will be reported to the terminal. If no query parameters
    are provided, all features are reported.

Geometries are reported in OGC WKT format.

C API
-----

This utility is also callable from C with :cpp:func:`GDALVectorInfo`.

.. versionadded:: 3.7

Examples
--------

Example of reporting the names of the layers in a NTF file:

.. code-block::

    ogrinfo wrk/SHETLAND_ISLANDS.NTF

    INFO: Open of `wrk/SHETLAND_ISLANDS.NTF'
    using driver `UK .NTF' successful.
    1: BL2000_LINK (Line String)
    2: BL2000_POLY (None)
    3: BL2000_COLLECTIONS (None)
    4: FEATURE_CLASSES (None)

Example of retrieving a summary (``-so``) of a layer without showing details about every single feature:

.. code-block::

    ogrinfo \
      -so \
      natural_earth_vector.gpkg \
      ne_10m_admin_0_antarctic_claim_limit_lines

      INFO: Open of `natural_earth_vector.gpkg'
           using driver `GPKG' successful.

      Layer name: ne_10m_admin_0_antarctic_claim_limit_lines
      Geometry: Line String
      Feature Count: 23
      Extent: (-150.000000, -90.000000) - (160.100000, -60.000000)
      Layer SRS WKT:
      GEOGCS["WGS 84",
          DATUM["WGS_1984",
              SPHEROID["WGS 84",6378137,298.257223563,
                  AUTHORITY["EPSG","7030"]],
              AUTHORITY["EPSG","6326"]],
          PRIMEM["Greenwich",0,
              AUTHORITY["EPSG","8901"]],
          UNIT["degree",0.0174532925199433,
              AUTHORITY["EPSG","9122"]],
          AUTHORITY["EPSG","4326"]]
      FID Column = fid
      Geometry Column = geom
      type: String (15.0)
      scalerank: Integer (0.0)
      featurecla: String (50.0)

Example of retrieving information on a file in JSON format without showing details about every single feature:

.. code-block::

    ogrinfo -json poly.shp


.. code-block:: json

    {
      "description":"autotest/ogr/data/poly.shp",
      "driverShortName":"ESRI Shapefile",
      "driverLongName":"ESRI Shapefile",
      "layers":[
        {
          "name":"poly",
          "metadata":{
            "":{
              "DBF_DATE_LAST_UPDATE":"2018-08-02"
            },
            "SHAPEFILE":{
              "SOURCE_ENCODING":""
            }
          },
          "geometryFields":[
            {
              "name":"",
              "type":"Polygon",
              "nullable":true,
              "extent":[
                478315.53125,
                4762880.5,
                481645.3125,
                4765610.5
              ],
              "coordinateSystem":{
                "wkt":"PROJCRS[\"OSGB36 / British National Grid\",BASEGEOGCRS[\"OSGB36\",DATUM...",
                "projjson":{
                  "$schema":"https://proj.org/schemas/v0.6/projjson.schema.json",
                  "type":"ProjectedCRS",
                  "name":"OSGB36 / British National Grid",
                  "base_crs":{
                    "name":"OSGB36",
                    "datum":{
                      "type":"GeodeticReferenceFrame",
                      "name":"Ordnance Survey of Great Britain 1936",
                      "ellipsoid":{
                        "name":"Airy 1830",
                        "semi_major_axis":6377563.396,
                        "inverse_flattening":299.3249646
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
                      "code":4277
                    }
                  },
                  "conversion":{
                    "name":"British National Grid",
                    "method":{
                      "name":"Transverse Mercator",
                      "id":{
                        "authority":"EPSG",
                        "code":9807
                      }
                    },
                    "parameters":[
                      {
                        "name":"Latitude of natural origin",
                        "value":49,
                        "unit":"degree",
                        "id":{
                          "authority":"EPSG",
                          "code":8801
                        }
                      },
                      {
                        "name":"Longitude of natural origin",
                        "value":-2,
                        "unit":"degree",
                        "id":{
                          "authority":"EPSG",
                          "code":8802
                        }
                      },
                      {
                        "name":"Scale factor at natural origin",
                        "value":0.9996012717,
                        "unit":"unity",
                        "id":{
                          "authority":"EPSG",
                          "code":8805
                        }
                      },
                      {
                        "name":"False easting",
                        "value":400000,
                        "unit":"metre",
                        "id":{
                          "authority":"EPSG",
                          "code":8806
                        }
                      },
                      {
                        "name":"False northing",
                        "value":-100000,
                        "unit":"metre",
                        "id":{
                          "authority":"EPSG",
                          "code":8807
                        }
                      }
                    ]
                  },
                  "coordinate_system":{
                    "subtype":"Cartesian",
                    "axis":[
                      {
                        "name":"Easting",
                        "abbreviation":"E",
                        "direction":"east",
                        "unit":"metre"
                      },
                      {
                        "name":"Northing",
                        "abbreviation":"N",
                        "direction":"north",
                        "unit":"metre"
                      }
                    ]
                  },
                  "scope":"Engineering survey, topographic mapping.",
                  "area":"United Kingdom (UK) - offshore to boundary of UKCS within 49°45...",
                  "bbox":{
                    "south_latitude":49.75,
                    "west_longitude":-9,
                    "north_latitude":61.01,
                    "east_longitude":2.01
                  },
                  "id":{
                    "authority":"EPSG",
                    "code":27700
                  }
                },
                "dataAxisToSRSAxisMapping":[
                  1,
                  2
                ]
              }
            }
          ],
          "featureCount":10,
          "fields":[
            {
              "name":"AREA",
              "type":"Real",
              "width":12,
              "precision":3,
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"EAS_ID",
              "type":"Integer64",
              "width":11,
              "nullable":true,
              "uniqueConstraint":false
            },
            {
              "name":"PRFEDEA",
              "type":"String",
              "width":16,
              "nullable":true,
              "uniqueConstraint":false
            }
          ]
        }
      ],
      "metadata":{
      },
      "domains":{
      },
      "relationships":{
      }
    }


Example of using -q and an attribute query, to restrict the output to
certain features in a layer:

.. code-block:: bash

    ogrinfo -q -ro \
      -where 'GLOBAL_LINK_ID=185878' \
      wrk/SHETLAND_ISLANDS.NTF BL2000_LINK

    Layer name: BL2000_LINK
    OGRFeature(BL2000_LINK):2
      LINE_ID (Integer) = 2
      GEOM_ID (Integer) = 2
      FEAT_CODE (String) = (null)
      GLOBAL_LINK_ID (Integer) = 185878
      TILE_REF (String) = SHETLAND I
      LINESTRING (419832.100 1069046.300,419820.100 1069043.800,...

Example of updating a value of an attribute in a shapefile with SQL by using the SQLite dialect:

.. code-block:: bash

   ogrinfo test.shp -dialect sqlite -sql "update test set attr='bar' where attr='foo'"

Adding a column to an input file:

.. code-block:: bash

   ogrinfo input.shp -sql "ALTER TABLE input ADD fieldX float"

Sometimes there is no input file involved in a calculation. In such cases one may
use the ``:memory:`` input file which is a in-memory empty SQLite file (and the SQLite SQL dialect will be implicitly used).

.. code-block:: bash

    ogrinfo :memory: -sql "SELECT ST_Buffer(ST_GeomFromText('POINT(0 0)'), 1)"

