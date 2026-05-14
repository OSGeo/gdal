.. _gdal_vector_create:

================================================================================
``gdal vector create``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Create one or more vector layers, creating the dataset if it doesn't exist.

.. Index:: gdal vector create

Synopsis
--------

.. program-output:: gdal vector create --help-doc

Description
-----------

:program:`gdal vector create` creates one or more vector layers, creating the
dataset if it doesn't exist or adding layers to an existing dataset if the
underlying format supports it.

The layer fields, geometry type and coordinate reference system can be defined either by
providing an existing dataset or an OGR_SCHEMA file/JSON as a template, or by explicitly
specifying the various components.


:program:`gdal vector create` can be used as the first
step of a pipeline.

The following options are available:

Program-Specific Options
------------------------

.. option:: --output-layer <OUTPUT-LAYER>

    Output layer name.

    Only meaningful when creating a single layer dataset. It allows defining or overriding
    the name of the created layer.

.. option:: -i, --like, --input <TEMPLATE-DATASET>

    Vector dataset to use as a template for the created dataset.

    If the template dataset contains multiple layers and the destination format does
    not support multiple layers the program will exit with an error unless
    :option:`--input-layer` is used to specify which single layer from the
    input dataset should be used as a template.

    Mutually exclusive with :option:`--schema` and with :option:`--field`, :option:`--geometry-type`,
    :option:`--geometry-field` and :option:`--crs`.

.. option::  --layer, --input-layer <INPUT-LAYER>

    Layer(s) of the input dataset to use as a template for the created layer.

    This option can be repeated multiple times to specify multiple layers.

    This option is only meaningful when :option:`--like` or :option:`--schema` are used with an
    input dataset containing multiple layers.


.. option:: --schema <OGR_SCHEMA>

    File or JSON string containing the OGR_SCHEMA of the layer to create.

    This can be a filename, a URL or JSON string conformant with the
    `ogr_fields_override.schema.json schema <https://raw.githubusercontent.com/OSGeo/gdal/refs/heads/master/ogr/data/ogr_fields_override.schema.json>`_

    If the OGR_SCHEMA contains multiple layers and the destination format does
    not support multiple layers the program will exit with an error unless
    :option:`--input-layer` is used to specify which single layer from the OGR_SCHEMA
    should be selected.

    Mutually exclusive with :option:`--like` and with :option:`--field`, :option:`--geometry-type`, :option:`--geometry-field` and :option:`--crs`.

.. option:: --crs <CRS>

    Defines the coordinate reference system of the created layer.

    The coordinate reference systems that can be passed are anything supported by the
    :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which includes EPSG Projected,
    Geographic or Compound CRS (i.e. EPSG:4296), a well known text (WKT) CRS definition,
    PROJ.4 declarations, or the name of a .prj file containing a WKT CRS definition.

    Mutually exclusive with :option:`--schema` and with :option:`--like`.

.. option:: --geometry-type <GEOMETRY-TYPE>

   Defines the geometry type of the created layer.

   Layer geometry type to be one of
   ``GEOMETRY``, ``POINT``, ``LINESTRING``, ``POLYGON``, ``MULTIPOINT``, ``MULTILINESTRING``,
   ``MULTIPOLYGON``, ``GEOMETRYCOLLECTION``, ``CURVE``, ``CIRCULARSTRING``, ``COMPOUNDCURVE``,
   ``SURFACE``, ``CURVEPOLYGON``, ``MULTICURVE``, ``MULTISURFACE``, ``POLYHEDRALSURFACE`` or ``TIN``.
   ``Z``, ``M`` or ``ZM`` suffixes can be appended to the above values to
   indicate the dimensionality.

   Mutually exclusive with :option:`--schema` and with :option:`--like`.

.. option:: --field <NAME>:<TYPE>[(,<WIDTH>[,<PRECISION>])]

   Defines a field to be created in the layer.

   This option can be repeated multiple times to create multiple fields.

   The field type can be one of ``INTEGER``, ``INTEGER64``, ``REAL``, ``STRING``, ``DATE``, ``TIME``,
   ``DATETIME``, ``BINARY``, ``INTEGERLIST``, ``REALLIST``, ``STRINGLIST``, ``INTEGER64LIST``.

   The width and precision can be specified for ``INTEGER``, ``INTEGER64``, ``REAL`` and ``STRING`` field types.
   For example, ``--field population:INTEGER(10,5)`` or ``--field name:STRING(255)``

   Mutually exclusive with :option:`--schema` and with :option:`--like`.

.. option:: --fid <FID>

   Defines the name of the Feature Identifier (FID) column, for drivers that
   support setting it (those which declare a ``FID`` layer creation option)

   Mutually exclusive with :option:`--schema` and with :option:`--like`.


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
   :title: Create a POINT layer named `names` with a string field named `name` in a new vector dataset

   .. code-block:: bash

       gdal vector create --geometry-type point  --crs EPSG:4326 --field name:string --output-layer names points.gpkg

.. example::
   :title: Add a POINT layer named `names2` with a string field named `name` to an existing vector dataset

   .. code-block:: bash

       gdal vector create --update --geometry-type point --crs EPSG:4326 --field name:string --output-layer names2 points.gpkg

.. example::
   :title: Create a new vector dataset with a layer named `countries_new` based on the layer `countries` of an existing dataset

   .. code-block:: bash

       gdal vector create --like ../data/poly.gpkg --input-layer poly --output-layer areas_new areas.gpkg
