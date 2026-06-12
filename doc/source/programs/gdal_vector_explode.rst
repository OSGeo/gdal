.. _gdal_vector_explode:

.. program:: gdal_vector_explode

================================================================================
``gdal vector explode``
================================================================================

.. versionadded:: 3.14

.. only:: html

    Explode fields or geometries of a vector dataset.

.. Index:: gdal vector explode

Synopsis
--------

.. program-output:: gdal vector explode --help-doc

Description
-----------

:program:`gdal vector explode` explodes features with array fields or multipart geometries, producing
a single feature for each element in the array or geometry. Fields that are not specified with ``--field``
``--geometry``, or ``--geometry-field`` are passed through unmodified.

For example, if exploding a geometry field with the value ``MULTIPOINT(1 2,3 4)``, two
corresponding target features will be generated: one with a geometry
``POINT(1 2)`` and another one with geometry ``POINT(3 4)``. Note that collections
are not recursively exploded, i.e. ``GEOMETRYCOLLECTION(GEOMETRYCOLLECTION(POINT (1 2),POINT(3 4))``
will be exploded as ``GEOMETRYCOLLECTION(POINT (1 2),POINT(3 4))``.

Multiple fields can be exploded in a single invocation; however, all fields must have the same length / 
number of geometry components.

It can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst


Program-Specific Options
------------------------

.. option:: --field <FIELD>

   Name of attribute field(s) to explode. The special value ``ALL`` can be used to explode all attribute fields with array types.

.. option:: --geometry

   Explode the default geometry field.

.. option:: --geometry-field <GEOMETRY-FIELD>

   Name or position (0-indexed) of geometry field(s) to explode. The special value ``ALL`` can be used to explode all geometry fields.
   The special value ``_OGR_GEOMETRY_`` can be used to explode the default geometry field. 

.. option:: --index-field <INDEX-FIELD>

   Optional name of a field to be added to the output containing the (0-indexed) value of each output feature in the
   input feature.


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

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Only retain parts of geometry collections that are of type Point

   .. code-block:: bash

        $ gdal vector pipeline read in.gpkg ! explode --geometry-field 0 ! set-geom-type --geometry-type=POINT --skip ! write points.shp --overwrite
