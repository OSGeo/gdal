.. _vector.svg:

SVG - Scalable Vector Graphics
==============================

.. shortname:: SVG

.. build_dependencies:: libexpat

OGR has support for SVG reading (if GDAL is built with *expat* library
support).

Currently, it will only read SVG files that are the output from
Cloudmade Vector Stream Server

All coordinates are relative to the Pseudo-mercator SRS (EPSG:3857).

The driver will return 3 layers :

-  points
-  lines
-  polygons

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `W3C SVG page <http://www.w3.org/TR/SVG/>`__
-  `Cloudmade vector
   documentation <http://developers.cloudmade.com/wiki/vector-stream-server/Documentation>`__
