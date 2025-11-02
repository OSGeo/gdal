.. _gdal_vector_collect:

================================================================================
``gdal vector collect``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Combine geometries into collections

.. Index:: gdal vector collect

Synopsis
--------

.. program-output:: gdal vector collect --help-doc

Description
-----------

:program:`gdal vector collect` combines geometries into geometry collections.

The :option:`--group-by` argument can be used to determine which features are combined.

``collect`` can be used as a step of :ref:`gdal_vector_pipeline`.

Options
+++++++

.. option:: --group-by <FIELD_NAMES>

   The names of fields whose unique values will be used to collect
   input geometries. Any fields not listed in :option:`--group-by` will be
   removed from the source layer. If :option:`--group-by` is omitted, the
   entire layer will be combined into a single feature.

