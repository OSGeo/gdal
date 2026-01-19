.. _gdal_vector_select:

================================================================================
``gdal vector select``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Select a subset of fields from a vector dataset.

.. Index:: gdal vector select

Synopsis
--------

.. program-output:: gdal vector select --help-doc

Description
-----------

:program:`gdal vector select` can be used to select a subset of fields.

``select`` can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Program-Specific Options
------------------------

.. option:: --exclude

    Modifies the behavior of the algorithm such that all fields are selected,
    except the ones mentioned by :option:`--fields`.

.. option:: --fields <FIELDS>

    Comma-separated list of fields from input layer to copy to the new layer
    (or to exclude if :option:`--exclude` is specified)

    Field names with spaces, commas or double-quote
    should be surrounded with a starting and ending double-quote character, and
    double-quote characters in a field name should be escaped with backslash.

    Depending on the shell used, this might require further quoting. For example,
    to select ``regular_field``, ``a_field_with space, and comma`` and
    ``a field with " double quote`` with a Unix shell:

    .. code-block:: bash

        --fields "regular_field,\"a_field_with space, and comma\",\"a field with \\\" double quote\""

    A field is only selected once, even if mentioned several times in the list.

    Geometry fields can also be specified in the list. If the source layer has
    no explicit name for the geometry field, ``_ogr_geometry_`` must be used to
    select the unique geometry field.

    Specifying a non-existing source field name results in an error.

.. option:: --ignore-missing-fields

    By default, if a field specified by :option:`--fields` does not exist in the input
    layer(s), an error is emitted and the processing is stopped.
    When specifying :option:`--ignore-missing-fields`, only a warning is
    emitted and the non existing fields are just ignored.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_layer.rst

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/input_layer.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_layer.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst

Examples
--------

.. example::
   :title: Select the EAS_ID field and the geometry field from a Shapefile

   .. code-block:: bash

        $ gdal vector select in.shp out.gpkg "EAS_ID,_ogr_geometry_" --overwrite


.. example::
   :title: Remove sensitive fields from a layer

   .. code-block:: bash

        $ gdal vector select in.shp out.gpkg --exclude "name,surname,address" --overwrite
