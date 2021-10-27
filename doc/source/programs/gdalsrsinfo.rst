.. _gdalsrsinfo:

================================================================================
gdalsrsinfo
================================================================================

.. only:: html

    Lists info about a given SRS in number of formats (WKT, PROJ.4, etc.)

.. Index:: gdalsrsinfo

Synopsis
--------

.. code-block::

    Usage: gdalsrsinfo [--single-line] [-V] [-e][-o <out_type>] <srs_def>

Description
-----------

The :program:`gdalsrsinfo` utility reports information about a given SRS from one of the following:

- The filename of a dataset supported by GDAL/OGR which contains SRS information
- Any of the usual GDAL/OGR forms (complete WKT, PROJ.4, EPSG:n or a file containing the SRS)


.. program:: gdalsrsinfo

.. option:: --single-line

    Print WKT on single line

.. option:: -V

    Validate SRS

.. option:: -e

    Search for EPSG number(s) corresponding to SRS

.. option:: -o <out_type>

    Output types:

    - ``default``: proj4 and wkt (default option)
    - ``all``: all options available
    - ``wkt_all``: all wkt options available
    - ``PROJJSON``: PROJJSON string (GDAL >= 3.1 and PROJ >= 6.2)
    - ``proj4``: PROJ.4 string
    - ``wkt1``: OGC WKT format (full)
    - ``wkt_simple``: OGC WKT 1 (simplified)
    - ``wkt_noct``: OGC WKT 1 (without OGC CT params)
    - ``wkt_esri``: ESRI WKT format
    - ``wkt``: Latest WKT version supported, currently wkt2_2018
    - ``wkt2``: Latest WKT2 version supported, currently wkt2_2018
    - ``wkt2_2015``: OGC WKT2:2015
    - ``wkt2_2018``: OGC WKT2:2018
    - ``mapinfo``: Mapinfo style CoordSys format
    - ``xml``: XML format (GML based)

.. option:: <srs_def>

    may be the filename of a dataset supported by GDAL/OGR from which to extract SRS information
    OR any of the usual GDAL/OGR forms (complete WKT, PROJ.4, EPSG:n or a file containing the SRS)

Example
-------

::

    $ gdalsrsinfo EPSG:4326

    PROJ.4 : +proj=longlat +datum=WGS84 +no_defs

    OGC WKT :
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

::

    $ gdalsrsinfo -o proj4 osr/data/lcc_esri.prj
    '+proj=lcc +lat_1=34.33333333333334 +lat_2=36.16666666666666 +lat_0=33.75 +lon_0=-79 +x_0=609601.22 +y_0=0 +datum=NAD83 +units=m +no_defs '
    \endverbatim

::

    $ gdalsrsinfo -o proj4 landsat.tif
    PROJ.4 : '+proj=utm +zone=19 +south +datum=WGS84 +units=m +no_defs '

::

    $ gdalsrsinfo  -o wkt "EPSG:32722"

    PROJCRS["WGS 84 / UTM zone 22S",
        BASEGEOGCRS["WGS 84",
            DATUM["World Geodetic System 1984",
                ELLIPSOID["WGS 84",6378137,298.257223563,
                    LENGTHUNIT["metre",1]]],
            PRIMEM["Greenwich",0,
                ANGLEUNIT["degree",0.0174532925199433]]],
        CONVERSION["UTM zone 22S",
            METHOD["Transverse Mercator",
                ID["EPSG",9807]],
            PARAMETER["Latitude of natural origin",0,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8801]],
            PARAMETER["Longitude of natural origin",-51,
                ANGLEUNIT["degree",0.0174532925199433],
                ID["EPSG",8802]],
            PARAMETER["Scale factor at natural origin",0.9996,
                SCALEUNIT["unity",1],
                ID["EPSG",8805]],
            PARAMETER["False easting",500000,
                LENGTHUNIT["metre",1],
                ID["EPSG",8806]],
            PARAMETER["False northing",10000000,
                LENGTHUNIT["metre",1],
                ID["EPSG",8807]]],
        CS[Cartesian,2],
            AXIS["(E)",east,
                ORDER[1],
                LENGTHUNIT["metre",1]],
            AXIS["(N)",north,
                ORDER[2],
                LENGTHUNIT["metre",1]],
        USAGE[
            SCOPE["unknown"],
            AREA["World - S hemisphere - 54°W to 48°W - by country"],
            BBOX[-80,-54,0,-48]],
        ID["EPSG",32722]]

::

    $ gdalsrsinfo -o wkt_all "EPSG:4322"
    OGC WKT 1:
    GEOGCS["WGS 72",
        DATUM["World_Geodetic_System_1972",
            SPHEROID["WGS 72",6378135,298.26,
                AUTHORITY["EPSG","7043"]],
            TOWGS84[0,0,4.5,0,0,0.554,0.2263],
            AUTHORITY["EPSG","6322"]],
        PRIMEM["Greenwich",0,
            AUTHORITY["EPSG","8901"]],
        UNIT["degree",0.0174532925199433,
            AUTHORITY["EPSG","9122"]],
        AXIS["Latitude",NORTH],
        AXIS["Longitude",EAST],
        AUTHORITY["EPSG","4322"]]

    OGC WKT2:2015 :
    BOUNDCRS[
        SOURCECRS[
            GEODCRS["WGS 72",
                DATUM["World Geodetic System 1972",
                    ELLIPSOID["WGS 72",6378135,298.26,
                        LENGTHUNIT["metre",1]]],
                PRIMEM["Greenwich",0,
                    ANGLEUNIT["degree",0.0174532925199433]],
                CS[ellipsoidal,2],
                    AXIS["geodetic latitude (Lat)",north,
                        ORDER[1],
                        ANGLEUNIT["degree",0.0174532925199433]],
                    AXIS["geodetic longitude (Lon)",east,
                        ORDER[2],
                        ANGLEUNIT["degree",0.0174532925199433]],
                AREA["World"],
                BBOX[-90,-180,90,180],
                ID["EPSG",4322]]],
        TARGETCRS[
            GEODCRS["WGS 84",
                DATUM["World Geodetic System 1984",
                    ELLIPSOID["WGS 84",6378137,298.257223563,
                        LENGTHUNIT["metre",1]]],
                PRIMEM["Greenwich",0,
                    ANGLEUNIT["degree",0.0174532925199433]],
                CS[ellipsoidal,2],
                    AXIS["latitude",north,
                        ORDER[1],
                        ANGLEUNIT["degree",0.0174532925199433]],
                    AXIS["longitude",east,
                        ORDER[2],
                        ANGLEUNIT["degree",0.0174532925199433]],
                ID["EPSG",4326]]],
        ABRIDGEDTRANSFORMATION["WGS 72 to WGS 84 (1)",
            METHOD["Position Vector transformation (geog2D domain)",
                ID["EPSG",9606]],
            PARAMETER["X-axis translation",0,
                ID["EPSG",8605]],
            PARAMETER["Y-axis translation",0,
                ID["EPSG",8606]],
            PARAMETER["Z-axis translation",4.5,
                ID["EPSG",8607]],
            PARAMETER["X-axis rotation",0,
                ID["EPSG",8608]],
            PARAMETER["Y-axis rotation",0,
                ID["EPSG",8609]],
            PARAMETER["Z-axis rotation",0.554,
                ID["EPSG",8610]],
            PARAMETER["Scale difference",1.0000002263,
                ID["EPSG",8611]],
            AREA["World"],
            BBOX[-90,-180,90,180],
            ID["EPSG",1237]]]

    OGC WKT2:2018 :
    BOUNDCRS[
        SOURCECRS[
            GEOGCRS["WGS 72",
                DATUM["World Geodetic System 1972",
                    ELLIPSOID["WGS 72",6378135,298.26,
                        LENGTHUNIT["metre",1]]],
                PRIMEM["Greenwich",0,
                    ANGLEUNIT["degree",0.0174532925199433]],
                CS[ellipsoidal,2],
                    AXIS["geodetic latitude (Lat)",north,
                        ORDER[1],
                        ANGLEUNIT["degree",0.0174532925199433]],
                    AXIS["geodetic longitude (Lon)",east,
                        ORDER[2],
                        ANGLEUNIT["degree",0.0174532925199433]],
                USAGE[
                    SCOPE["unknown"],
                    AREA["World"],
                    BBOX[-90,-180,90,180]],
                ID["EPSG",4322]]],
        TARGETCRS[
            GEOGCRS["WGS 84",
                DATUM["World Geodetic System 1984",
                    ELLIPSOID["WGS 84",6378137,298.257223563,
                        LENGTHUNIT["metre",1]]],
                PRIMEM["Greenwich",0,
                    ANGLEUNIT["degree",0.0174532925199433]],
                CS[ellipsoidal,2],
                    AXIS["latitude",north,
                        ORDER[1],
                        ANGLEUNIT["degree",0.0174532925199433]],
                    AXIS["longitude",east,
                        ORDER[2],
                        ANGLEUNIT["degree",0.0174532925199433]],
                ID["EPSG",4326]]],
        ABRIDGEDTRANSFORMATION["WGS 72 to WGS 84 (1)",
            METHOD["Position Vector transformation (geog2D domain)",
                ID["EPSG",9606]],
            PARAMETER["X-axis translation",0,
                ID["EPSG",8605]],
            PARAMETER["Y-axis translation",0,
                ID["EPSG",8606]],
            PARAMETER["Z-axis translation",4.5,
                ID["EPSG",8607]],
            PARAMETER["X-axis rotation",0,
                ID["EPSG",8608]],
            PARAMETER["Y-axis rotation",0,
                ID["EPSG",8609]],
            PARAMETER["Z-axis rotation",0.554,
                ID["EPSG",8610]],
            PARAMETER["Scale difference",1.0000002263,
                ID["EPSG",8611]],
            USAGE[
                SCOPE["unknown"],
                AREA["World"],
                BBOX[-90,-180,90,180]],
            ID["EPSG",1237]]]

    OGC WKT 1 (simple) :
    GEOGCS["WGS 72",
        DATUM["World_Geodetic_System_1972",
            SPHEROID["WGS 72",6378135,298.26],
            TOWGS84[0,0,4.5,0,0,0.554,0.2263]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]]

    OGC WKT 1 (no CT) :
    GEOGCS["WGS 72",
        DATUM["World_Geodetic_System_1972",
            SPHEROID["WGS 72",6378135,298.26]],
        PRIMEM["Greenwich",0],
        UNIT["degree",0.0174532925199433]]

    ESRI WKT :
    GEOGCS["GCS_WGS_1972",
        DATUM["D_WGS_1972",
            SPHEROID["WGS_1972",6378135.0,298.26]],
        PRIMEM["Greenwich",0.0],
        UNIT["Degree",0.0174532925199433]]
