.. _gdal_vector_create:

================================================================================
``gdal vector create``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Create a vector dataset.

.. Index:: gdal vector create

Synopsis
--------

.. program-output:: gdal vector create --help-doc

Description
-----------

:program:`gdal vector create` a vector dataset.


:program:`gdal vector creat` can be used as the first
step of a pipeline.

The following options are available:

Program-Specific Options
------------------------


.. option:: --output-layer <OUTPUT-LAYER>

    Output layer name.

.. option:: --crs <CRS>

    Defines the coordinate reference system of the created layer.

    The coordinate reference systems that can be passed are anything supported by the
    :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which includes EPSG Projected,
    Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
    PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.


.. option:: --geometry-type <GEOMETRY-TYPE>

   Defines the geometry type of the created layer.

   Layer geometry type to be one of
   ``GEOMETRY``, ``POINT``, ``LINESTRING``, ``POLYGON``, ``MULTIPOINT``, ``MULTILINESTRING``,
   ``MULTIPOLYGON``, ``GEOMETRYCOLLECTION``, ``CURVE``, ``CIRCULARSTRING``, ``COMPOUNDCURVE``,
   ``SURFACE``, ``CURVEPOLYGON``, ``MULTICURVE``, ``MULTISURFACE``, ``POLYHEDRALSURFACE`` or ``TIN``.
   ``Z``, ``M`` or ``ZM`` suffixes can be appended to the above values to
   indicate the dimensionality.

.. option:: --field <NAME>:<TYPE>[(<WIDTH>[,<PRECISION>])]>

   Defines a field to be created in the layer. This option can be repeated multiple times to create multiple fields.

   The field type can be one of ``INTEGER``, ``INTEGER64``, ``REAL``, ``STRING``, ``DATE``, ``TIME``,
   ``DATETIME``, ``BINARY``, ``INTEGERLIST``, ``REALLIST``, ``STRINGLIST``, ``INTEGER64LIST``.

    The width and precision can be specified for ``INTEGER``, ``INTEGER64``, ``REAL`` and ``STRING`` field types. For example, ``--field population:INTEGER(10,5)`` or ``--field name:STRING(255)``


Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/oo.rst


.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Create a POINT layer named `names` with a string field named `name` in a vector dataset

   .. command-output:: gdal vector create --geometry-type point  --crs EPSG:4326 --field name:string --output-layer names ./points.gpkg
      :cwd: ../../data
