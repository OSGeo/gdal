.. _gdal_vector_set_field_type:

================================================================================
``gdal vector set-field-type``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Change the type of a field of a vector layer.

.. Index:: gdal vector set-field-type

Synopsis
--------

.. program-output:: gdal vector set-field-type --help-doc

Description
-----------

:program:`gdal vector set-field-type` can be used to modify the field type of a vector dataset:

``set-field-type`` can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Program-Specific Options
------------------------

.. option:: --field-name <FIELD-NAME>

    The name of the field to modify.

    Mutually exclusive with :option:`--src-field-type`. One of them is required.

.. option:: --field-type, --dst-field-type <FIELD-TYPE>

    The new field type. Valid values are: ``Integer``, ``IntegerList``, ``Real``, ``RealList``, ``String``, ``StringList``, ``Binary``,  ` ``Date``, ``Time``, ``DateTime``, ``Integer64``, ``Integer64List``.
    A field subtype can be specified instead of a field type. Valid values are: ``Boolean``, ``Int16``, ``Float32``, ``JSON``, ``UUID``. The field type will be derived from the subtype.

.. option:: --src-field-type <FIELD-TYPE>

    The field type of the fields to modify.
    Valid values are: ``Integer``, ``IntegerList``, ``Real``, ``RealList``, ``String``, ``StringList``, ``Binary``,  ` ``Date``, ``Time``, ``DateTime``, ``Integer64``, ``Integer64List``.
    A field subtype can be specified instead of a field type. Valid values are: ``Boolean``, ``Int16``, ``Float32``, ``JSON``, ``UUID``.

    Mutually exclusive with :option:`--field-name`. One of them is required.

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
   :title: Change the type of a field given by its name to Integer

   .. code-block:: bash

        $ gdal vector set-field-type input.gpkg output.gpkg --field-name myfield --field-type Integer

.. example::
   :title: Change the type of all fields of type Date to DateTime

   .. code-block:: bash

        $ gdal vector set-field-type input.gpkg output.gpkg --src-field-type Date --dst-field-type DateTime
