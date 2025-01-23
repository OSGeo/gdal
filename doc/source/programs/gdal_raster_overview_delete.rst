.. _gdal_raster_overview_delete_subcommand:

================================================================================
"gdal raster overview delete" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Delete overviews of a raster dataset

.. Index:: gdal raster overview delete

Synopsis
--------

.. code-block::

    Usage: gdal raster overview delete [OPTIONS] <DATASET>

    Deleting overviews.

    Positional arguments:
      --dataset <DATASET>              Dataset (in-place updated, unless --external) [required]

    Common Options:
      -h, --help                       Display help message and exit
      --version                        Display GDAL version and exit
      --json-usage                     Display usage as JSON document and exit
      --drivers                        Display driver list as JSON document and exit
      --config <KEY>=<VALUE>           Configuration option [may be repeated]
      --progress                       Display progress bar

    Options:
      --external                       Remove external overviews

    Advanced Options:
      --oo, --open-option <KEY=VALUE>  Open options [may be repeated]


Description
-----------

:program:`gdal raster overview delete` can be used to delete all existing overviews
of a dataset.

.. note::

    For most file formats (including GeoTIFF or GeoPackage), the space
    previously occupied by the removed overviews may not be reclaimed.
    It might be needed for use :ref:`gdal_raster_convert_subcommand` to create
    a new compact dataset.

.. option:: --dataset <DATASET>

    Dataset name, to be in-place updated by default (unless :option:`--external` is specified). Required.

.. option:: --external

    Remove external ``.ovr`` overviews.

Examples
--------

.. example::
   :title: Delete overviews of a GeoTIFF file.

   .. code-block:: bash

       gdal raster overview delete my.tif
