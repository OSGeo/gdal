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

|about-dataset-creation-options|
The following dataset creation options are supported:

-  .. dsco:: HEAD

      Filename or inline XML content for head element.

-  .. dsco:: EXTENT_UNITS
      :choices: AUTO, WGS84, OSMTILE, CBMTILE, APSTILE

      To override the CRS.

-  .. dsco:: EXTENT_XMIN

      Override extent xmin value.

-  .. dsco:: EXTENT_YMIN

      Override extent ymin value.

-  .. dsco:: EXTENT_XMAX

      Override extent xmax value.

-  .. dsco:: EXTENT_YMAX

      Override extent ymax value.

-  .. dsco:: EXTENT_ZOOM
      :choices: <integer>

      Value of extent.zoom.

-  .. dsco:: EXTENT_ZOOM_MIN
      :choices: <integer>

      Min value for extent.zoom.

-  .. dsco:: EXTENT_ZOOM_MAX
      :choices: <integer>

      Max value for extent.zoom.

-  .. dsco:: HEAD_LINKS

      Inline XML content for extra content to insert as link elements in the map-head. For example '<link rel="license" href="bar" title="foo" /><link type="baz" href="baw" />'

Links
-----

-  `MapML.js documentation <https://maps4html.org/web-map-doc/>`_
-  `MapML specification <https://maps4html.org/MapML/spec>`_
-  `MapML schemas <https://github.com/Maps4HTML/MapML/tree/gh-pages/schema>`_
-  :ref:`gdal2tiles` mapml output
