%extend OGRGeometryShadow {
// File: ogrgeometry_8cpp.xml
%feature("docstring")  DumpReadable "void
OGR_G_DumpReadable(OGRGeometryH hGeom, FILE *fp, const char
*pszPrefix)

Dump geometry in well known text format to indicated output file.

This method is the same as the CPP method OGRGeometry::dumpReadable.

Parameters:
-----------

hGeom:  handle on the geometry to dump.

fp:  the text file to write the geometry to.

pszPrefix:  the prefix to put on each line of output. ";

%feature("docstring")  AssignSpatialReference "void
OGR_G_AssignSpatialReference(OGRGeometryH hGeom, OGRSpatialReferenceH
hSRS)

Assign spatial reference to this object.

Any existing spatial reference is replaced, but under no circumstances
does this result in the object being reprojected. It is just changing
the interpretation of the existing geometry. Note that assigning a
spatial reference increments the reference count on the
OGRSpatialReference, but does not copy it.

Starting with GDAL 2.3, this will also assign the spatial reference to
potential sub-geometries of the geometry ( OGRGeometryCollection,
OGRCurvePolygon/OGRPolygon, OGRCompoundCurve, OGRPolyhedralSurface and
their derived classes).

This is similar to the SFCOM IGeometry::put_SpatialReference() method.

This function is the same as the CPP method
OGRGeometry::assignSpatialReference.

Parameters:
-----------

hGeom:  handle on the geometry to apply the new spatial reference
system.

hSRS:  handle on the new spatial reference system to apply. ";

%feature("docstring")  Intersects "int OGR_G_Intersects(OGRGeometryH
hGeom, OGRGeometryH hOtherGeom)

Do these features intersect?

Determines whether two geometries intersect. If GEOS is enabled, then
this is done in rigorous fashion otherwise TRUE is returned if the
envelopes (bounding boxes) of the two geometries overlap.

This function is the same as the CPP method OGRGeometry::Intersects.

Parameters:
-----------

hGeom:  handle on the first geometry.

hOtherGeom:  handle on the other geometry to test against.

TRUE if the geometries intersect, otherwise FALSE. ";

%feature("docstring")  TransformTo "OGRErr
OGR_G_TransformTo(OGRGeometryH hGeom, OGRSpatialReferenceH hSRS)

Transform geometry to new spatial reference system.

This function will transform the coordinates of a geometry from their
current spatial reference system to a new target spatial reference
system. Normally this means reprojecting the vectors, but it could
include datum shifts, and changes of units.

This function will only work if the geometry already has an assigned
spatial reference system, and if it is transformable to the target
coordinate system.

Because this function requires internal creation and initialization of
an OGRCoordinateTransformation object it is significantly more
expensive to use this function to transform many geometries than it is
to create the OGRCoordinateTransformation in advance, and call
transform() with that transformation. This function exists primarily
for convenience when only transforming a single geometry.

This function is the same as the CPP method OGRGeometry::transformTo.

Parameters:
-----------

hGeom:  handle on the geometry to apply the transform to.

hSRS:  handle on the spatial reference system to apply.

OGRERR_NONE on success, or an error code. ";

%feature("docstring")  Transform "OGRErr OGR_G_Transform(OGRGeometryH
hGeom, OGRCoordinateTransformationH hTransform)

Apply arbitrary coordinate transformation to geometry.

This function will transform the coordinates of a geometry from their
current spatial reference system to a new target spatial reference
system. Normally this means reprojecting the vectors, but it could
include datum shifts, and changes of units.

Note that this function does not require that the geometry already
have a spatial reference system. It will be assumed that they can be
treated as having the source spatial reference system of the
OGRCoordinateTransformation object, and the actual SRS of the geometry
will be ignored. On successful completion the output
OGRSpatialReference of the OGRCoordinateTransformation will be
assigned to the geometry.

This function is the same as the CPP method OGRGeometry::transform.

Parameters:
-----------

hGeom:  handle on the geometry to apply the transform to.

hTransform:  handle on the transformation to apply.

OGRERR_NONE on success or an error code. ";

%feature("docstring")  Segmentize "void OGR_G_Segmentize(OGRGeometryH
hGeom, double dfMaxLength)

Modify the geometry such it has no segment longer then the given
distance.

Interpolated points will have Z and M values (if needed) set to 0.
Distance computation is performed in 2d only.

This function is the same as the CPP method OGRGeometry::segmentize().

Parameters:
-----------

hGeom:  handle on the geometry to segmentize

dfMaxLength:  the maximum distance between 2 points after
segmentization ";

%feature("docstring")  GetDimension "int
OGR_G_GetDimension(OGRGeometryH hGeom)

Get the dimension of this geometry.

This function corresponds to the SFCOM IGeometry::GetDimension()
method. It indicates the dimension of the geometry, but does not
indicate the dimension of the underlying space (as indicated by
OGR_G_GetCoordinateDimension() function).

This function is the same as the CPP method
OGRGeometry::getDimension().

Parameters:
-----------

hGeom:  handle on the geometry to get the dimension from.

0 for points, 1 for lines and 2 for surfaces. ";

%feature("docstring")  GetCoordinateDimension "int
OGR_G_GetCoordinateDimension(OGRGeometryH hGeom)

Get the dimension of the coordinates in this geometry.

This function is the same as the CPP method
OGRGeometry::getCoordinateDimension().

Parameters:
-----------

hGeom:  handle on the geometry to get the dimension of the coordinates
from.

Deprecated use OGR_G_CoordinateDimension(), OGR_G_Is3D(), or
OGR_G_IsMeasured().

this will return 2 or 3. ";

%feature("docstring")  CoordinateDimension "int
OGR_G_CoordinateDimension(OGRGeometryH hGeom)

Get the dimension of the coordinates in this geometry.

This function is the same as the CPP method
OGRGeometry::CoordinateDimension().

Parameters:
-----------

hGeom:  handle on the geometry to get the dimension of the coordinates
from.

this will return 2 for XY, 3 for XYZ and XYM, and 4 for XYZM data.

GDAL 2.1 ";

%feature("docstring")  Is3D "int OGR_G_Is3D(OGRGeometryH hGeom)

See whether this geometry has Z coordinates.

This function is the same as the CPP method OGRGeometry::Is3D().

Parameters:
-----------

hGeom:  handle on the geometry to check whether it has Z coordinates.

TRUE if the geometry has Z coordinates.

GDAL 2.1 ";

%feature("docstring")  IsMeasured "int OGR_G_IsMeasured(OGRGeometryH
hGeom)

See whether this geometry is measured.

This function is the same as the CPP method OGRGeometry::IsMeasured().

Parameters:
-----------

hGeom:  handle on the geometry to check whether it is measured.

TRUE if the geometry has M coordinates.

GDAL 2.1 ";

%feature("docstring")  SetCoordinateDimension "void
OGR_G_SetCoordinateDimension(OGRGeometryH hGeom, int nNewDimension)

Set the coordinate dimension.

This method sets the explicit coordinate dimension. Setting the
coordinate dimension of a geometry to 2 should zero out any existing Z
values. Setting the dimension of a geometry collection, a compound
curve, a polygon, etc. will affect the children geometries. This will
also remove the M dimension if present before this call.

Deprecated use OGR_G_Set3D() or OGR_G_SetMeasured().

Parameters:
-----------

hGeom:  handle on the geometry to set the dimension of the
coordinates.

nNewDimension:  New coordinate dimension value, either 2 or 3. ";

%feature("docstring")  Set3D "void OGR_G_Set3D(OGRGeometryH hGeom,
int bIs3D)

Add or remove the Z coordinate dimension.

This method adds or removes the explicit Z coordinate dimension.
Removing the Z coordinate dimension of a geometry will remove any
existing Z values. Adding the Z dimension to a geometry collection, a
compound curve, a polygon, etc. will affect the children geometries.

Parameters:
-----------

hGeom:  handle on the geometry to set or unset the Z dimension.

bIs3D:  Should the geometry have a Z dimension, either TRUE or FALSE.

GDAL 2.1 ";

%feature("docstring")  SetMeasured "void
OGR_G_SetMeasured(OGRGeometryH hGeom, int bIsMeasured)

Add or remove the M coordinate dimension.

This method adds or removes the explicit M coordinate dimension.
Removing the M coordinate dimension of a geometry will remove any
existing M values. Adding the M dimension to a geometry collection, a
compound curve, a polygon, etc. will affect the children geometries.

Parameters:
-----------

hGeom:  handle on the geometry to set or unset the M dimension.

bIsMeasured:  Should the geometry have a M dimension, either TRUE or
FALSE.

GDAL 2.1 ";

%feature("docstring")  Equals "int OGR_G_Equals(OGRGeometryH hGeom,
OGRGeometryH hOther)

Returns TRUE if two geometries are equivalent.

This operation implements the SQL/MM ST_OrderingEquals() operation.

The comparison is done in a structural way, that is to say that the
geometry types must be identical, as well as the number and ordering
of sub-geometries and vertices. Or equivalently, two geometries are
considered equal by this method if their WKT/WKB representation is
equal. Note: this must be distinguished for equality in a spatial way
(which is the purpose of the ST_Equals() operation).

This function is the same as the CPP method OGRGeometry::Equals()
method.

Parameters:
-----------

hGeom:  handle on the first geometry.

hOther:  handle on the other geometry to test against.

TRUE if equivalent or FALSE otherwise. ";

%feature("docstring")  WkbSize "int OGR_G_WkbSize(OGRGeometryH hGeom)

Returns size of related binary representation.

This function returns the exact number of bytes required to hold the
well known binary representation of this geometry object. Its
computation may be slightly expensive for complex geometries.

This function relates to the SFCOM IWks::WkbSize() method.

This function is the same as the CPP method OGRGeometry::WkbSize().

Parameters:
-----------

hGeom:  handle on the geometry to get the binary size from.

size of binary representation in bytes. ";

%feature("docstring")  GetEnvelope "void
OGR_G_GetEnvelope(OGRGeometryH hGeom, OGREnvelope *psEnvelope)

Computes and returns the bounding envelope for this geometry in the
passed psEnvelope structure.

This function is the same as the CPP method
OGRGeometry::getEnvelope().

Parameters:
-----------

hGeom:  handle of the geometry to get envelope from.

psEnvelope:  the structure in which to place the results. ";

%feature("docstring")  GetEnvelope3D "void
OGR_G_GetEnvelope3D(OGRGeometryH hGeom, OGREnvelope3D *psEnvelope)

Computes and returns the bounding envelope (3D) for this geometry in
the passed psEnvelope structure.

This function is the same as the CPP method
OGRGeometry::getEnvelope().

Parameters:
-----------

hGeom:  handle of the geometry to get envelope from.

psEnvelope:  the structure in which to place the results.

OGR 1.9.0 ";

%feature("docstring")  ImportFromWkb "OGRErr
OGR_G_ImportFromWkb(OGRGeometryH hGeom, const void *pabyData, int
nSize)

Assign geometry from well known binary data.

The object must have already been instantiated as the correct derived
type of geometry object to match the binaries type.

This function relates to the SFCOM IWks::ImportFromWKB() method.

This function is the same as the CPP method
OGRGeometry::importFromWkb().

Parameters:
-----------

hGeom:  handle on the geometry to assign the well know binary data to.

pabyData:  the binary input data.

nSize:  the size of pabyData in bytes, or -1 if not known.

OGRERR_NONE if all goes well, otherwise any of OGRERR_NOT_ENOUGH_DATA,
OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or OGRERR_CORRUPT_DATA may be
returned. ";

%feature("docstring")  ExportToWkb "OGRErr
OGR_G_ExportToWkb(OGRGeometryH hGeom, OGRwkbByteOrder eOrder, unsigned
char *pabyDstBuffer)

Convert a geometry well known binary format.

This function relates to the SFCOM IWks::ExportToWKB() method.

For backward compatibility purposes, it exports the Old-style 99-402
extended dimension (Z) WKB types for types Point, LineString, Polygon,
MultiPoint, MultiLineString, MultiPolygon and GeometryCollection. For
other geometry types, it is equivalent to OGR_G_ExportToIsoWkb().

This function is the same as the CPP method
OGRGeometry::exportToWkb(OGRwkbByteOrder, unsigned char *,
OGRwkbVariant) with eWkbVariant = wkbVariantOldOgc.

Parameters:
-----------

hGeom:  handle on the geometry to convert to a well know binary data
from.

eOrder:  One of wkbXDR or wkbNDR indicating MSB or LSB byte order
respectively.

pabyDstBuffer:  a buffer into which the binary representation is
written. This buffer must be at least OGR_G_WkbSize() byte in size.

Currently OGRERR_NONE is always returned. ";

%feature("docstring")  ExportToIsoWkb "OGRErr
OGR_G_ExportToIsoWkb(OGRGeometryH hGeom, OGRwkbByteOrder eOrder,
unsigned char *pabyDstBuffer)

Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known
binary format.

This function relates to the SFCOM IWks::ExportToWKB() method. It
exports the SFSQL 1.2 and ISO SQL/MM Part 3 extended dimension (Z&M)
WKB types.

This function is the same as the CPP method
OGRGeometry::exportToWkb(OGRwkbByteOrder, unsigned char *,
OGRwkbVariant) with eWkbVariant = wkbVariantIso.

Parameters:
-----------

hGeom:  handle on the geometry to convert to a well know binary data
from.

eOrder:  One of wkbXDR or wkbNDR indicating MSB or LSB byte order
respectively.

pabyDstBuffer:  a buffer into which the binary representation is
written. This buffer must be at least OGR_G_WkbSize() byte in size.

Currently OGRERR_NONE is always returned.

GDAL 2.0 ";

%feature("docstring")  ImportFromWkt "OGRErr
OGR_G_ImportFromWkt(OGRGeometryH hGeom, char **ppszSrcText)

Assign geometry from well known text data.

The object must have already been instantiated as the correct derived
type of geometry object to match the text type.

This function relates to the SFCOM IWks::ImportFromWKT() method.

This function is the same as the CPP method
OGRGeometry::importFromWkt().

Parameters:
-----------

hGeom:  handle on the geometry to assign well know text data to.

ppszSrcText:  pointer to a pointer to the source text. The pointer is
updated to pointer after the consumed text.

OGRERR_NONE if all goes well, otherwise any of OGRERR_NOT_ENOUGH_DATA,
OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or OGRERR_CORRUPT_DATA may be
returned. ";

%feature("docstring")  ExportToWkt "OGRErr
OGR_G_ExportToWkt(OGRGeometryH hGeom, char **ppszSrcText)

Convert a geometry into well known text format.

This function relates to the SFCOM IWks::ExportToWKT() method.

For backward compatibility purposes, it exports the Old-style 99-402
extended dimension (Z) WKB types for types Point, LineString, Polygon,
MultiPoint, MultiLineString, MultiPolygon and GeometryCollection. For
other geometry types, it is equivalent to OGR_G_ExportToIsoWkt().

This function is the same as the CPP method
OGRGeometry::exportToWkt().

Parameters:
-----------

hGeom:  handle on the geometry to convert to a text format from.

ppszSrcText:  a text buffer is allocated by the program, and assigned
to the passed pointer. After use, *ppszDstText should be freed with
CPLFree().

Currently OGRERR_NONE is always returned. ";

%feature("docstring")  ExportToIsoWkt "OGRErr
OGR_G_ExportToIsoWkt(OGRGeometryH hGeom, char **ppszSrcText)

Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known text
format.

This function relates to the SFCOM IWks::ExportToWKT() method. It
exports the SFSQL 1.2 and ISO SQL/MM Part 3 extended dimension (Z&M)
WKB types.

This function is the same as the CPP method
OGRGeometry::exportToWkt(wkbVariantIso).

Parameters:
-----------

hGeom:  handle on the geometry to convert to a text format from.

ppszSrcText:  a text buffer is allocated by the program, and assigned
to the passed pointer. After use, *ppszDstText should be freed with
CPLFree().

Currently OGRERR_NONE is always returned.

GDAL 2.0 ";

%feature("docstring")  GetGeometryType "OGRwkbGeometryType
OGR_G_GetGeometryType(OGRGeometryH hGeom)

Fetch geometry type.

Note that the geometry type may include the 2.5D flag. To get a 2D
flattened version of the geometry type apply the wkbFlatten() macro to
the return result.

This function is the same as the CPP method
OGRGeometry::getGeometryType().

Parameters:
-----------

hGeom:  handle on the geometry to get type from.

the geometry type code. ";

%feature("docstring")  GetGeometryName "const char*
OGR_G_GetGeometryName(OGRGeometryH hGeom)

Fetch WKT name for geometry type.

There is no SFCOM analog to this function.

This function is the same as the CPP method
OGRGeometry::getGeometryName().

Parameters:
-----------

hGeom:  handle on the geometry to get name from.

name used for this geometry type in well known text format. ";

%feature("docstring")  Clone "OGRGeometryH OGR_G_Clone(OGRGeometryH
hGeom)

Make a copy of this object.

This function relates to the SFCOM IGeometry::clone() method.

This function is the same as the CPP method OGRGeometry::clone().

Parameters:
-----------

hGeom:  handle on the geometry to clone from.

a handle on the copy of the geometry with the spatial reference
system as the original. ";

%feature("docstring")  GetSpatialReference "OGRSpatialReferenceH
OGR_G_GetSpatialReference(OGRGeometryH hGeom)

Returns spatial reference system for geometry.

This function relates to the SFCOM IGeometry::get_SpatialReference()
method.

This function is the same as the CPP method
OGRGeometry::getSpatialReference().

Parameters:
-----------

hGeom:  handle on the geometry to get spatial reference from.

a reference to the spatial reference geometry. ";

%feature("docstring")  Empty "void OGR_G_Empty(OGRGeometryH hGeom)

Clear geometry information.

This restores the geometry to its initial state after construction,
and before assignment of actual geometry.

This function relates to the SFCOM IGeometry::Empty() method.

This function is the same as the CPP method OGRGeometry::empty().

Parameters:
-----------

hGeom:  handle on the geometry to empty. ";

%feature("docstring")  IsEmpty "int OGR_G_IsEmpty(OGRGeometryH hGeom)

Test if the geometry is empty.

This method is the same as the CPP method OGRGeometry::IsEmpty().

Parameters:
-----------

hGeom:  The Geometry to test.

TRUE if the geometry has no points, otherwise FALSE. ";

%feature("docstring")  IsValid "int OGR_G_IsValid(OGRGeometryH hGeom)

Test if the geometry is valid.

This function is the same as the C++ method OGRGeometry::IsValid().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always return FALSE.

Parameters:
-----------

hGeom:  The Geometry to test.

TRUE if the geometry has no points, otherwise FALSE. ";

%feature("docstring")  IsSimple "int OGR_G_IsSimple(OGRGeometryH
hGeom)

Returns TRUE if the geometry is simple.

Returns TRUE if the geometry has no anomalous geometric points, such
as self intersection or self tangency. The description of each
instantiable geometric class will include the specific conditions that
cause an instance of that class to be classified as not simple.

This function is the same as the C++ method OGRGeometry::IsSimple()
method.

If OGR is built without the GEOS library, this function will always
return FALSE.

Parameters:
-----------

hGeom:  The Geometry to test.

TRUE if object is simple, otherwise FALSE. ";

%feature("docstring")  IsRing "int OGR_G_IsRing(OGRGeometryH hGeom)

Test if the geometry is a ring.

This function is the same as the C++ method OGRGeometry::IsRing().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always return FALSE.

Parameters:
-----------

hGeom:  The Geometry to test.

TRUE if the geometry has no points, otherwise FALSE. ";

%feature("docstring")  OGRFromOGCGeomType "OGRwkbGeometryType
OGRFromOGCGeomType(const char *pszGeomType)

Map OGCgeometry format type to corresponding OGR constants.

Parameters:
-----------

pszGeomType:  POINT[ ][Z][M], LINESTRING[ ][Z][M], etc...

OGR constant. ";

%feature("docstring")  OGRToOGCGeomType "const char*
OGRToOGCGeomType(OGRwkbGeometryType eGeomType)

Map OGR geometry format constants to corresponding OGC geometry type.

Parameters:
-----------

eGeomType:  OGR geometry type

string with OGC geometry type (without dimensionality) ";

%feature("docstring")  OGRGeometryTypeToName "const char*
OGRGeometryTypeToName(OGRwkbGeometryType eType)

Fetch a human readable name corresponding to an OGRwkbGeometryType
value.

The returned value should not be modified, or freed by the
application.

This function is C callable.

Parameters:
-----------

eType:  the geometry type.

internal human readable string, or NULL on failure. ";

%feature("docstring")  OGRMergeGeometryTypes "OGRwkbGeometryType
OGRMergeGeometryTypes(OGRwkbGeometryType eMain, OGRwkbGeometryType
eExtra)

Find common geometry type.

Given two geometry types, find the most specific common type. Normally
used repeatedly with the geometries in a layer to try and establish
the most specific geometry type that can be reported for the layer.

NOTE: wkbUnknown is the \"worst case\" indicating a mixture of
geometry types with nothing in common but the base geometry type.
wkbNone should be used to indicate that no geometries have been
encountered yet, and means the first geometry encountered will
establish the preliminary type.

Parameters:
-----------

eMain:  the first input geometry type.

eExtra:  the second input geometry type.

the merged geometry type. ";

%feature("docstring")  OGRMergeGeometryTypesEx "OGRwkbGeometryType
OGRMergeGeometryTypesEx(OGRwkbGeometryType eMain, OGRwkbGeometryType
eExtra, int bAllowPromotingToCurves)

Find common geometry type.

Given two geometry types, find the most specific common type. Normally
used repeatedly with the geometries in a layer to try and establish
the most specific geometry type that can be reported for the layer.

NOTE: wkbUnknown is the \"worst case\" indicating a mixture of
geometry types with nothing in common but the base geometry type.
wkbNone should be used to indicate that no geometries have been
encountered yet, and means the first geometry encountered will
establish the preliminary type.

If bAllowPromotingToCurves is set to TRUE, mixing Polygon and
CurvePolygon will return CurvePolygon. Mixing LineString,
CircularString, CompoundCurve will return CompoundCurve. Mixing
MultiPolygon and MultiSurface will return MultiSurface. Mixing
MultiCurve and MultiLineString will return MultiCurve.

Parameters:
-----------

eMain:  the first input geometry type.

eExtra:  the second input geometry type.

bAllowPromotingToCurves:  determine if promotion to curve type must be
done.

the merged geometry type.

GDAL 2.0 ";

%feature("docstring")  FlattenTo2D "void
OGR_G_FlattenTo2D(OGRGeometryH hGeom)

Convert geometry to strictly 2D.

In a sense this converts all Z coordinates to 0.0.

This function is the same as the CPP method
OGRGeometry::flattenTo2D().

Parameters:
-----------

hGeom:  handle on the geometry to convert. ";

%feature("docstring")  OGRSetGenerate_DB2_V72_BYTE_ORDER "OGRErr
OGRSetGenerate_DB2_V72_BYTE_ORDER(int bGenerate_DB2_V72_BYTE_ORDER)

Special entry point to enable the hack for generating DB2 V7.2 style
WKB.

DB2 seems to have placed (and require) an extra 0x30 or'ed with the
byte order in WKB. This entry point is used to turn on or off the
generation of such WKB. ";

%feature("docstring")  OGRGetGenerate_DB2_V72_BYTE_ORDER "int
OGRGetGenerate_DB2_V72_BYTE_ORDER() ";

%feature("docstring")  Distance "double OGR_G_Distance(OGRGeometryH
hFirst, OGRGeometryH hOther)

Compute distance between two geometries.

Returns the shortest distance between the two geometries. The distance
is expressed into the same unit as the coordinates of the geometries.

This function is the same as the C++ method OGRGeometry::Distance().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hFirst:  the first geometry to compare against.

hOther:  the other geometry to compare against.

the distance between the geometries or -1 if an error occurs. ";

%feature("docstring")  Distance3D "double
OGR_G_Distance3D(OGRGeometryH hFirst, OGRGeometryH hOther)

Returns the 3D distance between two geometries.

The distance is expressed into the same unit as the coordinates of the
geometries.

This method is built on the SFCGAL library, check it for the
definition of the geometry operation. If OGR is built without the
SFCGAL library, this method will always return -1.0

This function is the same as the C++ method OGRGeometry::Distance3D().

Parameters:
-----------

hFirst:  the first geometry to compare against.

hOther:  the other geometry to compare against.

distance between the two geometries

GDAL 2.2

the distance between the geometries or -1 if an error occurs. ";

%feature("docstring")  MakeValid "OGRGeometryH
OGR_G_MakeValid(OGRGeometryH hGeom)

Attempts to make an invalid geometry valid without losing vertices.

Already-valid geometries are cloned without further intervention.

This function is the same as the C++ method OGRGeometry::MakeValid().

This function is built on the GEOS >= 3.8 library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
>= 3.8 library, this function will return a clone of the input
geometry if it is valid, or NULL if it is invalid

Parameters:
-----------

hGeom:  The Geometry to make valid.

a newly allocated geometry now owned by the caller, or NULL on
failure.

GDAL 3.0 ";

%feature("docstring")  ConvexHull "OGRGeometryH
OGR_G_ConvexHull(OGRGeometryH hTarget)

Compute convex hull.

A new geometry object is created and returned containing the convex
hull of the geometry on which the method is invoked.

This function is the same as the C++ method OGRGeometry::ConvexHull().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hTarget:  The Geometry to calculate the convex hull of.

a handle to a newly allocated geometry now owned by the caller, or
NULL on failure. ";

%feature("docstring")  Boundary "OGRGeometryH
OGR_G_Boundary(OGRGeometryH hTarget)

Compute boundary.

A new geometry object is created and returned containing the boundary
of the geometry on which the method is invoked.

This function is the same as the C++ method OGR_G_Boundary().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hTarget:  The Geometry to calculate the boundary of.

a handle to a newly allocated geometry now owned by the caller, or
NULL on failure.

OGR 1.8.0 ";

%feature("docstring")  GetBoundary "OGRGeometryH
OGR_G_GetBoundary(OGRGeometryH hTarget)

Compute boundary (deprecated)

Deprecated

See:   OGR_G_Boundary() ";

%feature("docstring")  Buffer "OGRGeometryH OGR_G_Buffer(OGRGeometryH
hTarget, double dfDist, int nQuadSegs)

Compute buffer of geometry.

Builds a new geometry containing the buffer region around the geometry
on which it is invoked. The buffer is a polygon containing the region
within the buffer distance of the original geometry.

Some buffer sections are properly described as curves, but are
converted to approximate polygons. The nQuadSegs parameter can be used
to control how many segments should be used to define a 90 degree
curve - a quadrant of a circle. A value of 30 is a reasonable default.
Large values result in large numbers of vertices in the resulting
buffer geometry while small numbers reduce the accuracy of the result.

This function is the same as the C++ method OGRGeometry::Buffer().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hTarget:  the geometry.

dfDist:  the buffer distance to be applied. Should be expressed into
the same unit as the coordinates of the geometry.

nQuadSegs:  the number of segments used to approximate a 90 degree
(quadrant) of curvature.

the newly created geometry, or NULL if an error occurs. ";

%feature("docstring")  Intersection "OGRGeometryH
OGR_G_Intersection(OGRGeometryH hThis, OGRGeometryH hOther)

Compute intersection.

Generates a new geometry which is the region of intersection of the
two geometries operated on. The OGR_G_Intersects() function can be
used to test if two geometries intersect.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method
OGRGeometry::Intersection().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry.

hOther:  the other geometry.

a new geometry representing the intersection or NULL if there is no
intersection or an error occurs. ";

%feature("docstring")  Union "OGRGeometryH OGR_G_Union(OGRGeometryH
hThis, OGRGeometryH hOther)

Compute union.

Generates a new geometry which is the region of union of the two
geometries operated on.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method OGRGeometry::Union().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry.

hOther:  the other geometry.

a new geometry representing the union or NULL if an error occurs. ";

%feature("docstring")  UnionCascaded "OGRGeometryH
OGR_G_UnionCascaded(OGRGeometryH hThis)

Compute union using cascading.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method
OGRGeometry::UnionCascaded().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry.

a new geometry representing the union or NULL if an error occurs. ";

%feature("docstring")  Difference "OGRGeometryH
OGR_G_Difference(OGRGeometryH hThis, OGRGeometryH hOther)

Compute difference.

Generates a new geometry which is the region of this geometry with the
region of the other geometry removed.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method OGRGeometry::Difference().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry.

hOther:  the other geometry.

a new geometry representing the difference or NULL if the difference
is empty or an error occurs. ";

%feature("docstring")  SymDifference "OGRGeometryH
OGR_G_SymDifference(OGRGeometryH hThis, OGRGeometryH hOther)

Compute symmetric difference.

Generates a new geometry which is the symmetric difference of this
geometry and the other geometry.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method
OGRGeometry::SymmetricDifference().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry.

hOther:  the other geometry.

a new geometry representing the symmetric difference or NULL if the
difference is empty or an error occurs.

OGR 1.8.0 ";

%feature("docstring")  SymmetricDifference "OGRGeometryH
OGR_G_SymmetricDifference(OGRGeometryH hThis, OGRGeometryH hOther)

Compute symmetric difference (deprecated)

Deprecated

See:  OGR_G_SymmetricDifference() ";

%feature("docstring")  Disjoint "int OGR_G_Disjoint(OGRGeometryH
hThis, OGRGeometryH hOther)

Test for disjointness.

Tests if this geometry and the other geometry are disjoint.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method OGRGeometry::Disjoint().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry to compare.

hOther:  the other geometry to compare.

TRUE if they are disjoint, otherwise FALSE. ";

%feature("docstring")  Touches "int OGR_G_Touches(OGRGeometryH hThis,
OGRGeometryH hOther)

Test for touching.

Tests if this geometry and the other geometry are touching.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method OGRGeometry::Touches().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry to compare.

hOther:  the other geometry to compare.

TRUE if they are touching, otherwise FALSE. ";

%feature("docstring")  Crosses "int OGR_G_Crosses(OGRGeometryH hThis,
OGRGeometryH hOther)

Test for crossing.

Tests if this geometry and the other geometry are crossing.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method OGRGeometry::Crosses().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry to compare.

hOther:  the other geometry to compare.

TRUE if they are crossing, otherwise FALSE. ";

%feature("docstring")  Within "int OGR_G_Within(OGRGeometryH hThis,
OGRGeometryH hOther)

Test for containment.

Tests if this geometry is within the other geometry.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method OGRGeometry::Within().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry to compare.

hOther:  the other geometry to compare.

TRUE if hThis is within hOther, otherwise FALSE. ";

%feature("docstring")  Contains "int OGR_G_Contains(OGRGeometryH
hThis, OGRGeometryH hOther)

Test for containment.

Tests if this geometry contains the other geometry.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method OGRGeometry::Contains().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry to compare.

hOther:  the other geometry to compare.

TRUE if hThis contains hOther geometry, otherwise FALSE. ";

%feature("docstring")  Overlaps "int OGR_G_Overlaps(OGRGeometryH
hThis, OGRGeometryH hOther)

Test for overlap.

Tests if this geometry and the other geometry overlap, that is their
intersection has a non-zero area.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call IsValid() before, otherwise the
result might be wrong.

This function is the same as the C++ method OGRGeometry::Overlaps().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry to compare.

hOther:  the other geometry to compare.

TRUE if they are overlapping, otherwise FALSE. ";

%feature("docstring")  CloseRings "void OGR_G_CloseRings(OGRGeometryH
hGeom)

Force rings to be closed.

If this geometry, or any contained geometries has polygon rings that
are not closed, they will be closed by adding the starting point at
the end.

Parameters:
-----------

hGeom:  handle to the geometry. ";

%feature("docstring")  Centroid "int OGR_G_Centroid(OGRGeometryH
hGeom, OGRGeometryH hCentroidPoint)

Compute the geometry centroid.

The centroid location is applied to the passed in OGRPoint object. The
centroid is not necessarily within the geometry.

This method relates to the SFCOM ISurface::get_Centroid() method
however the current implementation based on GEOS can operate on other
geometry types such as multipoint, linestring, geometrycollection such
as multipolygons. OGC SF SQL 1.1 defines the operation for surfaces
(polygons). SQL/MM-Part 3 defines the operation for surfaces and
multisurfaces (multipolygons).

This function is the same as the C++ method OGRGeometry::Centroid().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

OGRERR_NONE on success or OGRERR_FAILURE on error. ";

%feature("docstring")  PointOnSurface "OGRGeometryH
OGR_G_PointOnSurface(OGRGeometryH hGeom)

Returns a point guaranteed to lie on the surface.

This method relates to the SFCOM ISurface::get_PointOnSurface() method
however the current implementation based on GEOS can operate on other
geometry types than the types that are supported by SQL/MM-Part 3 :
surfaces (polygons) and multisurfaces (multipolygons).

This method is built on the GEOS library, check it for the definition
of the geometry operation. If OGR is built without the GEOS library,
this method will always fail, issuing a CPLE_NotSupported error.

Parameters:
-----------

hGeom:  the geometry to operate on.

a point guaranteed to lie on the surface or NULL if an error occurred.

OGR 1.10 ";

%feature("docstring")  Simplify "OGRGeometryH
OGR_G_Simplify(OGRGeometryH hThis, double dTolerance)

Compute a simplified geometry.

This function is the same as the C++ method OGRGeometry::Simplify().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry.

dTolerance:  the distance tolerance for the simplification.

the simplified geometry or NULL if an error occurs.

OGR 1.8.0 ";

%feature("docstring")  SimplifyPreserveTopology "OGRGeometryH
OGR_G_SimplifyPreserveTopology(OGRGeometryH hThis, double dTolerance)

Simplify the geometry while preserving topology.

This function is the same as the C++ method
OGRGeometry::SimplifyPreserveTopology().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hThis:  the geometry.

dTolerance:  the distance tolerance for the simplification.

the simplified geometry or NULL if an error occurs.

OGR 1.9.0 ";

%feature("docstring")  DelaunayTriangulation "OGRGeometryH
OGR_G_DelaunayTriangulation(OGRGeometryH hThis, double dfTolerance,
int bOnlyEdges)

Return a Delaunay triangulation of the vertices of the geometry.

This function is the same as the C++ method
OGRGeometry::DelaunayTriangulation().

This function is built on the GEOS library, v3.4 or above. If OGR is
built without the GEOS library, this function will always fail,
issuing a CPLE_NotSupported error.

Parameters:
-----------

hThis:  the geometry.

dfTolerance:  optional snapping tolerance to use for improved
robustness

bOnlyEdges:  if TRUE, will return a MULTILINESTRING, otherwise it will
return a GEOMETRYCOLLECTION containing triangular POLYGONs.

the geometry resulting from the Delaunay triangulation or NULL if an
error occurs.

OGR 2.1 ";

%feature("docstring")  Polygonize "OGRGeometryH
OGR_G_Polygonize(OGRGeometryH hTarget)

Polygonizes a set of sparse edges.

A new geometry object is created and returned containing a collection
of reassembled Polygons: NULL will be returned if the input collection
doesn't corresponds to a MultiLinestring, or when reassembling Edges
into Polygons is impossible due to topological inconsistencies.

This function is the same as the C++ method OGRGeometry::Polygonize().

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters:
-----------

hTarget:  The Geometry to be polygonized.

a handle to a newly allocated geometry now owned by the caller, or
NULL on failure.

OGR 1.9.0 ";

%feature("docstring")  SwapXY "void OGR_G_SwapXY(OGRGeometryH hGeom)

Swap x and y coordinates.

Parameters:
-----------

hGeom:  geometry.

OGR 2.3.0 ";

%feature("docstring")  OGRHasPreparedGeometrySupport "int
OGRHasPreparedGeometrySupport()

Returns if GEOS has prepared geometry support.

TRUE or FALSE ";

%feature("docstring")  OGRCreatePreparedGeometry "OGRPreparedGeometry* OGRCreatePreparedGeometry(const OGRGeometry
*poGeom)

Creates a prepared geometry.

To free with OGRDestroyPreparedGeometry()

Parameters:
-----------

poGeom:  input geometry to prepare.

handle to a prepared geometry. ";

%feature("docstring")  OGRDestroyPreparedGeometry "void
OGRDestroyPreparedGeometry(OGRPreparedGeometry *poPreparedGeom)

Destroys a prepared geometry.

Parameters:
-----------

poPreparedGeom:  preprated geometry. ";

%feature("docstring")  OGRPreparedGeometryIntersects "int
OGRPreparedGeometryIntersects(const OGRPreparedGeometry
*poPreparedGeom, const OGRGeometry *poOtherGeom)

Returns whether a prepared geometry intersects with a geometry.

Parameters:
-----------

poPreparedGeom:  prepared geometry.

poOtherGeom:  other geometry.

TRUE or FALSE. ";

%feature("docstring")  OGRPreparedGeometryContains "int
OGRPreparedGeometryContains(const OGRPreparedGeometry *poPreparedGeom,
const OGRGeometry *poOtherGeom)

Returns whether a prepared geometry contains a geometry.

Parameters:
-----------

poPreparedGeom:  prepared geometry.

poOtherGeom:  other geometry.

TRUE or FALSE. ";

%feature("docstring")  OGRGeometryFromEWKB "OGRGeometry*
OGRGeometryFromEWKB(GByte *pabyWKB, int nLength, int *pnSRID, int
bIsPostGIS1_EWKB) ";

%feature("docstring")  OGRGeometryFromHexEWKB "OGRGeometry*
OGRGeometryFromHexEWKB(const char *pszBytea, int *pnSRID, int
bIsPostGIS1_EWKB) ";

%feature("docstring")  OGRGeometryToHexEWKB "char*
OGRGeometryToHexEWKB(OGRGeometry *poGeometry, int nSRSId, int
nPostGISMajor, int nPostGISMinor) ";

%feature("docstring")  OGR_GT_Flatten "OGRwkbGeometryType
OGR_GT_Flatten(OGRwkbGeometryType eType)

Returns the 2D geometry type corresponding to the passed geometry
type.

This function is intended to work with geometry types as old-style
99-402 extended dimension (Z) WKB types, as well as with newer SFSQL
1.2 and ISO SQL/MM Part 3 extended dimension (Z&M) WKB types.

Parameters:
-----------

eType:  Input geometry type

2D geometry type corresponding to the passed geometry type.

GDAL 2.0 ";

%feature("docstring")  OGR_GT_HasZ "int
OGR_GT_HasZ(OGRwkbGeometryType eType)

Return if the geometry type is a 3D geometry type.

Parameters:
-----------

eType:  Input geometry type

TRUE if the geometry type is a 3D geometry type.

GDAL 2.0 ";

%feature("docstring")  OGR_GT_HasM "int
OGR_GT_HasM(OGRwkbGeometryType eType)

Return if the geometry type is a measured type.

Parameters:
-----------

eType:  Input geometry type

TRUE if the geometry type is a measured type.

GDAL 2.1 ";

%feature("docstring")  OGR_GT_SetZ "OGRwkbGeometryType
OGR_GT_SetZ(OGRwkbGeometryType eType)

Returns the 3D geometry type corresponding to the passed geometry
type.

Parameters:
-----------

eType:  Input geometry type

3D geometry type corresponding to the passed geometry type.

GDAL 2.0 ";

%feature("docstring")  OGR_GT_SetM "OGRwkbGeometryType
OGR_GT_SetM(OGRwkbGeometryType eType)

Returns the measured geometry type corresponding to the passed
geometry type.

Parameters:
-----------

eType:  Input geometry type

measured geometry type corresponding to the passed geometry type.

GDAL 2.1 ";

%feature("docstring")  OGR_GT_SetModifier "OGRwkbGeometryType
OGR_GT_SetModifier(OGRwkbGeometryType eType, int bHasZ, int bHasM)

Returns a XY, XYZ, XYM or XYZM geometry type depending on parameter.

Parameters:
-----------

eType:  Input geometry type

bHasZ:  TRUE if the output geometry type must be 3D.

bHasM:  TRUE if the output geometry type must be measured.

Output geometry type.

GDAL 2.0 ";

%feature("docstring")  OGR_GT_IsSubClassOf "int
OGR_GT_IsSubClassOf(OGRwkbGeometryType eType, OGRwkbGeometryType
eSuperType)

Returns if a type is a subclass of another one.

Parameters:
-----------

eType:  Type.

eSuperType:  Super type

TRUE if eType is a subclass of eSuperType.

GDAL 2.0 ";

%feature("docstring")  OGR_GT_GetCollection "OGRwkbGeometryType
OGR_GT_GetCollection(OGRwkbGeometryType eType)

Returns the collection type that can contain the passed geometry type.

Handled conversions are : wkbNone->wkbNone, wkbPoint -> wkbMultiPoint,
wkbLineString->wkbMultiLineString,
wkbPolygon/wkbTriangle/wkbPolyhedralSurface/wkbTIN->wkbMultiPolygon,
wkbCircularString->wkbMultiCurve, wkbCompoundCurve->wkbMultiCurve,
wkbCurvePolygon->wkbMultiSurface. In other cases, wkbUnknown is
returned

Passed Z, M, ZM flag is preserved.

Parameters:
-----------

eType:  Input geometry type

the collection type that can contain the passed geometry type or
wkbUnknown

GDAL 2.0 ";

%feature("docstring")  OGR_GT_GetCurve "OGRwkbGeometryType
OGR_GT_GetCurve(OGRwkbGeometryType eType)

Returns the curve geometry type that can contain the passed geometry
type.

Handled conversions are : wkbPolygon -> wkbCurvePolygon,
wkbLineString->wkbCompoundCurve, wkbMultiPolygon->wkbMultiSurface and
wkbMultiLineString->wkbMultiCurve. In other cases, the passed geometry
is returned.

Passed Z, M, ZM flag is preserved.

Parameters:
-----------

eType:  Input geometry type

the curve type that can contain the passed geometry type

GDAL 2.0 ";

%feature("docstring")  OGR_GT_GetLinear "OGRwkbGeometryType
OGR_GT_GetLinear(OGRwkbGeometryType eType)

Returns the non-curve geometry type that can contain the passed
geometry type.

Handled conversions are : wkbCurvePolygon -> wkbPolygon,
wkbCircularString->wkbLineString, wkbCompoundCurve->wkbLineString,
wkbMultiSurface->wkbMultiPolygon and
wkbMultiCurve->wkbMultiLineString. In other cases, the passed geometry
is returned.

Passed Z, M, ZM flag is preserved.

Parameters:
-----------

eType:  Input geometry type

the non-curve type that can contain the passed geometry type

GDAL 2.0 ";

%feature("docstring")  OGR_GT_IsCurve "int
OGR_GT_IsCurve(OGRwkbGeometryType eGeomType)

Return if a geometry type is an instance of Curve.

Such geometry type are wkbLineString, wkbCircularString,
wkbCompoundCurve and their Z/M/ZM variant.

Parameters:
-----------

eGeomType:  the geometry type

TRUE if the geometry type is an instance of Curve

GDAL 2.0 ";

%feature("docstring")  OGR_GT_IsSurface "int
OGR_GT_IsSurface(OGRwkbGeometryType eGeomType)

Return if a geometry type is an instance of Surface.

Such geometry type are wkbCurvePolygon and wkbPolygon and their Z/M/ZM
variant.

Parameters:
-----------

eGeomType:  the geometry type

TRUE if the geometry type is an instance of Surface

GDAL 2.0 ";

%feature("docstring")  OGR_GT_IsNonLinear "int
OGR_GT_IsNonLinear(OGRwkbGeometryType eGeomType)

Return if a geometry type is a non-linear geometry type.

Such geometry type are wkbCurve, wkbCircularString, wkbCompoundCurve,
wkbSurface, wkbCurvePolygon, wkbMultiCurve, wkbMultiSurface and their
Z/M variants.

Parameters:
-----------

eGeomType:  the geometry type

TRUE if the geometry type is a non-linear geometry type.

GDAL 2.0 ";

}