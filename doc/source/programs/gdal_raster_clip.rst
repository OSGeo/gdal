.. _gdal_raster_clip_subcommand:

================================================================================
"gdal raster clip" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Clip a raster dataset.

.. Index:: gdal raster clip

Synopsis
--------

.. code-block::

    Usage: gdal raster clip [OPTIONS] <INPUT> <OUTPUT>

    Clip a raster dataset.

    Positional arguments:
      -i, --input <INPUT>                                  Input raster dataset [required]
      -o, --output <OUTPUT>                                Output raster dataset [required]

    Common Options:
      -h, --help                                           Display help message and exit
      --version                                            Display GDAL version and exit
      --json-usage                                         Display usage as JSON document and exit
      --drivers                                            Display driver list as JSON document and exit
      --config <KEY>=<VALUE>                               Configuration option [may be repeated]
      --progress                                           Display progress bar

    Options:
      -f, --of, --format, --output-format <OUTPUT-FORMAT>  Output format
      --co, --creation-option <KEY>=<VALUE>                Creation option [may be repeated]
      --overwrite                                          Whether overwriting existing output is allowed
      --bbox <BBOX>                                        Clipping bounding box as xmin,ymin,xmax,ymax
                                                           Mutually exclusive with --like
      --bbox-crs <BBOX-CRS>                                CRS of clipping bounding box
      --like <DATASET>                                     Raster dataset to use as a template for bounds
                                                           Mutually exclusive with --bbox

    Advanced Options:
      --if, --input-format <INPUT-FORMAT>                  Input formats [may be repeated]
      --oo, --open-option <KEY=VALUE>                      Open options [may be repeated]


Description
-----------

:program:`gdal raster clip` can be used to clip a raster dataset using
georeferenced coordinates.

Either :option:`--bbox` or :option:`--like` must be specified.

The output dataset is in the same SRS as the input one, and the original
resolution is preserved. Bounds are rounded to match whole pixel locations
(i.e. there is no resampling involved)

``clip`` can also be used as a step of :ref:`gdal_raster_pipeline_subcommand`.

Standard options
++++++++++++++++

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

.. option:: --bbox <xmin>,<ymin>,<xmax>,<ymax>

    Bounds to which to clip the dataset. They are assumed to be in the CRS of
    the input dataset, unless :option:`--bbox-crs` is specified.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.
    The bounds are expended if necessary to match input pixel boundaries.

.. option:: --bbox-crs <CRS>

    CRS in which the <xmin>,<ymin>,<xmax>,<ymax> values of :option:`--bbox`
    are expressed. If not specified, it is assumed to be the CRS of the input
    dataset.
    Not that specifying --bbox-crs does not involve doing raster reprojection.
    Instead, the bounds are reprojected from the bbox-crs to the CRS of the
    input dataset.

.. option:: --like <DATASET>

    Raster dataset to use as a template for bounds, forming a rectangular shape
    following the geotransformation matrix (and thus potentially including
    nodata collar).
    This option is mutually
    exclusive with :option:`--bbox` and :option:`--bbox-crs`.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

Examples
--------

.. example::
   :title: Clip a GeoTIFF file to the bounding box from longitude 2, latitude 49, to longitude 3, latitude 50 in WGS 84

   .. code-block:: bash

        $ gdal raster clip --bbox=2,49,3,50 --bbox-crs=EPSG:4326 in.tif out.tif --overwrite

.. example::
   :title: Clip a GeoTIFF file using the bounds of :file:`reference.tif`

   .. code-block:: bash

        $ gdal raster clip --like=reference.tif in.tif out.tif --overwrite
