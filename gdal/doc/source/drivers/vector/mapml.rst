.. _vector.mapml:

MapML
=====

.. versionadded:: 3.1

.. shortname:: MapML

.. built_in_by_default::

This driver implements read and write support for the
`MapML specification <https://maps4html.org/MapML/spec>`_.
It only implements reading and writing vector features.

.. warning::

    This driver implements an experimental specification, and inherits its
    experimental status. This specification may change at a later point, or not
    be adopted. Files written by this driver may no longer be readable in later
    versions of GDAL.


Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Read support
------------

Layers are identified with the ``class`` attribute of features.

Fields are retrieved from the HTML table in the ``properties`` element of
features. This assumes that they are written following the exact same structure
as the write side of the driver. Otherwise no fields will be retrieved.
Field type is guessed from the values, and may consequently be sometimes inaccurate.

Write support
-------------

Several layers can be written in the same MapML file.

Only the following CRS are natively supports, EPSG:4326 (WGS 84),
EPSG:3857 (WebMercator), EPSG:3978 (NAD83 / Canada Atlas Lambert) and
EPSG:5936 (WGS 84 / EPSG Alaska Polar Stereographic). Layers in other CRS
will be automatically reprojected to EPSG:4326.

Geometry types Point, LineString, Polygon, MultiPoint, MultiLineString,
MultiPolygon and GeometryCollection are supported.

Attributes are written as a HTML table.

Dataset creation options
------------------------

-  **HEAD**\ =string: Filename or inline XML content for head element.
-  **EXTENT_UNITS**\ =AUTO/WGS84/OSMTILE/CBMTILE/APSTILE: To override the CRS.
-  **EXTENT_ACTION**\ =string: Value of ``extent@action`` attribute.
-  **EXTENT_XMIN**\ =float: Override extent xmin value.
-  **EXTENT_YMIN**\ =float: Override extent ymin value.
-  **EXTENT_XMAX**\ =float: Override extent xmax value.
-  **EXTENT_YMAX**\ =float: Override extent ymax value.
-  **EXTENT_XMIN_MIN**\ =float: Min value for extent.xmin value.
-  **EXTENT_XMIN_MAX**\ =float: Max value for extent.xmin value.
-  **EXTENT_YMIN_MIN**\ =float: Min value for extent.ymin value.
-  **EXTENT_YMIN_MAX**\ =float: Max value for extent.ymin value.
-  **EXTENT_XMAX_MIN**\ =float: Min value for extent.xmax value.
-  **EXTENT_XMAX_MAX**\ =float: Max value for extent.xmax value.
-  **EXTENT_YMAX_MIN**\ =float: Min value for extent.ymax value.
-  **EXTENT_YMAX_MAX**\ =float: Max value for extent.ymax value.
-  **EXTENT_ZOOM**\ =int: Value of extent.zoom.
-  **EXTENT_ZOOM_MIN**\ =int: Min value for extent.zoom.
-  **EXTENT_ZOOM_MAX**\ =int: Max value for extent.zoom.
-  **EXTENT_EXTRA**\ =string: Filename of inline XML content for extra content to insert in extent element.
-  **BODY_LINKS**\ =string: Inline XML content for extra content to insert as link elements in the body. For example '<link type="foo" href="bar" /><link type="baz" href="baw" />'

Links
-----

-  `MapML specification <https://maps4html.org/MapML/spec>`_
-  `MapML schemas <https://github.com/Maps4HTML/MapML/tree/gh-pages/schema>`_
-  :ref:`gdal2tiles` mapml output
