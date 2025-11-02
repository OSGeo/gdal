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

Program-Specific Options
------------------------

.. option:: --group-by <FIELD_NAMES>

   The names of fields whose unique values will be used to collect
   input geometries. Any fields not listed in :option:`--group-by` will be
   removed from the source layer. If :option:`--group-by` is omitted, the
   entire layer will be combined into a single feature.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/lco.rst
       
    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst

