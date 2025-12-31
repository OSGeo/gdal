.. _gdal_vector_layer_algebra:

================================================================================
``gdal vector layer-algebra``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Perform algebraic operation between 2 layers

.. Index:: gdal vector layer-algebra

Synopsis
--------

.. program-output:: gdal vector layer-algebra --help-doc

Description
-----------

:program:`gdal vector layer-algebra` performs various vector layer algebraic operations.
The  command takes a vector input source and a method source and generates the
output of the operation in the specified output file.

Program-Specific Options
------------------------

.. option:: --geometry-type <GEOMETRY-TYPE>

   Change the geometry type to be one of
   ``GEOMETRY``, ``POINT``, ``LINESTRING``, ``POLYGON``, ``MULTIPOINT``, ``MULTILINESTRING``,
   ``MULTIPOLYGON``, ``GEOMETRYCOLLECTION``, ``CURVE``, ``CIRCULARSTRING``, ``COMPOUNDCURVE``,
   ``SURFACE``, ``CURVEPOLYGON``, ``MULTICURVE``, ``MULTISURFACE``, ``POLYHEDRALSURFACE`` or ``TIN``.
   ``Z``, ``M`` or ``ZM`` suffixes can be appended to the above values to
   indicate the dimensionality.

.. option:: --input-layer <INPUT-LAYER>

    Name of the input vector layer.

.. option:: --operation union|intersection|sym-difference|identity|update|clip|erase

    Select the operation to perform among:

    * ``union``

        A union is a set of features, which represent areas that are in either of the operand layers.
        The operation is symmetric, and input and method layers can be interchanged.

        .. only:: html

           .. image:: ../../images/programs/gdal_vector_layer_algebra_union.svg

    * ``intersection``

        An intersection is a set of features, which represent the common areas of two layers.
        The operation is symmetric, and input and method layers can be interchanged.

        .. only:: html

           .. image:: ../../images/programs/gdal_vector_layer_algebra_intersection.svg

    * ``sym-difference``

        A symmetric difference is a set of features, which represent areas that are in operand layers but which do not intersect.
        The operation is symmetric, and input and method layers can be interchanged.

        .. only:: html

           .. image:: ../../images/programs/gdal_vector_layer_algebra_sym_difference.svg

    * ``identity``

        The identity method identifies features in the input layer with features in the method layer possibly splitting features into several features.
        By default the result layer has attributes from both operand layers.

        .. only:: html

           .. image:: ../../images/programs/gdal_vector_layer_algebra_identity.svg


    * ``update``

        The update method creates a layer, which add features into the input layer from the method layer possibly cutting features in the input layer.
        By default the result layer has attributes only from the input layer.

        .. only:: html

           .. image:: ../../images/programs/gdal_vector_layer_algebra_update.svg

    * ``clip``

        The clip method creates a layer, which has features from the input layer clipped to the areas of the features in the method layer.
        By default the result layer has attributes of the input layer.

        .. only:: html

           .. image:: ../../images/programs/gdal_vector_layer_algebra_clip.svg


    * ``erase``

        The erase method creates a layer, which has features from the input layer whose areas are erased by the features in the method layer.
        By default the result layer has attributes of the input layer.

        .. only:: html

           .. image:: ../../images/programs/gdal_vector_layer_algebra_erase.svg

.. option:: --method-layer <METHOD-LAYER>

    Name of the method vector layer.

Advanced options
++++++++++++++++

.. option:: --all-input-field

   Add all input fields to output layer.
   Mutually exclusive with :option:`--input-field`, :option:`--no-input-field`.

.. option:: --all-method-field

   Add all method fields to output layer.
   Mutually exclusive with :option:`--method-field`, :option:`--no-method-field`.

.. option:: --input-field <INPUT-FIELD>

   Input field(s) to add to output layer [may be repeated]
   Mutually exclusive with :option:`--no-input-field`, :option:`--all-input-field`.

.. option:: --input-prefix <INPUT-PREFIX>

   Prefix for fields corresponding to input layer. Defaults to ``input_``
   if there are both input and method fields, otherwise empty string.

.. option:: --method-field <METHOD-FIELD>

   Input field(s) to add to output layer [may be repeated]
   Mutually exclusive with :option:`--no-method-field`, :option:`--all-method-field`.

.. option:: --method-prefix <METHOD-PREFIX>

   Prefix for fields corresponding to method layer. Defaults to ``method_``
   if there are both input and method fields, otherwise empty string.

.. option:: --no-input-field

   Do not add any input field to output layer.
   Mutually exclusive with :option:`--input-field`, :option:`--all-input-field`.

.. option:: --no-method-field

   Do not add any method field to output layer.
   Mutually exclusive with :option:`--method-field`, :option:`--all-method-field`.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_layer.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/update.rst

Examples
--------

.. example::
   :title: Performs a union between both input and method layers.

   .. code-block:: bash

        $ gdal vector layer-algebra union input.shp method.shp output.shp
