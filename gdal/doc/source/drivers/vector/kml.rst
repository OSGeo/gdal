.. _vector.kml:

KML - Keyhole Markup Language
=============================

.. shortname:: KML

.. build_dependencies:: (read support needs libexpat) 

Keyhole Markup Language (KML) is an XML-based language for managing the
display of 3D geospatial data. KML has been accepted as an OGC standard,
and is supported in one way or another on the major GeoBrowsers. Note
that KML by specification uses only a single projection, EPSG:4326. All
OGR KML output will be presented in EPSG:4326. As such OGR will create
layers in the correct coordinate system and transform any geometries.

At this time, only vector layers are handled by the KML driver. *(there
are additional scripts supplied with the GDAL project that can build
other kinds of output)*

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

KML Reading
~~~~~~~~~~~

KML reading is only available if GDAL/OGR is built with the Expat XML
Parser, otherwise only KML writing will be supported.

Supported geometry types are ``Point``, ``Linestring``, ``Polygon``,
``MultiPoint``, ``MultiLineString``, ``MultiPolygon`` and
``MultiGeometry``. There are limitations, for example: the nested nature
of folders in a source KML file is lost; folder ``<description>`` tags
will not carry through to output. Folders containing
multiple geometry types, like POINT and POLYGON, are supported.

KML Writing
~~~~~~~~~~~

Since not all features of KML are able to be represented in the Simple
Features geometry model, you will not be able to generate many
KML-specific attributes from within GDAL/OGR. Please try a few test
files to get a sense of what is possible.

When outputting KML, the OGR KML driver will translate each OGR Layer
into a KML Folder (you may encounter unexpected behavior if you try to
mix the geometry types of elements in a layer, e.g. ``LINESTRING`` and
``POINT`` data).

The KML Driver will rename some layers, or source KML folder names, into
new names it considers valid, for example '``Layer #0``', the default
name of the first unnamed Layer, becomes ``'Layer__0'``.

KML is mix of formatting and feature data. The <description> tag of a
Placemark will be displayed in most geobrowsers as an HTML-filled
balloon. When writing KML, Layer element attributes are added as simple
schema fields. This best preserves feature type information.

Limited support is available for fills, line color and other styling
attributes. Please try a few sample files to get a better sense of
actual behavior.

Encoding issues
~~~~~~~~~~~~~~~

Expat library supports reading the following built-in encodings :

-  US-ASCII
-  UTF-8
-  UTF-16
-  ISO-8859-1
-  Windows-1252

The content returned by OGR will be encoded in UTF-8, after the
conversion from the encoding mentioned in the file header is.

| If your KML file is not encoded in one of the previous encodings, it
  will not be parsed by the KML driver. You may convert it into one of
  the supported encoding with the *iconv* utility for example and change
  accordingly the *encoding* parameter value in the XML header.

When writing a KML file, the driver expects UTF-8 content to be passed
in.

Creation Options
~~~~~~~~~~~~~~~~

The following dataset creation options are supported:

-  **NameField**: Allows you to specify the field to use for the KML
   <name> element. Default value : 'Name'
-  **DescriptionField**: Allows you to specify the field to use for the
   KML <description> element. Default value : 'Description'
-  **AltitudeMode**: Allows you to specify the AltitudeMode to use for
   KML geometries. This will only affect 3D geometries and must be one
   of the valid KML options. See the `relevant KML reference
   material <http://code.google.com/apis/kml/documentation/kml_tags_21.html#altitudemode>`__
   for further information.

   ::

      ogr2ogr -f KML output.kml input.shp -dsco AltitudeMode=absolute

-  **DOCUMENT_ID**\ =string: Starting with GDAL 2.2, the DOCUMENT_ID
   datasource creation option can be used to specified the id of the
   root <Document> node. The default value is root_doc.

VSI Virtual File System API support
-----------------------------------

The driver supports reading and writing to files managed by VSI Virtual
File System API, which include "regular" files, as well as files in the
/vsizip/ (read-write) , /vsigzip/ (read-write) , /vsicurl/ (read-only)
domains.

Writing to /dev/stdout or /vsistdout/ is also supported.

Example
-------

The ogr2ogr utility can be used to dump the results of a PostGIS query
to KML:

::

   ogr2ogr -f KML output.kml PG:'host=myserver dbname=warmerda' -sql "SELECT pop_1994 from canada where province_name = 'Alberta'"

How to dump contents of .kml file as OGR sees it:

::

   ogrinfo -ro somedisplay.kml

Caveats
-------

Google Earth seems to have some limits regarding the number of
coordinates in complex geometries like polygons. If the problem appears,
then problematic geometries are displayed completely or partially
covered by vertical stripes. Unfortunately, there are no exact number
given in the KML specification about this limitation, so the KML driver
will not warn about potential problems. One of possible and tested
solutions is to simplify a line or a polygon to remove some coordinates.
Here is the whole discussion about this issue on the `Google KML
Developer Forum <http://groups.google.com/group/kml-support>`__, in the
`polygon displays with vertical
stripes <http://groups.google.com/group/kml-support-getting-started/browse_thread/thread/e6995b8073e69c41>`__
thread.

See Also
--------

-  `KML Specification <https://developers.google.com/kml/?csw=1>`__
-  `KML
   Tutorial <https://developers.google.com/kml/documentation/kml_tut>`__
-  :ref:`LIBKML driver <vector.libkml>` An alternative GDAL KML driver
