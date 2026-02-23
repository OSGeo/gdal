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

