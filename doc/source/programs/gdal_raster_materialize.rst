.. _gdal_raster_materialize:

================================================================================
``gdal raster materialize``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Materialize a piped dataset on disk to increase the efficiency of the following steps

.. Index:: gdal raster materialize

Description
-----------

The ``materialize`` operation is for use only in a pipeline, and writes the
incoming dataset to a dataset on disk, before passing that materialized dataset
to following steps. This can be used when materializing a dataset helps for
performance compared to on-demand evaluation of previous steps.

By default, a tiled compressed GeoTIFF file is used for the materialized dataset,
created in the directory pointed by the :config:`CPL_TMPDIR` configuration option or in the
current directory if not specified, and is deleted when the pipeline finishes.
The user can also select another format, or specify an explicit filename,
in which case the materialized dataset is not deleted when the pipeline finishes.

Synopsis
--------

.. program-output:: gdal raster pipeline --help-doc=materialize

Options
-------

.. option:: --output <OUTPUT>

   Optional dataset name. When specified, it is not removed at the end of the
   process.

.. include:: gdal_options/of_raster_create_copy.rst

.. include:: gdal_options/co.rst

.. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Reproject a GeoTIFF file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N"), materialize it to a temporary file and compute its contour lines

   .. code-block:: bash

        $ gdal pipeline ! read in.tif ! reproject --dst-crs=EPSG:32632 ! \
                        ! materialize ! contour --interval=10 ! write out.gpkg --overwrite
