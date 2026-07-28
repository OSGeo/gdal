.. _gdal_mdim_write:

.. program:: gdal_mdim_pipeline_write

================================================================================
``gdal mdim pipeline write``
================================================================================

.. versionadded:: 3.14

.. only:: html

   Write a multidimensional dataset (pipeline only)

.. Index:: gdal mdim pipeline write

Description
-----------

The ``write`` operation is for use in a :ref:`gdal_mdim_pipeline` only, and writes a
multidimensional dataset. This is the last step of a pipeline.

.. NOT YET IMPLEMENTED
.. To write a temporary dataset in the middle of a pipeline, use :ref:`gdal_mdim_materialize`.

Synopsis
--------

.. program-output:: gdal mdim pipeline --help-doc=write

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/of_mdim_create_copy.rst

    .. include:: gdal_options/co.rst

    .. include:: gdal_options/overwrite.rst

Examples
--------

.. example::
   :title: Write a Zarr dataset

   .. code-block:: bash

        $ gdal mdim pipeline ... [other commands here] ... ! write out.zarr --overwrite
