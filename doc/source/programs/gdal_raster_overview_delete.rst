.. _gdal_raster_overview_delete:

================================================================================
``gdal raster overview delete``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Delete overviews of a raster dataset

.. Index:: gdal raster overview delete

Synopsis
--------

.. program-output:: gdal raster overview delete --help-doc

Description
-----------

:program:`gdal raster overview delete` can be used to delete all existing overviews
of a dataset.

.. note::

    For most file formats (including GeoTIFF or GeoPackage), the space
    previously occupied by the removed overviews may not be reclaimed.
    It might be needed for use :ref:`gdal_raster_convert` to create
    a new compact dataset.

Program-Specific Options
------------------------

.. option:: --dataset <DATASET>

    Dataset name, to be updated in-place by default (unless :option:`--external` is specified). Required.

.. option:: --external

    Remove external ``.ovr`` overviews.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/oo.rst

Examples
--------

.. example::
   :title: Delete overviews of a GeoTIFF file.

   .. code-block:: bash

       gdal raster overview delete my.tif
