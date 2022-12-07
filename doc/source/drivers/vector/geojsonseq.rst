.. _vector.geojsonseq:

GeoJSONSeq: sequence of GeoJSON features
========================================

.. versionadded:: 2.4

.. shortname:: GeoJSONSeq

.. built_in_by_default::

This driver implements read/creation support for features encoded
individually as `GeoJSON <http://geojson.org/>`__ Feature objects,
separated by newline (LF) (`Newline Delimited
JSON <http://ndjson.org/>`__) or record-separator (RS) characters (`RFC
8142 <https://tools.ietf.org/html/rfc8142>`__ standard: GeoJSON Text
Sequences)

Such files are equivalent to a GeoJSON FeatureCollection, but are more
friendly for incremental parsing.

The driver automatically reprojects geometries to WGS84 longitude, latitude,
if the layer is created with another SRS.

Appending to an existing file is supported since GDAL 3.6

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Datasource
----------

The driver accepts three types of sources of data:

-  Uniform Resource Locator (`URL <http://en.wikipedia.org/wiki/URL>`__)
   - a Web address to perform
   `HTTP <http://en.wikipedia.org/wiki/HTTP>`__ request
-  Plain text file with GeoJSON data - identified from the file
   extension .geojsonl or .geojsons
-  Text passed directly as filename, and encoded as GeoJSON sequences

The URL/filename/text might be prefixed with GeoJSONSeq: to avoid any
ambiguity with other drivers.

Configuration options
---------------------

The following :ref:`configuration option <configoptions>` is
available:

-  :decl_configoption:`OGR_GEOJSON_MAX_OBJ_SIZE` (GDAL >= 3.5.2): size in
   MBytes of the maximum accepted single feature, default value is 200MB.
   Or 0 to allow for a unlimited size.

Layer creation options
----------------------

-  **RS**\ =YES/NO: whether to start records with the RS=0x1E character,
   so as to be compatible with the `RFC
   8142 <https://tools.ietf.org/html/rfc8142>`__ standard. Defaults to
   NO, unless the filename extension is "geojsons"
-  **COORDINATE_PRECISION** = int_number: Maximum number of figures
   after decimal separator to write in coordinates. Default to 7.
   "Smart" truncation will occur to remove trailing zeros.
-  **SIGNIFICANT_FIGURES** = int_number: Maximum number of significant
   figures when writing floating-point numbers. Default to 17. If
   explicitly specified, and COORDINATE_PRECISION is not, this will also
   apply to coordinates.
-  **ID_FIELD**\ =string. Name of the source field that must be written
   as the 'id' member of Feature objects.
-  **ID_TYPE**\ =AUTO/String/Integer. Type of the 'id' member of Feature
   objects.

See Also
--------

-  :ref:`GeoJSON driver <vector.geojson>`
-  `RFC 7946 <https://tools.ietf.org/html/rfc7946>`__ standard: the
   GeoJSON Format.
-  `RFC 8142 <https://tools.ietf.org/html/rfc8142>`__ standard: GeoJSON
   Text Sequences (RS separator)
-  `Newline Delimited JSON <http://ndjson.org/>`__
-  `GeoJSONL <https://www.interline.io/blog/geojsonl-extracts/>`__: An
   optimized format for large geographic datasets
