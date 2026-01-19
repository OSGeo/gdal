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

Program-Specific Options
------------------------

.. option:: --approx-stats

    Compute raster band statistics for all bands. They may be computed
    based on overviews or a subset of all tiles. Useful if you are in a
    hurry and don't need precise stats.

    .. note:: This option is not available when the command is part of a pipeline.

.. option:: --auxiliary

    Force opening the dataset in read-only mode. For drivers that implement the
    Persistent Auxiliary Metadata (PAM) mechanism, changes will be
    saved in an auxiliary side car file of extension ``.aux.xml``.

.. option:: --bbox <xmin>,<ymin>,<xmax>,ymax>

    Override the spatial bounding box, in CRS units, without reprojecting or subsetting.
    'x' is longitude values for geographic CRS and easting for projected CRS.
    'y' is latitude values for geographic CRS and northing for projected CRS.

.. option:: --crs <CRS>

    Override CRS, without reprojecting.

    The coordinate reference systems that can be passed are anything supported by the
    :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which includes EPSG Projected,
    Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
    PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.

    ``null`` or ``none`` can be specified to unset an existing CRS.

    Note that the spatial extent is also left unchanged.

.. option:: --dataset <DATASET>

    Dataset name, to be updated in-place, unless :option:`--auxiliary` is set. Required.

.. option:: --gcp <GCP>

    .. versionadded:: 3.12

    Set ground control point(s), replacing any existing GCPs. Each GCP must be formatted as a string
    "pixel,line,easting,northing" or "pixel,line,easting,northing,elevation".
    Each GCP must be specified with a ``--gcp`` argument.

    It is also possible to provide a single ``--gcp`` argument whose value is
    the filename of a vector dataset, prefixed with `@`. This dataset must have
    a single layer with the following required fields ``column``, ``line``, ``x``, ``y``,
    and optionally ``id``, ``info`` and ``z``.

.. option:: --hist

    Compute histogram information for all bands.

    .. note:: This option is not available when the command is part of a pipeline.

.. option:: --metadata <KEY>=<VALUE>

    Add/update metadata item, at the dataset level. May be repeated.

.. option:: --nodata <value>

    Override nodata value.

    ``null`` or ``none`` can be specified to unset an existing nodata value.

.. option:: --stats

    Compute raster band statistics for all bands.


.. option:: --unset-metadata <KEY>

    Remove metadata item, at the dataset level. May be repeated.

.. option:: --unset-metadata-domain <DOMAIN>

    .. versionadded:: 3.12

    Remove metadata domain, at the dataset level. May be repeated.


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

.. example::
   :title: Add 2 ground control point (GCP) for (column=0,line=0,X=2,Y=49) and (column=50,line=100,X=3,Y=48)

   .. code-block:: bash

        $ gdal raster edit --gcp 0,0,2,49 --gcp 50,100,3,48 my.tif

.. example::
   :title: Add ground control point (GCP) from :file:`gcps.csv`, that must have fields named ``column``, ``line``, ``x`` and  ``y``.

   .. code-block:: bash

        $ gdal raster edit --gcp @gcps.csv my.tif
