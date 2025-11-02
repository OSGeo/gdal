.. _gdal_vector_dissolve:

================================================================================
``gdal vector dissolve``
================================================================================

.. versionadded:: 3.13

.. only:: html

     Unions the elements of each feature's geometry.

.. Index:: gdal vector dissolve

Synopsis
--------

.. program-output:: gdal vector dissolve --help-doc

Description
-----------

:program:`gdal vector dissolve` performs a union operation on the elements of each feature's geometry. This has the following effects:

- Duplicate vertices are eliminated.
- Nodes are added where input linework intersects.
- Polygons that overlap are "dissolved" into a single feature.

``dissolve`` can be used as a step of :ref:`gdal_vector_pipeline`.
