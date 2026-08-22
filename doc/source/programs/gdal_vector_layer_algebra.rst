.. _gdal_vector_layer_algebra:

.. program:: gdal_vector_layer_algebra

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
The command takes a vector input source and a method source and generates the
output of the operation in the specified output file. The output fields depend on the
operation and can be controlled with the field options.

Z and M coordinates do not affect the result of algebraic operations, which
are computed using X and Y coordinates. When present, Z and M coordinates
are propagated to the output, with values interpolated from the input
geometries.

When operations result in mixed geometry types, for example, a ``union`` operation
between a polygon layer and a point layer, the output data format must be able to
support mixed geometry types, such as GeoPackage or GeoJSON.

By default, the output layer will have the geometry type of the input layer, but this
can be overridden with the :option:`--geometry-type` option.

.. code-block:: bash

    $ gdal vector layer-algebra union points.gpkg polygon.gpkg output.gpkg --geometry-type GEOMETRYCOLLECTION
    $ gdal vector info output.gpkg
    Layer name: output
    Geometry: Geometry Collection

Since GDAL 3.14, :program:`gdal vector layer-algebra` can be used as a step of a pipeline.

Program-Specific Options
------------------------

.. option:: --geometry-type <GEOMETRY-TYPE>

   Change the geometry type to be one of
   ``GEOMETRY``, ``POINT``, ``LINESTRING``, ``POLYGON``, ``MULTIPOINT``, ``MULTILINESTRING``,
   ``MULTIPOLYGON``, ``GEOMETRYCOLLECTION``, ``CURVE``, ``CIRCULARSTRING``, ``COMPOUNDCURVE``,
   ``SURFACE``, ``CURVEPOLYGON``, ``MULTICURVE``, ``MULTISURFACE``, ``POLYHEDRALSURFACE`` or ``TIN``.
   ``Z``, ``M`` or ``ZM`` suffixes can be appended to the above values to
   indicate the dimensionality.

   If the geometry resulting from the operation cannot be converted to the
   geometry type, the corresponding output feature is not written in the output
   layer.

.. option:: --input-layer <INPUT-LAYER>

    Name of the input vector layer.

.. option:: --operation union|intersection|sym-difference|identity|update|clip|erase

    Select the operation to perform among:

    * ``union``

        A union is a set of features, which represent areas that are in either of the operand layers.
        The operation is symmetric, and input and method layers can be interchanged.
        See also the :cpp:func:`Union <OGRLayer::Union>` C++ API documentation.

        .. image:: ../../images/programs/gdal_vector_layer_algebra_union.svg

    * ``intersection``

        An intersection is a set of features, which represent the common areas of two layers.
        The operation is symmetric, and input and method layers can be interchanged.
        See also the :cpp:func:`Intersection <OGRLayer::Intersection>` C++ API documentation.

        .. image:: ../../images/programs/gdal_vector_layer_algebra_intersection.svg

    * ``sym-difference``

        A symmetric difference is a set of features, which represent areas that are in operand layers but which do not intersect.
        The operation is symmetric, and input and method layers can be interchanged.
        See also the :cpp:func:`SymDifference <OGRLayer::SymDifference>` C++ API documentation.

        .. image:: ../../images/programs/gdal_vector_layer_algebra_sym_difference.svg

    * ``identity``

        The identity method identifies features in the input layer with features in the method layer possibly splitting features into several features.
        By default the result layer has attributes from both operand layers.
        See also the :cpp:func:`Identity <OGRLayer::Identity>` C++ API documentation.

        .. image:: ../../images/programs/gdal_vector_layer_algebra_identity.svg

    * ``update``

        The update method creates a layer, which add features into the input layer from the method layer possibly cutting features in the input layer.
        By default the result layer has attributes only from the input layer.
        See also the :cpp:func:`Update <OGRLayer::Update>` C++ API documentation.

        .. image:: ../../images/programs/gdal_vector_layer_algebra_update.svg

    * ``clip``

        The clip method creates a layer, which has features from the input layer clipped to the areas of the features in the method layer.
        By default the result layer has attributes of the input layer.
        See also the :cpp:func:`Clip <OGRLayer::Clip>` C++ API documentation.

        .. image:: ../../images/programs/gdal_vector_layer_algebra_clip.svg

    * ``erase``

        The erase method creates a layer, which has features from the input layer whose areas are erased by the features in the method layer.
        By default the result layer has attributes of the input layer.
        See also the :cpp:func:`Erase <OGRLayer::Erase>` C++ API documentation.

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

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Performs a union between both input and method layers.

   .. code-block:: bash

        $ gdal vector layer-algebra union input.shp method.shp output.shp

.. example::
   :title: Performs a union between layers, with custom field names.

   In this example ``output.gpkg`` will include a field named ``analysis_fid``
   rather than the default name ``method_fid``. It will contain values from the
   ``fid`` field of the method layer.

   ``--geometry-type`` is set to ``GEOMETRY`` to allow the output layer to contain different
   geometry types.

   .. code-block:: bash

        $ gdal vector layer-algebra points.geojson polygon.geojson output.gpkg \
            --operation union --all-input-field \
            --method-field fid --method-prefix "analysis_" \
            --geometry-type GEOMETRY

.. example::
   :title: Clip a line with a polygon.

   The same result is obtained using :option:`--operation intersection`.

   .. image:: ../../images/programs/gdal_vector_layer_algebra_line_polygon_clip.svg

   .. code-block:: bash

        $ gdal vector layer-algebra line3.geojson polygon.geojson out.geojson --operation clip

.. example::
   :title: Erase points with a polygon.

   .. image:: ../../images/programs/gdal_vector_layer_algebra_points_polygon_erase.svg

   .. code-block:: bash

        $ gdal vector layer-algebra points.geojson polygon.geojson out.geojson --operation erase

.. example::
   :title: Symmetric difference between line layers.

   The input layer is displayed in blue, and the method layer in red.

   The input layer contains:

   ::

      LINESTRING (0 0,1 1,2 1,3 0)

   The method layer contains:

   ::

      LINESTRING (0 1,1 1,2 1,3 1)

   This produces two features:

   ::

      MULTILINESTRING ((0 0,1 1),(2 1,3 0))
      MULTILINESTRING ((0 1,1 1),(2 1,3 1))

   .. image:: ../../images/programs/gdal_vector_layer_algebra_lines_sym_difference.svg

   .. code-block:: bash

      $ gdal vector layer-algebra line1.geojson line2.geojson out.geojson --operation sym-difference

.. example::
   :title: Identity operation between line layers.

   The output contains two features:

   ::

      LINESTRING (1 1,2 1) # has input and method attributes
      MULTILINESTRING ((0 0,1 1),(2 1,3 0)) # has input attributes only

   .. image:: ../../images/programs/gdal_vector_layer_algebra_lines_identity.svg

   .. code-block:: bash

      $ gdal vector layer-algebra line1.geojson line2.geojson out.geojson --operation identity
