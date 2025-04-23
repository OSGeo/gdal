.. _gdal_raster_edit:

================================================================================
``gdal raster edit``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Edit in place a raster dataset.

.. Index:: gdal raster edit

Synopsis
--------

.. program-output:: gdal raster edit --help-doc

Description
-----------

:program:`gdal raster edit` can be used to edit a raster dataset.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline`

.. option:: --dataset <DATASET>

    Dataset name, to be updated in-place, unless :option:`--auxiliary` is set. Required.

.. option:: --auxiliary

    Force opening the dataset in read-only mode. For drivers that implement the
    Persistent Auxiliary Metadata (PAM) mechanism, changes will be
    saved in an auxiliary side car file of extension ``.aux.xml``.

.. option:: --crs <CRS>

    Override CRS, without reprojecting.

    The coordinate reference systems that can be passed are anything supported by the
    :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which includes EPSG Projected,
    Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
    PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.

    ``null`` or ``none`` can be specified to unset an existing CRS.

    Note that the spatial extent is also left unchanged.

.. option:: --bbox <xmin>,<ymin>,<xmax>,ymax>

    Override the spatial bounding box, in CRS units, without reprojecting or subsetting.
    'x' is longitude values for geographic CRS and easting for projected CRS.
    'y' is latitude values for geographic CRS and northing for projected CRS.

.. option:: --nodata <value>

    Override nodata value.

    ``null`` or ``none`` can be specified to unset an existing nodata value.

.. option:: --metadata <KEY>=<VALUE>

    Add/update metadata item, at the dataset level.

.. option:: --unset-metadata <KEY>

    Remove metadata item, at the dataset level.

.. option:: --stats

    Compute raster band statistics for all bands.

.. option:: --approx-stats

    Compute raster band statistics for all bands. They may be computed
    based on overviews or a subset of all tiles. Useful if you are in a
    hurry and don't need precise stats.

.. option:: --hist

    Compute histogram information for all bands.


Examples
--------

.. example::
   :title: Override (without reprojecting) the CRS of a dataset

   .. code-block:: bash

        $ gdal raster edit --crs=EPSG:32632 my.tif

.. example::
   :title: Override (without reprojecting or subsetting) the bounding box of a dataset

   .. code-block:: bash

        $ gdal raster edit --bbox=2,49,3,50 my.tif

.. example::
   :title: Add a metadata item

   .. code-block:: bash

        $ gdal raster edit --metadata AUTHOR=EvenR my.tif

.. example::
   :title: Remove a metadata item

   .. code-block:: bash

        $ gdal raster edit --unset-metadata AUTHOR my.tif
