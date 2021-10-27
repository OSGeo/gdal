.. _vector.avcbin:

================================================================================
Arc/Info Binary Coverage
================================================================================

.. shortname:: AVCBIN

.. built_in_by_default::

Arc/Info Binary Coverages (eg. Arc/Info V7 and earlier) are supported by OGR
for read access.

The label, arc, polygon, centroid, region and text sections of a coverage are
all supported as layers. Attributes from INFO are appended to labels, arcs,
polygons or region where appropriate. When available the projection information
is read and translated. Polygon geometries are collected for polygon and region
layers from the composing arcs.

Text sections are represented as point layers. Display height is preserved in
the HEIGHT attribute field; however, other information about text orientation
is discarded.

Info tables associated with a coverage, but not specifically named to be
attached to one of the existing geometric layers is currently not accessible
through OGR. Note that info tables are stored in an 'info' directory at the
same level as the coverage directory. If this is inaccessible or corrupt no
info attributes will be appended to coverage layers, but the geometry should
still be accessible.

If the directory contains files with names like w001001.adf then the coverage
is a :ref:`grid coverage <raster.aig>` suitable
to read with GDAL, not a vector coverage supported by OGR.

The layers are named as follows:

#. A label layer (polygon labels, or free standing points) is named LAB
   if present.
#. A centroid layer (polygon centroids) is named CNT if present.
#. An arc (line) layer is named ARC if present.
#. A polygon layer is named "PAL" if present.
#. A text section is named according to the section subclass.
#. A region subclass is named according to the subclass name.

The Arc/Info binary coverage driver attempts to optimize spatial queries but
due to the lack of a spatial index this is just accomplished by minimizing
processing for features not within the spatial window.

Random (by FID) reads of arcs, and polygons is supported it may not be
supported for other feature types.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `AVCE00 Library Page <http://avce00.maptools.org/>`__
-  :ref:`AVCE00 OGR Driver (.E00) <vector.avce00>`
