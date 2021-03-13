.. _vector.cad:

================================================================================
CAD -- AutoCAD DWG
================================================================================

.. shortname:: CAD

.. build_dependencies:: (internal libopencad provided)

OGR DWG support is based on libopencad, so the list of supported DWG (DXF)
versions can be seen in libopencad documentation. All drawing entities are
separated into layers as they are in DWG file, not in 1 layer as DXF Driver
does.

DWG files are considered to have no georeferencing information through OGR.
Features will all have the following generic attributes:

-  CADGeometry: CAD Type of the presented geometry.
-  Thickness: Thickness of the object drawing units (if it is not
   supported by this type, it is set to 0.0).
-  Color (RGB): IntegerList contains R,G,B components of the color.
-  ExtendedEntity: Where available, extended entity attributes all
   appended to form a single text attribute.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Supported Elements
------------------

The following element types are supported:

-  POINT: Produces a simple point geometry feature.
-  LINE: Translated as a LINESTRING. Rounded polylines (those with their
   vertices' budge attributes set) will be tessellated. Single-vertex
   polylines are translated to POINT.
-  CIRCLE, ARC: Translated as a CIRCULARSTRING.
-  3DFACE: Translated as POLYGON.

The driver is read-only.

See Also
--------

-  `ODA DWG Reference <https://www.opendesign.com/files/guestdownloads/OpenDesign_Specification_for_.dwg_files.pdf>`__
-  `Libopencad repository <https://github.com/nextgis-borsch/lib_opencad>`__
