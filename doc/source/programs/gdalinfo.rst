.. _gdalinfo:

================================================================================
gdalinfo
================================================================================

.. only:: html

    Lists information about a raster dataset.

.. Index:: gdalinfo

Synopsis
--------

.. program-output:: gdalinfo --help-doc

Description
-----------

:program:`gdalinfo` program lists various information about a GDAL supported
raster dataset.

The following command line parameters can appear in any order

.. tip:: Equivalent in new "gdal" command line interface:

    See :ref:`gdal_raster_info`.

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
    hurry and don't need precise stats.

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

Examples
--------

.. example::
   :title: Default output format

   .. command-output:: gdalinfo utmsmall.tif
      :cwd: ../../../autotest/gcore/data

.. example::
   :title: JSON output

   For corner coordinates formatted as decimal degree instead of the above degree, minute, second, inspect the ``wgs84Extent`` member of ``gdalinfo -json``:

   Example of JSON output with ``gdalinfo -json utmsmall.tif``:

   .. program-output:: gdalinfo -json utmsmall.tif
      :cwd: ../../../autotest/gcore/data
      :language: json
