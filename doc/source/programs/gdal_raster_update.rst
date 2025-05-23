.. _gdal_raster_update:

================================================================================
``gdal raster update``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Update the destination raster with the content of the input one.

.. Index:: gdal raster update

Synopsis
--------

.. program-output:: gdal raster update --help-doc

Description
-----------

:program:`gdal raster update` can be used to update an existing target raster dataset
with the pixel values of the source raster, doing reprojection if the source
and target datasets do not have the same coordinate reference systems.
The extent, size, resolution or coordinate reference system of the target dataset
are not modified by this operation.

Overviews are updated by this command (using :ref:`gdal_raster_overview_refresh`),
unless :option:`--no-update-overviews` is specified.

Standard options
++++++++++++++++

.. include:: gdal_options/warp_resampling.rst

.. option:: --geometry <WKT_or_GeoJSON>

    Geometry as a WKT or GeoJSON string of a polygon (or multipolygon) to which
    to restrict the update.
    If the input geometry is GeoJSON, its CRS is assumed to be WGS84, unless there is
    a CRS defined in the GeoJSON geometry or :option:`--geometry-crs` is specified.
    If the input geometry is WKT, its CRS is assumed to be the one of the input dataset,
    unless :option:`--geometry-crs` is specified.
    The X and Y axis are the "GIS friendly ones", that is X is longitude or easting,
    and Y is latitude or northing.

.. option:: --geometry-crs <CRS>

    CRS in which the coordinate values of :option:`--geometry`
    are expressed. If not specified, it is assumed to be the CRS of the input
    dataset.
    The geometry will be reprojected from the geometry-crs to the CRS of the
    dataset being updated.

.. option:: --no-update-overviews

    Do not update existing overviews.

Advanced options
++++++++++++++++

.. option:: --wo, --warp-option <NAME>=<VALUE>

    Set a warp option.  The :cpp:member:`GDALWarpOptions::papszWarpOptions` docs show all options.
    Multiple options may be listed.

.. option:: --to <NAME>=<VALUE>

    Set a transformer option suitable to pass to :cpp:func:`GDALCreateGenImgProjTransformer2`.
    See :cpp:func:`GDALCreateRPCTransformerV2()` for RPC specific options.

.. option:: --et, --error-threshold <ERROR-THRESHOLD>

    Error threshold for transformation approximation, expressed as a number of
    source pixels. Defaults to 0.125 pixels unless the ``RPC_DEM`` transformer
    option is specified, in which case an exact transformer, i.e.
    ``--error-threshold=0``, will be used.

Examples
--------

.. example::
   :title: Update existing out.tif with content of in.tif using cubic interpolation

   .. code-block:: bash

        $ gdal raster update -r cubic in.tif out.tif
