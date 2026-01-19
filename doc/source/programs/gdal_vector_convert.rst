.. _gdal_vector_convert:

================================================================================
``gdal vector convert``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Convert a vector dataset.

.. Index:: gdal vector convert

Synopsis
--------

.. program-output:: gdal vector convert --help-doc

Description
-----------

:program:`gdal vector convert` can be used to convert data data between
different formats.

Standard Options
----------------

.. collapse:: Details

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
   :title: Converting file :file:`poly.shp` to a GeoPackage

   .. code-block:: console

       $ gdal vector convert poly.shp output.gpkg

.. example::
   :title: Add new layer from file :file:`line.shp` to an existing GeoPackage, and rename it "lines"

   .. code-block:: console

       $ gdal vector convert --update --output-layer=lines line.shp output.gpkg

.. example::
   :title: Append features from from file :file:`poly2.shp` to an existing layer ``poly`` of a GeoPackage, without a progress bar

   .. code-block:: console

       $ gdal vector convert --quiet --append --output-layer=poly poly2.shp output.gpkg
