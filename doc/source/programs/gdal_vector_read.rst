.. _gdal_vector_read:

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

To test simple spatial operations, the ``read`` step can read a single geometry in (extended)
WKT format rather than a dataset.

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


.. example::
   :title: Read and buffer a single point

   .. code-block:: console

       $ gdal vector pipeline read "SRID=32145;POINT (442922 217537)" ! buffer 20 ! ... [other commands here] ...
    
