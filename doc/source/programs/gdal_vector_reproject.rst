.. _gdal_vector_reproject:

================================================================================
``gdal vector reproject``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Reproject a vector dataset.

.. Index:: gdal vector reproject

Synopsis
--------

.. program-output:: gdal vector reproject --help-doc

Description
-----------

:program:`gdal vector reproject` can be used to reproject a vector dataset.
The program can reproject to any supported projection.

This command can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst


Program-Specific Options
------------------------

.. option:: -d, --dst-crs <SRC-CRS>

    Set destination spatial reference.

    .. include:: gdal_options/srs_def_gdal_raster_reproject.rst

.. option:: -s, --src-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: gdal_options/srs_def_gdal_raster_reproject.rst

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_layer.rst

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/input_layer.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_layer.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst

Examples
--------

.. example::
   :title: Reproject a GeoPackage file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N")

   .. code-block:: bash

        $ gdal vector reproject --dst-crs=EPSG:32632 in.gpkg out.gpkg --overwrite
