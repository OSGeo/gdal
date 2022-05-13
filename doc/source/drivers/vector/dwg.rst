.. _vector.dwg:

AutoCAD DWG
===========

.. shortname:: DWG

.. build_dependencies:: Open Design Alliance Teigha library

OGR supports reading most versions of AutoCAD DWG when built with the
Open Design Alliance Teigha library. DWG is an binary working format used
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

Configuration options
---------------------

The following :ref:`configuration options <configoptions>` are 
available:

- :decl_configoption:`OGR_ARC_STEPSIZE`: The approximation of arcs, 
  ellipses, circles and rounded polylines as linestrings is done by 
  splitting the arcs into subarcs of no more than a threshold angle. 
  This angle is the OGR_ARC_STEPSIZE. This defaults to four degrees, 
  but may be overridden by setting the configuration variable.

- :decl_configoption:`DWG_INLINE_BLOCKS`: The default behavior is for 
  block references to be expanded with the geometry of the block they 
  reference. However, if the :decl_configoption:`DWG_INLINE_BLOCKS` 
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

  The intention is that with :decl_configoption:`DWG_INLINE_BLOCKS` 
  disabled, the block references will remain as references and the 
  original block definitions will be available via the blocks layer.

- :decl_configoption:`DWG_ATTRIBUTES`: If option is set to TRUE value, 
  then block attributes are treated as feature attributes, one feature 
  attribute for each tag. This option allow conversion to rows and 
  columns data such as database tables.

- :decl_configoption:`DWG_ALL_ATTRIBUTES`: If option is set to FALSE value, 
  then block attributes are ignored if the visible property of the tag 
  attribute is false. To see all attributes set 
  :decl_configoption:`DWG_ALL_ATTRIBUTES` to TRUE value (this is the 
  default value).

Building
--------

See :ref:`ODA platform support <vector.oda>` for building GDAL with ODA support.
