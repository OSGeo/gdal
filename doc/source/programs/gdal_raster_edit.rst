.. _gdal_raster_edit_subcommand:

================================================================================
"gdal raster edit" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Edit in place a raster dataset.

.. Index:: gdal raster edit

Synopsis
--------

.. code-block::

    Usage: gdal raster edit [OPTIONS] <DATASET>

    Edit a raster dataset.

    Positional arguments:
      --dataset <DATASET>       Dataset (in-place updated) [required]

    Common Options:
      -h, --help                Display help message and exit
      --version                 Display GDAL version and exit
      --json-usage              Display usage as JSON document and exit
      --drivers                 Display driver list as JSON document and exit

    Options:
      --crs <CRS>               Override CRS (without reprojection)
      --bbox <EXTENT>           Bounding box as xmin,ymin,xmax,ymax
      --metadata <KEY>=<VALUE>  Add/update dataset metadata item [may be repeated]
      --unset-metadata <KEY>    Remove dataset metadata item [may be repeated]


Description
-----------

:program:`gdal raster edit` can be used to edit a raster dataset.

This subcommand is also available as a potential step of :ref:`gdal_raster_pipeline_subcommand`

.. option:: --dataset <DATASET>

    Dataset name, to be in-place updated. Required.

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

.. option:: --metadata <KEY>=<VALUE>

    Add/update dataset metadata item, at the dataset level.

.. option:: --unset-metadata <KEY>

    Remove dataset metadata item, at the dataset level.


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
