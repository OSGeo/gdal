.. _vector.jml:

JML: OpenJUMP JML format
========================

.. shortname:: JML

.. build_dependencies:: (read support needs libexpat) 

OGR has support for reading and writing .JML files used by the OpenJUMP
software. Read support is only available if GDAL is built with *expat*
library support

.jml is a variant of GML format. There is no formal definition of the
format. It supports a single layer per file, mixed geometry types, and
for each feature, a geometry and several attributes of type integer,
double, string, date or object. That object data type, used for example
to store 64 bit integers, but potentially arbitrary serialized Java
objects, is converted as string when reading. Contrary to GML, the
definition of fields is embedded in the .jml file, at its beginning.

Support for reading and writing spatial reference systems requires GDAL
2.3 or later.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Encoding issues
---------------

Expat library supports reading the following built-in encodings :

-  US-ASCII
-  UTF-8
-  UTF-16
-  ISO-8859-1
-  Windows-1252

The content returned by OGR will be encoded in UTF-8, after the
conversion from the encoding mentioned in the file header is. But files
produced by OpenJUMP are always UTF-8 encoded.

When writing a JML file, the driver expects UTF-8 content to be passed
in.

Styling
-------

OpenJUMP uses an optional string attribute called "R_G_B" to determine
the color of objects. The field value is "RRGGBB" where RR, GG, BB are
respectively the value of the red, green and blue components expressed
as hexadecimal values from 00 to FF. When reading a .jml file, OGR will
translate the R_G_B attribute to the Feature Style encoding, unless a
OGR_STYLE attribute is present. When writing a .jml file, OGR will
extract from the Feature Style string the color of the PEN tool or the
forecolor of the BRUSH tool to write the R_G_B attribute, unless the
R_G_B attribute is defined in the provided feature. The addition of the
R_G_B attribute can be disabled by setting the CREATE_R_G_B_FIELD layer
creation option to NO.

Creation Issues
---------------

The JML writer supports the following *layer* creation options:

-  **CREATE_R_G_B_FIELD**\ =YES/NO: whether the create a R_G_B field
   that will contain the color of the PEN tool or the forecolor of the
   BRUSH tool of the OGR Feature Style string. Default value : YES
-  **CREATE_OGR_STYLE_FIELD**\ =YES/NO: whether the create a OGR_STYLE
   field that will contain the Feature Style string. Default value : NO

See Also
--------

- :ref:`ogr_feature_style`

Credits
-------

The author wishes to thank Jukka Rahkonen for funding the development of
this driver.
