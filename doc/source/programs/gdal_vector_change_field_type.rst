.. _gdal_vector_change_field_type:

================================================================================
``gdal vector change-field-type``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Change the type of a field of a vector layer.

.. Index:: gdal vector change-field-type

Synopsis
--------

.. program-output:: gdal vector change-field-type --help-doc

Description
-----------

:program:`gdal vector change-field-type` can be used to change the field type of a vector dataset:

``change-field-type`` can also be used as a step of :ref:`gdal_vector_pipeline`.


Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: gdal_options/lco.rst

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/active_layer.rst


.. option:: --field-name <FIELD-NAME>

    The name of the field to modify. Required.

.. option:: --field-type <FIELD-TYPE>

    The new field type. Valid values are: ``Integer``, ``IntegerList``, ``Real``, ``RealList``, ``String``, ``StringList``, ``Binary``,  ` ``Date``, ``Time``, ``DateTime``, ``Integer64``, ``Integer64List``.
    A field subtype can be specified instead of a field type. Valid values are: ``Boolean``, ``Int16``, ``Float32``, ``JSON``, ``UUID``. The field type will be derived from the subtype.


Advanced options
++++++++++++++++

.. include:: gdal_options/if.rst

.. include:: gdal_options/oo.rst

.. include:: gdal_options/output-oo.rst


.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Examples
--------

.. example::
   :title: Change the type of a field from String to Integer

   .. code-block:: bash

        $ gdal vector change-field-type input.gpkg output.gpkg --field-name myfield --field-type Integer
