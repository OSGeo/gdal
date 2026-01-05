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

.. note:: VRT output is not compatible with materialize

Synopsis
--------

.. program-output:: gdal raster pipeline --help-doc=materialize

Program-Specific Options
------------------------

.. option:: --output <OUTPUT>

   Optional dataset name. When specified, it is not removed at the end of the
   process.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/of_raster_create_copy.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Reproject a GeoTIFF file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N"), materialize it to a temporary file and compute its contour lines

   .. code-block:: bash

        $ gdal pipeline ! read in.tif ! reproject --dst-crs=EPSG:32632 ! \
                        ! materialize ! contour --interval=10 ! write out.gpkg --overwrite


.. example::
   :title: How to use materialize for Cloud Optimized GeoTIFF (COG)

    Usually when you want load COG data, you are not loading the whole data but instead on certain region and using lower resolution for fast analysis.
    Therefore, after using command `read`, it is not supposed to be materialize right away because it mean it will download the whole thing into the memory/disk.
    Instead, you could use command `reproject` and use `--bbox` to limit the region of interest and set `--size` or `--resolution` to use lower resolution overview.

   .. code-block:: bash

        $ gdal pipeline ! read /vsicurl/https://some.storage.com/mydata/landcover.tif \
                        ! reproject -r mode -d EPSG:4326 --bbox=112,2,116,4.5 --bbox-crs=EPSG:4326 --size=3000,3000 \
                        ! materialize \
                        ! zonal-stats --zones=/vsicurl/https://some.storage.com/mydata/adm_level4.fgb --stat=values \
                        ! write out.geojson

