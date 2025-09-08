.. _gdal_raster_info:

================================================================================
``gdal raster info``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Get information on a raster dataset.

.. Index:: gdal raster info

Synopsis
--------

.. program-output:: gdal raster info --help-doc

Description
-----------

:program:`gdal raster info` lists various information about a GDAL supported
raster dataset.

The following items will be reported (when known):

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

Starting with GDAL 3.12, :program:`gdal raster info` can be used as the last
step of a pipeline.

The following options are available:

Standard options
++++++++++++++++

.. option:: -f, --of, --format, --output-format json|text

    Which output format to use. Default is JSON, and starting with GDAL 3.12,
    text when invoked from command line.

.. option:: --mm, --min-max

    Force computation of the actual min/max values for each band in the
    dataset.

.. option:: --stats

    Read and display image statistics. Force computation if no
    statistics are stored in an image.

.. option:: --approx-stats

    Read and display image statistics. Force computation if no
    statistics are stored in an image. However, they may be computed
    based on overviews or a subset of all tiles. Useful if you are in a
    hurry and don't need precise stats.

.. option:: --hist

    Report histogram information for all bands.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

.. option:: --no-gcp

    Suppress ground control points list printing. It may be useful for
    datasets with huge amount of GCPs, such as L1B AVHRR or HDF4 MODIS
    which contain thousands of them.

.. option:: --no-md

    Suppress metadata printing. Some datasets may contain a lot of
    metadata strings.

.. option:: --no-ct

    Suppress printing of color table.

.. option:: --no-rat

    Suppress printing of raster attribute table.

.. option:: --no-fl

    Only display the first file of the file list.

.. option:: --checksum

    Force computation of the checksum for each band in the dataset.

.. option:: --list-mdd

    List all metadata domains available for the dataset.

.. option:: --mdd <domain>|all

    adds metadata using:

    ``domain`` Report metadata for the specified domain.

    ``all`` Report metadata for all domains.

Esoteric options
++++++++++++++++

.. option:: --no-nodata

    Suppress nodata printing. Implies :option:`--no-mask`.

    Can be useful for example when querying a remove GRIB2 dataset that has an
    index .idx side-car file, together with :option:`--no-md`

.. option:: --no-mask

    Suppress band mask printing. Is implied if :option:`--no-nodata` is specified.

.. option:: --subdataset <n>

    If the input dataset contains several subdatasets read and display
    a subdataset with specified ``n`` number (starting from 1).
    This is an alternative of giving the full subdataset name.

Examples
--------

.. example::
   :title: Getting information on the file :file:`utmsmall.tif` as JSON output

   .. command-output:: gdal raster info utmsmall.tif
      :cwd: ../../data

.. example::
   :title: Getting information on the file :file:`utmsmall.tif` as text output, including statistics

   .. command-output:: gdal raster info --format=text --stats utmsmall.tif
      :cwd: ../../data/with_stats
