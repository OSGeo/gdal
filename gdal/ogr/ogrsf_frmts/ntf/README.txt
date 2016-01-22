
	NTF OGR Implementation Notes
	============================


Products (and Layers) Supported
-------------------------------

Landline (and Landline Plus):
	LANDLINE_POINT
	LANDLINE_LINE
	LANDLINE_NAME

Panorama Contours:
	PANORAMA_POINT
	PANORAMA_CONTOUR

	HEIGHT attribute holds elevation.

Strategi:
	STRATEGI_POINT
	STRATEGI_LINE
	STRATEGI_TEXT
	STRATEGI_NODE

Meridian:
	MERIDIAN_POINT
	MERIDIAN_LINE
	MERIDIAN_TEXT
	MERIDIAN_NODE

Boundaryline:
	BOUNDARYLINE_LINK
	BOUNDARYLINE_POLY
	BOUNDARYLINE_COLLECTIONS

	The _POLY layer has links to links allowing true polygons to  
        be formed (otherwise the _POLY's only have a seed point for geometry.
	The collections are collections of polygons (also without geometry
	as read).  This is the only product from which polygons can be
	constructed.
	
Boundary Line 2000:
	BL2000_LINK
	BL2000_POLY
	BL2000_COLLECTIONS

	Similar to Boundaryline, but with different attributes, feature code
	is largely unused, and the _POLY layer doesn't have a seed point.
	
BaseData.GB:
	BASEDATA_POINT
	BASEDATA_LINE
	BASEDATA_TEXT
	BASEDATA_NODE

OSCAR Asset/Traffic:
	OSCAR_POINT
	OSCAR_LINE
	OSCAR_NODE

OSCAR Network:
	OSCAR_NETWORK_POINT
	OSCAR_NETWORK_LINE
	OSCAR_NETWORK_NODE

Address Point:
	ADDRESS_POINT

Code Point:
	CODE_POINT

Code Point Plus:
	CODE_POINT_PLUS

The dataset as a whole will also have a FEATURE_CLASSES layer containing a 
pure table relating FEAT_CODE numbers with feature class names (FC_NAME).  
This applies to all products in the dataset.  A few layer types (such as 
the Code Point, and Address Point products) don't include feature classes.
Some products use features classes that are not defined in the file, and
so they will not appear in the FEATURE_CLASSES layer. 
	
	
Product Schemas
---------------

The approach taken in this reader is to treat one file, or a directory
of files as a single dataset.  All files in the dataset are scanned on 
open.  For each particular product (listed above) a set of layers are
created; however, these layers may be extracted from several files of the
same product. 

The layers are based on a low level feature type in the NTF file, but will
generally contain features of many different feature codes (FEAT_CODE 
attribute).  Different features within a given layer may have a variety of
attributes in the file; however, the schema is established based on the
union of all attributes possible within features of a particular type
(ie. POINT) of that product family (ie. OSCAR Network).

If an NTF product is read that doesn't match one of the known schema's
it will go through a different generic handler which has only 
layers of type GENERIC_POINT and GENERIC_LINE.  The features only have
a FEAT_CODE attribute.  

Details of what layers of what products have what attributes can be found
in the NTFFileReader::EstablishLayers() method at the end of ntf_estlayers.cpp.
This file also contains all the product specific translation code. 


Special Attributes
------------------

FEAT_CODE: General feature code integer, can be used to lookup a name in the
           FEATURE_CLASSES layer/table. 

TEXT_ID/POINT_ID/LINE_ID/NAME_ID/COLL_ID/POLY_ID/GEOM_ID:
          Unique identifier for a feature of the appropriate type.  

TILE_REF: All layers (except FEATURE_CLASSES) contain a TILE_REF attribute
          which indicates which tile (file) the features came from.  Generally
          speaking the id numbers are only unique within the tile and so
          the TILE_REF can be used restrict id links within features from
          the same file. 

FONT/TEXT_HT/DIG_POSTN/ORIENT:
	Detailed information on the font, text height, digitizing position, 
        and orientation of text or name objects.  Review the OS product
        manuals to understand the units, and meaning of these codes. 

GEOM_ID_OF_POINT:
	For _NODE features this defines the POINT_ID of the point layer object
        to which this node corresponds.  Generally speaking the nodes don't
        carry a geometry of their own.  The node must be related to a point
        to establish it's position. 

GEOM_ID_OF_LINK:
	A _list_ of _LINK or _LINE features to end/start at a node.  Nodes,
        and this field are generally only of value when establishing 
        connectivity of line features for network analysis.   Note that this
        should be related to the target features GEOM_ID, not it's LINE_ID.

        On the BOUNDARYLINE_POLY layer this attribute contains the GEOM_IDs
        of the lines which form the edge of the polygon. 

POLY_ID:
	A list of POLY_ID's from the BOUNDARYLINE_POLY layer associated with
        a given collection in the BOUNDARYLINE_COLLECTIONS layer. 


Adding a New Product
--------------------

It is anticipated that over time the UK Ordnance Survey will define new 
product formats, and to get decent milage out of them this library should
be updated to support them.  While I will endevour to do this myself, it
seems prudent to define how it is done in case I am not available to do it,
or am unwilling to do it on a timely basis.  To add a new product type the 
following steps are required:

 o Add an NPC_ code for the product in ntf.h
 o Add a case in NTFFileReader::Open() to translate the GetProduct() result
   into the NPC_ code. 
 o Add a case in NTFFileReader::EstablishLayers() defining the layers found
   on this product.  
 o Add translate functions for layers of this product.  Generally they can
   be cloned from an existing translate function, and the attribute mapping
   in the NTFReader::ApplyAttributeValues() call can be modified. 

Occationally existing products will change slightly.  This may result in a
slight change to the detection logic in NTFFileReader::Open() and changes
in the list of user attributes associated with the layer.  If the differences
are signifiant it may be necessary to define a whole new product family 
type (as is done for Code Point Plus vs. Code Point). 

Generic Products
----------------

In situations where a file is not identified as being part of an existing
known product it will be treated generically.  In this case the entire dataset
is scanned to establish what features have what attributes.  Because of this,
opening a generic dataset can be much slower than opening a recognised dataset.
Based on this scan a list of generic features (layers) are defined from the 
following set:

 GENERIC_POINT
 GENERIC_LINE
 GENERIC_NAME
 GENERIC_TEXT
 GENERIC_POLY
 GENERIC_NODE
 GENERIC_COLLECTION

Generic products are primarily handled by the ntf_generic.cpp module whereas
specific products are handled in ntf_estlayers.cpp.  

Because some data products (OSNI datasets) not from the Ordnance Survey
were found to have record groups in unusual orders compared to what the
UK Ordnance Survey does, it was found necessary to cache all the records of
level 3 and higher generic products, and construct record groups by id
reference from within this cache rather than depending on convenient record
orderings.  This is accomplished by the NTFFileReader "indexing" capability
near the bottom of ntffilereader.cpp.  Because of this in memory indexing
accessing generic datasets can be much more memory intensive than accessing
known data products, though it isn't necessary for generic level 1 and 2
products. 

It is possible to force a known product to be treated as generic by setting
the FORCE_GENERIC option to "ON" using OGRNTFDataSource::SetOptionsList() as
is demonstrated in ntfdump.cpp. 




