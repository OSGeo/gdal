%extend OGRGeometryShadow {
// File: ogrgeometry_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrgeometry.cpp
23140 2011-09-29 20:13:09Z rouault $\") ";

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

Currently this is not implemented in a rigerous fashion, and generally
just tests whether the envelopes of the two features intersect.
Eventually this will be made rigerous.

This function is the same as the CPP method OGRGeometry::Intersects.

Parameters:
-----------

hGeom:  handle on the first geometry.

hOtherGeom:  handle on the other geometry to test against.

TRUE if the geometries intersect, otherwise FALSE. ";

%feature("docstring")  Intersect "int OGR_G_Intersect(OGRGeometryH
hGeom, OGRGeometryH hOtherGeom) ";

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
Distance computation is performed in 2d only

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

This function corresponds to the SFCOM IGeometry::GetDimension()
method.

This function is the same as the CPP method
OGRGeometry::getCoordinateDimension().

Parameters:
-----------

hGeom:  handle on the geometry to get the dimension of the coordinates
from.

in practice this will return 2 or 3. It can also return 0 in the case
of an empty point. ";

%feature("docstring")  SetCoordinateDimension "void
OGR_G_SetCoordinateDimension(OGRGeometryH hGeom, int nNewDimension)

Set the coordinate dimension.

This method sets the explicit coordinate dimension. Setting the
coordinate dimension of a geometry to 2 should zero out any existing Z
values. Setting the dimension of a geometry collection will not
necessarily affect the children geometries.

Parameters:
-----------

hGeom:  handle on the geometry to set the dimension of the
coordinates.

nNewDimension:  New coordinate dimension value, either 2 or 3. ";

%feature("docstring")  Equals "int OGR_G_Equals(OGRGeometryH hGeom,
OGRGeometryH hOther)

Returns TRUE if two geometries are equivalent.

This function is the same as the CPP method OGRGeometry::Equals()
method.

Parameters:
-----------

hGeom:  handle on the first geometry.

hOther:  handle on the other geometry to test against.

TRUE if equivalent or FALSE otherwise. ";

%feature("docstring")  Equal "int OGR_G_Equal(OGRGeometryH hGeom,
OGRGeometryH hOther) ";

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
OGR_G_ImportFromWkb(OGRGeometryH hGeom, unsigned char *pabyData, int
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

nSize:  the size of pabyData in bytes, or zero if not known.

OGRERR_NONE if all goes well, otherwise any of OGRERR_NOT_ENOUGH_DATA,
OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or OGRERR_CORRUPT_DATA may be
returned. ";

%feature("docstring")  ExportToWkb "OGRErr
OGR_G_ExportToWkb(OGRGeometryH hGeom, OGRwkbByteOrder eOrder, unsigned
char *pabyDstBuffer)

Convert a geometry into well known binary format.

This function relates to the SFCOM IWks::ExportToWKB() method.

This function is the same as the CPP method
OGRGeometry::exportToWkb().

Parameters:
-----------

hGeom:  handle on the geometry to convert to a well know binary data
from.

eOrder:  One of wkbXDR or wkbNDR indicating MSB or LSB byte order
respectively.

pabyDstBuffer:  a buffer into which the binary representation is
written. This buffer must be at least OGR_G_WkbSize() byte in size.

Currently OGRERR_NONE is always returned. ";

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

This function is the same as the CPP method
OGRGeometry::exportToWkt().

Parameters:
-----------

hGeom:  handle on the geometry to convert to a text format from.

ppszSrcText:  a text buffer is allocated by the program, and assigned
to the passed pointer.

Currently OGRERR_NONE is always returned. ";

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

an handle on the copy of the geometry with the spatial reference
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

Clear geometry information. This restores the geometry to it's initial
state after construction, and before assignment of actual geometry.

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

This function is the same as the c++ method OGRGeometry::IsSimple()
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
OGRFromOGCGeomType(const char *pszGeomType) ";

%feature("docstring")  OGRToOGCGeomType "const char*
OGRToOGCGeomType(OGRwkbGeometryType eGeomType) ";

%feature("docstring")  OGRGeometryTypeToName "const char*
OGRGeometryTypeToName(OGRwkbGeometryType eType)

Fetch a human readable name corresponding to an OGRwkBGeometryType
value. The returned value should not be modified, or freed by the
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
encountered yet, and means the first geometry encounted will establish
the preliminary type.

Parameters:
-----------

eMain:  the first input geometry type.

eExtra:  the second input geometry type.

the merged geometry type. ";

%feature("docstring")  FlattenTo2D "void
OGR_G_FlattenTo2D(OGRGeometryH hGeom)

Convert geometry to strictly 2D. In a sense this converts all Z
coordinates to 0.0.

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

Returns the shortest distance between the two geometries.

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

Compute boundary (deprecated).

Deprecated See:   OGR_G_Boundary() ";

%feature("docstring")  Buffer "OGRGeometryH OGR_G_Buffer(OGRGeometryH
hTarget, double dfDist, int nQuadSegs)

Compute buffer of geometry.

Builds a new geometry containing the buffer region around the geometry
on which it is invoked. The buffer is a polygon containing the region
within the buffer distance of the original geometry.

Some buffer sections are properly described as curves, but are
converted to approximate polygons. The nQuadSegs parameter can be used
to control how many segements should be used to define a 90 degree
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

dfDist:  the buffer distance to be applied.

nQuadSegs:  the number of segments used to approximate a 90 degree
(quadrant) of curvature.

the newly created geometry, or NULL if an error occurs. ";

%feature("docstring")  Intersection "OGRGeometryH
OGR_G_Intersection(OGRGeometryH hThis, OGRGeometryH hOther)

Compute intersection.

Generates a new geometry which is the region of intersection of the
two geometries operated on. The OGR_G_Intersects() function can be
used to test if two geometries intersect.

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

Compute symmetric difference (deprecated).

Deprecated See:   OGR_G_SymmetricDifference() ";

%feature("docstring")  Disjoint "int OGR_G_Disjoint(OGRGeometryH
hThis, OGRGeometryH hOther)

Test for disjointness.

Tests if this geometry and the other geometry are disjoint.

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

Compute a simplified geometry.

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

}