.. _gdal_crs_info:

.. program:: gdal_crs_info

================================================================================
``gdal crs info``
================================================================================

.. versionadded:: 3.14

.. only:: html

    Returns information about a coordinate reference system (CRS).

.. Index:: gdal crs info

:program:`gdal crs info` reports information about a coordinate reference system (CRS).

Synopsis
--------

.. program-output:: gdal crs info --help-doc

Options
-------

.. option:: --input-crs, -s <INPUT-CRS>

    Set input coordinate reference system (CRS).

    The coordinate reference systems that can be passed are anything supported by the
    OGRSpatialReference.SetFromUserInput() call, which includes EPSG Projected,
    Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
    PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.

.. option:: -o, --output-type <OUTPUT-TYPE>

    Set output type. OUTPUT-TYPE=default|all|wkt_all|wkt|projjson|proj4|wkt1|wkt_esri|wkt2|mapinfo|xml (default: default)
    Default output type is WKT2 + PROJ4. If OUTPUT-TYPE=all, all available representations will be printed.

.. option:: --validate

    Validate the CRS and report any issues found.

.. option:: --identify, -i

    Search for EPSG number(s) corresponding to the CRS.

.. option:: --single-line

    Print WKT or PROJJSON on a single line.


.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Identify a CRS from its WKT representation and return its PROJ.4 representation

   .. code-block:: console

       $ gdal crs info -o proj4 --input-crs "PROJCS[\"WGS 84 / UTM zone 33N\",GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"4326\"]],PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",15],PARAMETER[\"scale_factor\",0.9996],PARAMETER[\"false_easting\",500000],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],AXIS[\"Easting\",EAST],AXIS[\"Northing\",NORTH],AUTHORITY[\"EPSG\",\"32633\"]]"

       +proj=utm +zone=33 +datum=WGS84 +units=m +no_defs


.. example::
   :title: Print information about a CRS in PROJJSON format

   .. code-block:: console

       $ gdal crs info EPSG:4326 -o projjson
       PROJJSON :

   .. code-block:: json

     {
            "$schema": "https://proj.org/schemas/v0.7/projjson.schema.json",
            "type": "GeographicCRS",
            "name": "WGS 84",
            "datum_ensemble": {
                "name": "World Geodetic System 1984 ensemble",
                "members": [
                {
                    "name": "World Geodetic System 1984 (Transit)",
                    "id": {
                    "authority": "EPSG",
                    "code": 1166
                    }
                },
                {
                    "name": "World Geodetic System 1984 (G730)",
                    "id": {
                    "authority": "EPSG",
                    "code": 1152
                    }
                },
                {
                    "name": "World Geodetic System 1984 (G873)",
                    "id": {
                    "authority": "EPSG",
                    "code": 1153
                    }
                },
                {
                    "name": "World Geodetic System 1984 (G1150)",
                    "id": {
                    "authority": "EPSG",
                    "code": 1154
                    }
                },
                {
                    "name": "World Geodetic System 1984 (G1674)",
                    "id": {
                    "authority": "EPSG",
                    "code": 1155
                    }
                },
                {
                    "name": "World Geodetic System 1984 (G1762)",
                    "id": {
                    "authority": "EPSG",
                    "code": 1156
                    }
                },
                {
                    "name": "World Geodetic System 1984 (G2139)",
                    "id": {
                    "authority": "EPSG",
                    "code": 1309
                    }
                },
                {
                    "name": "World Geodetic System 1984 (G2296)",
                    "id": {
                    "authority": "EPSG",
                    "code": 1383
                    }
                }
                ],
                "ellipsoid": {
                "name": "WGS 84",
                "semi_major_axis": 6378137,
                "inverse_flattening": 298.257223563
                },
                "accuracy": "2.0",
                "id": {
                "authority": "EPSG",
                "code": 6326
                }
            },
            "coordinate_system": {
                "subtype": "ellipsoidal",
                "axis": [
                {
                    "name": "Geodetic latitude",
                    "abbreviation": "Lat",
                    "direction": "north",
                    "unit": "degree"
                },
                {
                    "name": "Geodetic longitude",
                    "abbreviation": "Lon",
                    "direction": "east",
                    "unit": "degree"
                }
                ]
            },
            "scope": "Horizontal component of 3D system.",
            "area": "World.",
            "bbox": {
                "south_latitude": -90,
                "west_longitude": -180,
                "north_latitude": 90,
                "east_longitude": 180
            },
            "id": {
                "authority": "EPSG",
                "code": 4326
            }
      }
