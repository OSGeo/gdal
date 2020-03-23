.. _vector.topojson:

TopoJSON driver
===============

.. shortname:: TopoJSON

.. built_in_by_default::

(Note: prior to GDAL 2.3, the functionality of this driver was available
in the GeoJSON driver. They are now distinct drivers)

The driver can read the `TopoJSON
format <https://github.com/topojson/topojson-specification/blob/master/README.md>`__

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Datasource
----------

The driver accepts three types of sources of data:

-  Uniform Resource Locator (`URL <http://en.wikipedia.org/wiki/URL>`__)
   - a Web address to perform
   `HTTP <http://en.wikipedia.org/wiki/HTTP>`__ request.
-  Plain text file with TopoJSON data - identified from the file
   extension .json or .topojson
-  Text passed directly and encoded in Topo JSON

Starting with GDAL 2.3, the URL/filename/text might be prefixed with
TopoJSON: to avoid any ambiguity with other drivers.

See Also
--------

-  :ref:`GeoJSON driver <vector.geojson>`
-  `TopoJSON Format
   Specification <https://github.com/topojson/topojson-specification/blob/master/README.md>`__
