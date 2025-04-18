.. _gdal_raster_clip:

================================================================================
``gdal raster clip``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Clip a raster dataset.

.. Index:: gdal raster clip

Synopsis
--------

.. program-output:: gdal raster clip --help-doc

Description
-----------

:program:`gdal raster clip` can be used to clip a raster dataset using
georeferenced coordinates.

Either :option:`--bbox` or :option:`--like` must be specified.

The output dataset is in the same SRS as the input one, and the original
resolution is preserved. Bounds are rounded to match whole pixel locations
(i.e. there is no resampling involved)

``clip`` can also be used as a step of :ref:`gdal_raster_pipeline`.

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
    The bounds are expanded if necessary to match input pixel boundaries.
    By default, :program:`gdal raster clip` will produce an error if the bounds indicated
    by :option:`--bbox` are greater than the extents of input dataset. This check can be
    bypassed using :option:`--allow-bbox-outside-source`.

.. option:: --bbox-crs <CRS>

    CRS in which the <xmin>,<ymin>,<xmax>,<ymax> values of :option:`--bbox`
    are expressed. If not specified, it is assumed to be the CRS of the input
    dataset.
    Note that specifying :option:`--bbox-crs` does not cause the raster to be reprojected.
    Instead, the bounds are reprojected from the bbox-crs to the CRS of the
    input dataset.

.. option:: --allow-bbox-outside-source

    If set, allows the bounds indicated by :option:`--bbox` to cover an extent that is greater
    than the input dataset. Output pixels from areas beyond the input extent will be set to
    zero or the NoData value of the input dataset.

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
