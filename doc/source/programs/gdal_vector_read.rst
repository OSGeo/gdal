.. _gdal_vector_read:

.. program:: gdal_vector_read

================================================================================
``gdal vector pipeline read``
================================================================================

.. versionadded:: 3.11

.. only:: html

   Read a vector dataset (pipeline only)

.. Index:: gdal vector pipeline read

Description
-----------

The ``read`` operation is for use in a :ref:`gdal_pipeline` only, and reads a single input
vector dataset. This is the first step of a pipeline.

Synopsis
--------

.. program-output:: gdal vector pipeline --help-doc=read

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

Examples
--------

.. example::
   :title: Read a GeoPackage file

   .. code-block:: bash

        $ gdal vector pipeline read input.gpkg ! ... [other commands here] ...
