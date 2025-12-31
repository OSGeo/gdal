.. _gdal_vector_sort:

================================================================================
``gdal vector sort``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Spatially sort a vector dataset.

.. Index:: gdal vector sort

Synopsis
--------

.. program-output:: gdal vector sort --help-doc

Description
-----------

:program:`gdal vector sort` sorts the features in a vector dataset according to the position of its geometries. The sorting method can be specified via :option:`--method`, with Hilbert and Sort-Tile-Recursive (STRTree) algorithms available.

Features that have null or empty geometries will be placed at the end of the sorted dataset.

This command can also be used as a step of :ref:`gdal_vector_pipeline`.

.. note:: Use of the STRtree algorithm requires a GDAL build against the GEOS library.

Program-Specific Options
------------------------

.. option:: --geometry-field <GEOMETRY-FIELD>

   The name of the geometry field by which features should be sorted.

.. option:: --method <METHOD>

   Specifies the sorting method. Available options are:

   * hilbert : default method. Geometries are sorted according to the Hilbert code of the center point of their bounding box
   * strtree : Geometries are sorted by constructing a sort-tile-recursive tree using the GEOS library and performing a depth-first iteration.

Examples
--------

.. example::
   :title: Create a cloud-optimized Shapefile

   .. code-block:: bash

        $ gdal vector sort in.gpkg out.shp --method hilbert --lco SPATIAL_INDEX=YES

.. below is an allow-list for spelling checker.

.. spelling:word-list::
        Hilbert
        hilbert
        strtree
