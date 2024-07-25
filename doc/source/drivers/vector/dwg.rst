.. _vector.dwg:

AutoCAD DWG
===========

.. shortname:: DWG

.. build_dependencies:: Open Design Alliance Teigha library

OGR supports reading most versions of AutoCAD DWG when built with the
Open Design Alliance Teigha library. DWG is a binary working format used
for AutoCAD drawings. A reasonable effort has been made to make the OGR
DWG driver work similarly to the OGR DXF driver which shares a common
data model. The entire contents of the .dwg file is represented as a
single layer named "entities".

DWG files are considered to have no georeferencing information through
OGR. Features will all have the following generic attributes:

-  Layer: The name of the DXF layer. The default layer is "0".
-  SubClasses: Where available, a list of classes to which an element
   belongs.
-  ExtendedEntity: Where available, extended entity attributes all
   appended to form a single text attribute.
-  Linetype: Where available, the line type used for this entity.
-  EntityHandle: The hexadecimal entity handle. A sort of feature id.
-  Text: The text of labels.

A reasonable attempt is made to preserve line color, line width, text
size and orientation via OGR feature styling information when
translating elements. Currently no effort is made to preserve fill
styles or complex line style attributes.

The :config:`OGR_ARC_STEPSIZE` and :config:`OGR_ARC_MAX_GAP` configurations
options control the approximation of arcs, ellipses, circles and rounded
polylines as linestrings.

Configuration options
---------------------

|about-config-options|
The following configuration options are available:

- .. config:: DWG_INLINE_BLOCKS
     :choices: TRUE, FALSE

     The default behavior is for
     block references to be expanded with the geometry of the block they
     reference. However, if the :config:`DWG_INLINE_BLOCKS`
     configuration option is set to the value FALSE, then the behavior is
     different as described here:

     - A new layer will be available called blocks. It will contain one or
       more features for each block defined in the file. In addition to the
       usual attributes, they will also have a BlockName attribute indicate
       what block they are part of.
     - The entities layer will have new attributes BlockName, BlockScale,
       BlockAngle and BlockAttributes.
     - BlockAttributes will be a list of (tag x value) pairs of all
       visible attributes (JSON encoded).
     - block referenced will populate these new fields with the
       corresponding information (they are null for all other entities).
     - block references will not have block geometry inlined - instead they
       will have a point geometry for the insertion point.

     The intention is that with :config:`DWG_INLINE_BLOCKS`
     disabled, the block references will remain as references and the
     original block definitions will be available via the blocks layer.

- .. config:: DWG_ATTRIBUTES
     :choices: TRUE, FALSE

     If option is set to TRUE value,
     then block attributes are treated as feature attributes, one feature
     attribute for each tag. This option allow conversion to rows and
     columns data such as database tables.

- .. config:: DWG_ALL_ATTRIBUTES
     :choices: TRUE, FALSE
     :default: TRUE

     If option is set to FALSE value,
     then block attributes are ignored if the visible property of the tag
     attribute is false. To see all attributes set
     :config:`DWG_ALL_ATTRIBUTES` to TRUE value (this is the
     default value).

- .. config:: DWG_CLOSED_LINE_AS_POLYGON
     :choices: TRUE, FALSE
     :default: FALSE
     :since: 3.10

     This option can be set to TRUE specified to ask for closed POLYLINE and
     LWPOLYLINE to be exposed as OGR polygons.

Building
--------

See :ref:`ODA platform support <vector.oda>` for building GDAL with ODA support.
