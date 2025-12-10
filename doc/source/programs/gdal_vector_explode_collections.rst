.. _gdal_vector_explode_collections:

================================================================================
``gdal vector explode-collections``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Explode geometries of type collection of a vector dataset.

.. Index:: gdal vector explode-collections

Synopsis
--------

.. program-output:: gdal vector explode-collections --help-doc

Description
-----------

:program:`gdal vector explode-collections` produces one feature for
each geometry in any kind of geometry collection.

For example if a feature contains a geometry ``MULTIPOINT(1 2,3 4)``, two
corresponding target features will be generated: one with a geometry
``POINT(1 2)`` and another one with geometry ``POINT(3 4)``. Note that collections
are not recursively exploded, i.e. ``GEOMETRYCOLLECTION(GEOMETRYCOLLECTION(POINT (1 2),POINT(3 4))``
will be exploded as ``GEOMETRYCOLLECTION(POINT (1 2),POINT(3 4))``.

It can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst


Program-Specific Options
------------------------

.. option:: --geometry-type <GEOMETRY-TYPE>

   Change the geometry type of exploded geometries to be one of
   ``GEOMETRY``, ``POINT``, ``LINESTRING``, ``POLYGON``, ``CIRCULARSTRING``, ``COMPOUNDCURVE``,
   ``CURVEPOLYGON``, ``POLYHEDRALSURFACE`` or ``TIN``.
   ``Z``, ``M`` or ``ZM`` suffixes can be appended to the above values to
   indicate the dimensionality. The presence of geometries that cannot be
   coerced to the specified type will cause an error, unless :option:`--skip-on-type-mismatch` is
   specified.

.. option:: --skip-on-type-mismatch

   Do not generate output features when their target geometry type does not
   match the one specified by :option:`--geometry-type`.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_geometry.rst

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
   :title: Only retain parts of geometry collections that are of type Point.

   .. code-block:: bash

        $ gdal vector explode-collections --geometry-type=POINT --skip-on-type-mismatch in.gpkg points.shp --overwrite
