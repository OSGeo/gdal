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

    gdalinfo [--help-general] [-json] [-mm] [-stats | -approx_stats] [-hist] [-nogcp] [-nomd]
             [-norat] [-noct] [-nofl] [-checksum] [-proj4]
             [-listmdd] [-mdd domain|`all`]* [-wkt_format WKT1|WKT2|...]
             [-sd subdataset] [-oo NAME=VALUE]* [-if format]* datasetname

Description
-----------

:program:`gdalinfo` program lists various information about a GDAL supported
raster dataset.

The following command line parameters can appear in any order

.. program:: gdalinfo

.. option:: -json

    Display the output in json format.

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

.. option:: -wkt_format WKT1|WKT2|WKT2_2015|WKT2_2018

    WKT format used to display the SRS.
    Currently the supported values are:

    ``WKT1``

    ``WKT2`` (latest WKT version, currently *WKT2_2018*)

    ``WKT2_2015``

    ``WKT2_2018``

    .. versionadded:: 3.0.0

.. option:: -sd <n>

    If the input dataset contains several subdatasets read and display
    a subdataset with specified ``n`` number (starting from 1).
    This is an alternative of giving the full subdataset name.

.. option:: -proj4

    Report a PROJ.4 string corresponding to the file's coordinate system.

.. option:: -oo <NAME=VALUE>

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
