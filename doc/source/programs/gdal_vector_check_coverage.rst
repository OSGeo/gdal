.. _gdal_vector_check_coverage:

================================================================================
``gdal vector check-coverage``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Checks whether a polygon dataset forms a valid coverage.

.. Index:: gdal vector check-coverage

Synopsis
--------

.. program-output:: gdal vector check-coverage --help-doc

Description
-----------

:program:`gdal vector check-coverage` checks whether a polygon dataset forms a valid coverage, meaning:

- no polygons overlap
- all shared edges have nodes at the same locations
- any gaps between polygons are larger than a specified width

For each polygon not satisfying these criteria, a linear feature with the geometry of the invalid edge is output.
If the coverage is valid, the output dataset will be empty unless :option:`--include-valid` is used.

It is assumed that the individual polygons are themselves valid according to the :term:`OGC` Simple Features standard. This can be checked by :ref:`gdal_vector_check_geometry`.

.. note:: This command requires a GDAL build against the GEOS library (version 3.12 or greater).

Standard options
++++++++++++++++

.. option:: --geometry-field

   Specify the name of the geometry field to test, for layers having multiple geometry fields. By default the first
   geometry field will be used.

.. option:: --include-valid

   Include features for valid geometries in the output, maintaining 1:1 correspondence between input and output features.

.. option:: --maximum-gap-width <MAXIMUM-GAP-WIDTH>

   Defines the largest area that should be considered a gap.

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible_non_natively_streamable.rst

