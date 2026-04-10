.. _gdal_vector_combine:

================================================================================
``gdal vector combine``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Combine geometries into collections

.. Index:: gdal vector combine

Synopsis
--------

.. program-output:: gdal vector combine --help-doc

Description
-----------

:program:`gdal vector combine` combines geometries into geometry collections.

The :option:`--group-by` argument can be used to determine which features are combined.

By default, the parts of multipart geometries will be combined into an
un-nested collection of the most specific type possible (e.g., ``MultiPolygon``
rather than ``GeometryCollection``).  For example, a Polygon and a two-part
MultiPolygon would be combined into a three-part MultiPoygon. This is done
because many GIS file formats and software packages do not handle nested
GeometryCollections types. If the nested representation in the manner of
PostGIS' ``ST_Collect`` is preferred (a two-component GeometryCollection
containing the Polygon and MultiPolygon), then :option:`--keep-nested` can be
used.

``combine`` can be used as a step of :ref:`gdal_vector_pipeline`.

Program-Specific Options
------------------------

.. option:: --group-by <FIELD_NAMES>

   The names of fields whose unique values will be used to collect
   input geometries. Any fields not listed in :option:`--group-by` will be
   removed from the source layer. If :option:`--group-by` is omitted, the
   entire layer will be combined into a single feature.

.. option:: --keep-nested

   If input geometries have multiple parts, combine them into a GeometryCollection
   of the input geometries.

.. option:: --add-extra-fields no|sometimes-identical|always-identical

    Whether fields from source features that have the same value among the features
    belonging to a same group are copied to the corresponding output feature.

    By default, ``no``, only fields listed in :option:`--group-by` are copied to
    the output layer.

    When specifying ``sometimes-identical``, fields, for which all input
    features within at least one output group have the same value, will be
    copied to the output layer schema. The value of the field is the value that
    is common for all input features of a group. If the input features within
    some group are not identical, the field value in the output for this group
    will be set to null.

    When specifying ``always-identical``, fields for which all input features
    within a given output group have the same value are copied to the output layer,
    provided this condition holds for all output groups.

    For example, let's suppose we have four input features with the following
    content, and we group them by ``country``

    .. list-table:: Input features
        :header-rows: 1

        * - name
          - country
          - country_fr
          - type
        * - Mainland France
          - France
          - France
          - continental
        * - Corsica
          - France
          - France
          - island
        * - Mainland USA
          - USA
          - Etats-Unis d'Amérique
          - continental
        * - Alaska
          - USA
          - Etats-Unis d'Amérique
          - continental

    With ``sometimes-identical``, the output will be:

    .. list-table:: Output with ``sometimes-identical``
       :header-rows: 1

       * - country
         - country_fr
         - type
       * - France
         - France
         -
       * - USA
         - Etats-Unis d'Amérique
         - continental


    The ``name`` field has been removed because it is distinct for each
    input feature. The ``type`` field appears as an output field, because at
    least for the USA, the two grouped input features that makes it have the
    same value. For France, its content is set to null, because its value is
    different in the two input features.

    With ``always-identical``, the output will be:

    .. list-table:: Output with ``always-identical``
       :header-rows: 1

       * - country
         - country_fr
       * - France
         - France
       * - USA
         - Etats-Unis d'Amérique


    The ``type`` field has been removed because its value is different in the two input
    features of France.

.. Return status code
.. ------------------

.. include:: return_code.rst

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

.. below is an allow-list for spelling checker.

.. spelling:word-list::
        Etats
        d'Amérique
