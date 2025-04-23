.. _gdal_vector_geom_set_type:

================================================================================
``gdal vector geom set-type``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Modify the geometry type of a vector dataset.

.. Index:: gdal vector geom set-type

Synopsis
--------

.. program-output:: gdal vector geom set-type --help-doc

Description
-----------

:program:`gdal vector geom set-type` can be used to modify the geometry type
of geometry fields at the layer and/or feature level.

It can also be used as a step of :ref:`gdal_vector_pipeline`.

The following groups of options can be combined together:
-  :option:`--multi` / :option:`--single`
-  :option:`--linear` / :option:`--curve`
-  :option:`--xy` / :option:`--xyz` / :option:`--xym` / :option:`--xyzm`

Standard options
++++++++++++++++

.. include:: gdal_options/of_vector.rst

.. include:: gdal_options/co_vector.rst

.. include:: options/lco.rst

.. include:: gdal_options/overwrite.rst

.. include:: gdal_options/active_layer.rst

.. include:: gdal_options/active_geometry.rst

.. option:: --layer-only

    Only modify the layer geometry type. A typical use case is if the layer
    geometry type is set to unknown and the user knows that the layer only
    contains geometry of a given type.

.. option:: --feature-only

    Only modify the geometry type of features.

.. option:: --geometry-type <GEOMETRY-TYPE>

   Change the geometry type to be one of
   ``GEOMETRY``, ``POINT``, ``LINESTRING``, ``POLYGON``, ``MULTIPOINT``, ``MULTILINESTRING``,
   ``MULTIPOLYGON``, ``GEOMETRYCOLLECTION``, ``CURVE``, ``CIRCULARSTRING``, ``COMPOUNDCURVE``,
   ``SURFACE``, ``CURVEPOLYGON``, ``MULTICURVE``, ``MULTISURFACE``, ``POLYHEDRALSURFACE`` or ``TIN``.
   ``Z``, ``M`` or ``ZM`` suffixes can be appended to the above values to
   indicate the dimensionality.

   This option is mutually exclusive with :option:`--multi`, :option:`--single`,
   :option:`--linear`, :option:`--curve`, :option:`--xy`, :option:`--xyz`,
   :option:`--xym`, :option:`--xyzm`.

.. option:: --multi

   Force geometries to MULTI geometry types. e.g. ``POINT`` ==> ``MULTIPOINT``.
   This option is mutually exclusive with :option:`--single`.

.. option:: --single

   Force geometries to non-MULTI geometry types. e.g. ``MULTIPOINT`` ==> ``POINT``.
   This option is mutually exclusive with :option:`--multi`.
   MultiPoint or MultiLinestring with more than one component will be kept
   unmodified. MultiPolygon with more than one component will be merged into
   a (likely invalid according to OGC Simple Features rule) single polygon,
   merging all their rings.

.. option:: --linear

   Force geometries to linear/non-curve geometry types, approximating arcs with
   linear segments. The linear approximation can be controlled with configuration
   options :config:`OGR_ARC_STEPSIZE` and :config:`OGR_ARC_MAX_GAP`
   (specified using :option:`--config`).
   e.g. ``COMPOUNDCURVE`` ==> ``LINESTRING``.
   This option is mutually exclusive with :option:`--curve`.

.. option:: --curve

   Force geometries to curve geometry types. e.g. ``LINESTRING`` ==> ``COMPOUNDCURVE``
   or ``POLYGON`` ==> ``CURVEPOLYGON``. Points are kept unmodified.
   This option is mutually exclusive with :option:`--linear`.

.. option:: --xy

   Force geometries to XY dimension.
   This option is mutually exclusive with :option:`--xyz`, :option:`--xym`, :option:`--xyzm`.

.. option:: --xyz

   Force geometries to XYZ dimension. If the input geometry lacks a Z component,
   it will be set to 0.
   This option is mutually exclusive with :option:`--xy`, :option:`--xym`, :option:`--xyzm`.

.. option:: --xym

   Force geometries to XYM dimension. If the input geometry lacks a M component,
   it will be set to 0.
   This option is mutually exclusive with :option:`--xy`, :option:`--xyz`, :option:`--xyzm`.

.. option:: --xyzm

   Force geometries to XYZM dimension. If the input geometry lacks a Z or M component,
   it will be set to 0.
   This option is mutually exclusive with :option:`--xy`, :option:`--xyz`, :option:`--xym`.

.. option:: --skip

   Skip feature when change of feature geometry type failed (e.g. attempting
   to force a Point geometry type for a LineString geometry). Otherwise the
   source geometry will be kept unmodified (which may make it incompatible with
   the output layer if it does not support mix of geometry types).
   This option applies both when the target geometry type is defined with
   :option:`--geometry-type`, or with any of :option:`--multi`, :option:`--single`,
   :option:`--linear`, :option:`--curve`, :option:`--xy`, :option:`--xyz`,
   :option:`--xym` or :option:`--xyzm`.

Advanced options
++++++++++++++++

.. include:: gdal_options/oo.rst

.. include:: gdal_options/if.rst

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst

Examples
--------

.. example::
   :title: Convert a shapefile mixing polygons and multipolygons to a GeoPackage with multipolygons.

   .. code-block:: bash

        $ gdal vector geom set-type --geometry-type=MULTIPOLYGON in.shp out.gpkg --overwrite

.. example::
   :title: Convert a GeoPackage with curve geometries to a Shapefile (that does not support them).

   .. code-block:: bash

        $ gdal vector geom set-type --linear in.gpkg out.shp --overwrite
