.. _vector.topojson:

TopoJSON driver
===============

.. shortname:: TopoJSON

.. built_in_by_default::

The driver can read the `TopoJSON
format <https://github.com/topojson/topojson-specification/blob/master/README.md>`__.
The driver does not support writing TopoJSON datasets.

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

The URL/filename/text might be prefixed with
TopoJSON: to avoid any ambiguity with other drivers. Alternatively, starting
with GDAL 3.10, specifying the ``-if TopoJSON`` option to command line utilities
accepting it, or ``TopoJSON`` as the only value of the ``papszAllowedDrivers`` of
:cpp:func:`GDALOpenEx`, also forces the driver to recognize the passed
URL/filename/text.

Examples
--------

Read a TopoJSON file with multiple layers.

::

   gdal vector info "https://cdn.jsdelivr.net/npm/us-atlas@3/counties-albers-10m.json"

Write a single layer from a TopoJSON file to GeoJSON file.

::

    gdal vector convert "https://cdn.jsdelivr.net/npm/us-atlas@3/counties-albers-10m.json" counties-albers-10m.geojson --layer counties

See Also
--------

-  :ref:`GeoJSON driver <vector.geojson>`
-  `TopoJSON Format
   Specification <https://github.com/topojson/topojson-specification/blob/master/README.md>`__
