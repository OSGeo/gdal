.. _gdalinfo:

================================================================================
gdalinfo
================================================================================

.. only:: html

    Lists information about a raster dataset.

.. Index:: gdalinfo

Synopsis
--------

.. code-block::

    gdalinfo [--help] [--help-general]
             [-json] [-mm] [-stats | -approx_stats] [-hist]
             [-nogcp] [-nomd] [-norat] [-noct] [-nofl] [-nonodata] [-nomask]
             [-checksum] [-listmdd] [-mdd <domain>|all]
             [-proj4] [-wkt_format {WKT1|WKT2|<other_format>}]...
             [-sd <subdataset>] [-oo <NAME>=<VALUE>]... [-if <format>]...
             <datasetname>

Description
-----------

:program:`gdalinfo` program lists various information about a GDAL supported
raster dataset.

The following command line parameters can appear in any order

.. program:: gdalinfo

.. include:: options/help_and_help_general.rst

.. option:: -json

    Display the output in json format. Since GDAL 3.6, this includes key-value
    pairs useful for building a `STAC item
    <https://github.com/radiantearth/stac-spec/blob/v1.0.0/item-spec/item-spec.md>`_
    , including statistics and histograms if ``-stats`` or ``-hist`` flags are
    passed, respectively.

.. option:: -mm

    Force computation of the actual min/max values for each band in the
    dataset.

.. option:: -stats

    Read and display image statistics. Force computation if no
    statistics are stored in an image.

.. option:: -approx_stats

    Read and display image statistics. Force computation if no
    statistics are stored in an image. However, they may be computed
    based on overviews or a subset of all tiles. Useful if you are in a
    hurry and don't want precise stats.

.. option:: -hist

    Report histogram information for all bands.

.. option:: -nogcp

    Suppress ground control points list printing. It may be useful for
    datasets with huge amount of GCPs, such as L1B AVHRR or HDF4 MODIS
    which contain thousands of them.

.. option:: -nomd

    Suppress metadata printing. Some datasets may contain a lot of
    metadata strings.

.. option:: -norat

    Suppress printing of raster attribute table.

.. option:: -noct

    Suppress printing of color table.

.. option:: -nonodata

    .. versionadded:: 3.10

    Suppress nodata printing. Implies :option:`-nomask`.

    Can be useful for example when querying a remove GRIB2 dataset that has an
    index .idx side-car file, together with :option:`-nomd`

.. option:: -nomask

    .. versionadded:: 3.10

    Suppress band mask printing. Is implied if :option:`-nonodata` is specified.

.. option:: -checksum

    Force computation of the checksum for each band in the dataset.

.. option:: -listmdd

    List all metadata domains available for the dataset.

.. option:: -mdd <domain>|all

    adds metadata using:

    ``domain`` Report metadata for the specified domain.

    ``all`` Report metadata for all domains.

.. option:: -nofl

    Only display the first file of the file list.

.. option:: -wkt_format WKT1|WKT2|WKT2_2015|WKT2_2018|WKT2_2019

    WKT format used to display the SRS.
    Currently the supported values are:

    ``WKT1``

    ``WKT2`` (latest WKT version, currently *WKT2_2019*)

    ``WKT2_2015``

    ``WKT2_2018`` (deprecated)

    ``WKT2_2019``

    .. versionadded:: 3.0.0

.. option:: -sd <n>

    If the input dataset contains several subdatasets read and display
    a subdataset with specified ``n`` number (starting from 1).
    This is an alternative of giving the full subdataset name.

.. option:: -proj4

    Report a PROJ.4 string corresponding to the file's coordinate system.

.. option:: -oo <NAME>=<VALUE>

    Dataset open option (format specific).

.. include:: options/if.rst


The gdalinfo will report all of the following (if known):

-  The format driver used to access the file.
-  Raster size (in pixels and lines).
-  The coordinate system for the file (in OGC WKT).
-  The geotransform associated with the file (rotational coefficients
   are currently not reported).
-  Corner coordinates in georeferenced, and if possible lat/long based
   on the full geotransform (but not GCPs).
-  Ground control points.
-  File wide (including subdatasets) metadata.
-  Band data types.
-  Band color interpretations.
-  Band block size.
-  Band descriptions.
-  Band min/max values (internally known and possibly computed).
-  Band checksum (if computation asked).
-  Band NODATA value.
-  Band overview resolutions available.
-  Band unit type (i.e.. "meters" or "feet" for elevation bands).
-  Band pseudo-color tables.

C API
-----

This utility is also callable from C with :cpp:func:`GDALInfo`.

.. versionadded:: 2.1

Example
-------

.. code-block::

    gdalinfo ~/openev/utm.tif
    Driver: GTiff/GeoTIFF
    Size is 512, 512
    Coordinate System is:
    PROJCS["NAD27 / UTM zone 11N",
        GEOGCS["NAD27",
            DATUM["North_American_Datum_1927",
                SPHEROID["Clarke 1866",6378206.4,294.978698213901]],
            PRIMEM["Greenwich",0],
            UNIT["degree",0.0174532925199433]],
        PROJECTION["Transverse_Mercator"],
        PARAMETER["latitude_of_origin",0],
        PARAMETER["central_meridian",-117],
        PARAMETER["scale_factor",0.9996],
        PARAMETER["false_easting",500000],
        PARAMETER["false_northing",0],
        UNIT["metre",1]]
    Origin = (440720.000000,3751320.000000)
    Pixel Size = (60.000000,-60.000000)
    Corner Coordinates:
    Upper Left  (  440720.000, 3751320.000) (117d38'28.21"W, 33d54'8.47"N)
    Lower Left  (  440720.000, 3720600.000) (117d38'20.79"W, 33d37'31.04"N)
    Upper Right (  471440.000, 3751320.000) (117d18'32.07"W, 33d54'13.08"N)
    Lower Right (  471440.000, 3720600.000) (117d18'28.50"W, 33d37'35.61"N)
    Center      (  456080.000, 3735960.000) (117d28'27.39"W, 33d45'52.46"N)
    Band 1 Block=512x16 Type=Byte, ColorInterp=Gray

For corner coordinates formatted as decimal degree instead of the above degree, minute, second, inspect the ``wgs84Extent`` member of gdalinfo -json:

Example of JSON output with ``gdalinfo -json byte.tif``

.. code-block:: json

    {
      "description":"byte.tif",
      "driverShortName":"GTiff",
      "driverLongName":"GeoTIFF",
      "files":[
        "byte.tif"
      ],
      "size":[
        20,
        20
      ],
      "coordinateSystem":{
        "wkt":"PROJCRS[\"NAD27 / UTM zone 11N\",\n    BASEGEOGCRS[\"NAD27\",\n        DATUM[\"North American Datum 1927\",\n            ELLIPSOID[\"Clarke 1866\",6378206.4,294.978698213898,\n                LENGTHUNIT[\"metre\",1]]],\n        PRIMEM[\"Greenwich\",0,\n            ANGLEUNIT[\"degree\",0.0174532925199433]],\n        ID[\"EPSG\",4267]],\n    CONVERSION[\"UTM zone 11N\",\n        METHOD[\"Transverse Mercator\",\n            ID[\"EPSG\",9807]],\n        PARAMETER[\"Latitude of natural origin\",0,\n            ANGLEUNIT[\"degree\",0.0174532925199433],\n            ID[\"EPSG\",8801]],\n        PARAMETER[\"Longitude of natural origin\",-117,\n            ANGLEUNIT[\"degree\",0.0174532925199433],\n            ID[\"EPSG\",8802]],\n        PARAMETER[\"Scale factor at natural origin\",0.9996,\n            SCALEUNIT[\"unity\",1],\n            ID[\"EPSG\",8805]],\n        PARAMETER[\"False easting\",500000,\n            LENGTHUNIT[\"metre\",1],\n            ID[\"EPSG\",8806]],\n        PARAMETER[\"False northing\",0,\n            LENGTHUNIT[\"metre\",1],\n            ID[\"EPSG\",8807]]],\n    CS[Cartesian,2],\n        AXIS[\"(E)\",east,\n            ORDER[1],\n            LENGTHUNIT[\"metre\",1]],\n        AXIS[\"(N)\",north,\n            ORDER[2],\n            LENGTHUNIT[\"metre\",1]],\n    USAGE[\n        SCOPE[\"Engineering survey, topographic mapping.\"],\n        AREA[\"North America - between 120°W and 114°W - onshore. Canada - Alberta; British Columbia; Northwest Territories; Nunavut. Mexico. United States (USA) - California; Idaho; Nevada; Oregon; Washington.\"],\n        BBOX[26.93,-120,78.13,-114]],\n    ID[\"EPSG\",26711]]",
        "dataAxisToSRSAxisMapping":[
          1,
          2
        ]
      },
      "geoTransform":[
        440720.0,
        60.0,
        0.0,
        3751320.0,
        0.0,
        -60.0
      ],
      "metadata":{
        "":{
          "AREA_OR_POINT":"Area"
        },
        "IMAGE_STRUCTURE":{
          "INTERLEAVE":"BAND"
        }
      },
      "cornerCoordinates":{
        "upperLeft":[
          440720.0,
          3751320.0
        ],
        "lowerLeft":[
          440720.0,
          3750120.0
        ],
        "lowerRight":[
          441920.0,
          3750120.0
        ],
        "upperRight":[
          441920.0,
          3751320.0
        ],
        "center":[
          441320.0,
          3750720.0
        ]
      },
      "wgs84Extent":{
        "type":"Polygon",
        "coordinates":[
          [
            [
              -117.6420428,
              33.9023684
            ],
            [
              -117.6419617,
              33.8915461
            ],
            [
              -117.6289846,
              33.8916131
            ],
            [
              -117.629064,
              33.9024353
            ],
            [
              -117.6420428,
              33.9023684
            ]
          ]
        ]
      },
      "bands":[
        {
          "band":1,
          "block":[
            20,
            20
          ],
          "type":"Byte",
          "colorInterpretation":"Gray",
          "metadata":{
          }
        }
      ],
      "stac":{
        "proj:shape":[
          20,
          20
        ],
        "proj:wkt2":"PROJCRS[\"NAD27 / UTM zone 11N\",\n    BASEGEOGCRS[\"NAD27\",\n        DATUM[\"North American Datum 1927\",\n            ELLIPSOID[\"Clarke 1866\",6378206.4,294.978698213898,\n                LENGTHUNIT[\"metre\",1]]],\n        PRIMEM[\"Greenwich\",0,\n            ANGLEUNIT[\"degree\",0.0174532925199433]],\n        ID[\"EPSG\",4267]],\n    CONVERSION[\"UTM zone 11N\",\n        METHOD[\"Transverse Mercator\",\n            ID[\"EPSG\",9807]],\n        PARAMETER[\"Latitude of natural origin\",0,\n            ANGLEUNIT[\"degree\",0.0174532925199433],\n            ID[\"EPSG\",8801]],\n        PARAMETER[\"Longitude of natural origin\",-117,\n            ANGLEUNIT[\"degree\",0.0174532925199433],\n            ID[\"EPSG\",8802]],\n        PARAMETER[\"Scale factor at natural origin\",0.9996,\n            SCALEUNIT[\"unity\",1],\n            ID[\"EPSG\",8805]],\n        PARAMETER[\"False easting\",500000,\n            LENGTHUNIT[\"metre\",1],\n            ID[\"EPSG\",8806]],\n        PARAMETER[\"False northing\",0,\n            LENGTHUNIT[\"metre\",1],\n            ID[\"EPSG\",8807]]],\n    CS[Cartesian,2],\n        AXIS[\"(E)\",east,\n            ORDER[1],\n            LENGTHUNIT[\"metre\",1]],\n        AXIS[\"(N)\",north,\n            ORDER[2],\n            LENGTHUNIT[\"metre\",1]],\n    USAGE[\n        SCOPE[\"Engineering survey, topographic mapping.\"],\n        AREA[\"North America - between 120°W and 114°W - onshore. Canada - Alberta; British Columbia; Northwest Territories; Nunavut. Mexico. United States (USA) - California; Idaho; Nevada; Oregon; Washington.\"],\n        BBOX[26.93,-120,78.13,-114]],\n    ID[\"EPSG\",26711]]",
        "proj:epsg":26711,
        "proj:projjson":{
          "$schema":"https://proj.org/schemas/v0.6/projjson.schema.json",
          "type":"ProjectedCRS",
          "name":"NAD27 / UTM zone 11N",
          "base_crs":{
            "name":"NAD27",
            "datum":{
              "type":"GeodeticReferenceFrame",
              "name":"North American Datum 1927",
              "ellipsoid":{
                "name":"Clarke 1866",
                "semi_major_axis":6378206.4,
                "semi_minor_axis":6356583.8
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
              "code":4267
            }
          },
          "conversion":{
            "name":"UTM zone 11N",
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
                "value":0,
                "unit":"degree",
                "id":{
                  "authority":"EPSG",
                  "code":8801
                }
              },
              {
                "name":"Longitude of natural origin",
                "value":-117,
                "unit":"degree",
                "id":{
                  "authority":"EPSG",
                  "code":8802
                }
              },
              {
                "name":"Scale factor at natural origin",
                "value":0.9996,
                "unit":"unity",
                "id":{
                  "authority":"EPSG",
                  "code":8805
                }
              },
              {
                "name":"False easting",
                "value":500000,
                "unit":"metre",
                "id":{
                  "authority":"EPSG",
                  "code":8806
                }
              },
              {
                "name":"False northing",
                "value":0,
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
          "area":"North America - between 120°W and 114°W - onshore. Canada - Alberta; British Columbia; Northwest Territories; Nunavut. Mexico. United States (USA) - California; Idaho; Nevada; Oregon; Washington.",
          "bbox":{
            "south_latitude":26.93,
            "west_longitude":-120,
            "north_latitude":78.13,
            "east_longitude":-114
          },
          "id":{
            "authority":"EPSG",
            "code":26711
          }
        },
        "proj:transform":[
          440720.0,
          60.0,
          0.0,
          3751320.0,
          0.0,
          -60.0
        ],
        "raster:bands":[
          {
            "data_type":"uint8"
          }
        ],
        "eo:bands":[
          {
            "name":"b1",
            "description":"Gray"
          }
        ]
      }
    }
