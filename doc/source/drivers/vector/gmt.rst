.. _vector.gmt:

GMT ASCII Vectors (.gmt)
========================

.. shortname:: GMT

.. built_in_by_default::

OGR supports reading and writing GMT ASCII vector format. This is the
format used by the Generic Mapping Tools (GMT) package, and includes
recent additions to the format to handle more geometry types, and
attributes. Currently GMT files are only supported if they have the
extension ".gmt". Old (simple) GMT files are treated as either point, or
linestring files depending on whether a ">" line is encountered before
the first vertex. New style files have a variety of auxiliary
information including geometry type, layer extents, coordinate system
and attribute field declarations in comments in the header, and for each
feature can have attributes.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creation Issues
---------------

The driver supports creating new GMT files, and appending additional
features to existing files, but update of existing features is not
supported. Each layer is created as a separate .gmt file. If a name that
ends with .gmt is not given, then the GMT driver will take the layer
name and add the ".gmt" extension.

Writing to /dev/stdout or /vsistdout/ is supported since GDAL 3.5.0 (note
that the file will then lack the optional region/bounding box header item)
