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

-  :copy-config:`OGR_GEOJSON_MAX_OBJ_SIZE`

Layer creation options
----------------------

-  .. lco:: RS
      :choices: YES, NO

      whether to start records with the RS=0x1E character,
      so as to be compatible with the `RFC
      8142 <https://tools.ietf.org/html/rfc8142>`__ standard. Defaults to
      NO, unless the filename extension is "geojsons"

-  .. lco:: COORDINATE_PRECISION
      :choices: <integer>
      :default: 7

      Maximum number of figures
      after decimal separator to write in coordinates.
      "Smart" truncation will occur to remove trailing zeros.

-  .. lco:: SIGNIFICANT_FIGURES
      :choices: <integer>
      :default: 17

      Maximum number of significant
      figures when writing floating-point numbers. If
      explicitly specified, and :lco:`COORDINATE_PRECISION` is not, this will also
      apply to coordinates.

-  .. lco:: ID_FIELD

      Name of the source field that must be written
      as the 'id' member of Feature objects.

-  .. lco:: ID_TYPE
      :choices: AUTO, String, Integer

      Type of the 'id' member of Feature objects.

-  .. lco:: WRITE_NON_FINITE_VALUES
      :choices: YES, NO
      :default: NO
      :since: 3.8

      Whether to write
      NaN / Infinity values. Such values are not allowed in strict JSon
      mode, but some JSon parsers (libjson-c >= 0.12 for example) can
      understand them as they are allowed by ECMAScript.

-  .. lco:: AUTODETECT_JSON_STRINGS
      :choices: YES, NO
      :default: YES
      :since: 3.8

      Whether to try to interpret string fields as JSON arrays or objects
      if they start and end with brackets and braces, even if they do
      not have their subtype set to JSON.

Geometry coordinate precision
-----------------------------

.. versionadded:: GDAL 3.9

On creation, the GeoJSONSeq driver supports using the geometry coordinate
precision, from th :cpp:class:`OGRGeomCoordinatePrecision` settings of the
:cpp:class:`OGRGeomFieldDefn`. Those settings are used to round the coordinates
of the geometry of the features to an appropriate decimal precision.

.. note::

    The :lco:`COORDINATE_PRECISION` layer creation option has precedence over
    the values set on the :cpp:class:`OGRGeomFieldDefn`.

The value of those geometry coordinate precision is *not* serialized in the
generated file, hence on reading, the driver will not advertise a geometry
coordinate precision.

See Also
--------

-  :ref:`GeoJSON driver <vector.geojson>`
-  `RFC 7946 <https://tools.ietf.org/html/rfc7946>`__ standard: the
   GeoJSON Format.
-  `RFC 8142 <https://tools.ietf.org/html/rfc8142>`__ standard: GeoJSON
   Text Sequences (RS separator)
-  `GeoJSONL <https://www.interline.io/blog/geojsonl-extracts/>`__: An
   optimized format for large geographic datasets
-  `JSON streaming on Wikipedia <https://en.wikipedia.org/wiki/JSON_streaming>`__: An
   overview over formats for concatenated JSON in a single file
