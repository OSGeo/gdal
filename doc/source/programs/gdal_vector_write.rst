.. _gdal_vector_write:

================================================================================
``gdal vector pipeline write``
================================================================================

.. versionadded:: 3.11

.. only:: html

   Write a vector dataset (pipeline only)

.. Index:: gdal vector pipeline write

Description
-----------

The ``write`` operation is for use in a :ref:`gdal_pipeline` only, and writes a
vector dataset. This is the last step of a pipeline.

To write a temporary dataset in the middle of a pipeline, use :ref:`gdal_vector_materialize`.

Synopsis
--------

.. program-output:: gdal vector pipeline --help-doc=write

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/output_layer.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/upsert.rst

Examples
--------

.. example::
   :title: Write a GeoPackage file

   .. code-block:: bash

        $ gdal vector pipeline ... [other commands here] ... ! write out.gpkg --overwrite
