%extend OGRGeometryShadow {
// File: ogrgeometry_8cpp.xml
%feature("docstring")  CPL_CVSID "CPL_CVSID(\"$Id: ogrgeometry.cpp
15346 2008-09-08 18:28:46Z rouault $\") ";

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

Assign spatial reference to this object. Any existing spatial
reference is replaced, but under no circumstances does this result in
the object being reprojected. It is just changing the interpretation
of the existing geometry. Note that assigning a spatial reference
increments the reference count on the OGRSpatialReference, but does
not copy it.

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
distance. Interpolated points will have Z and M values (if needed) set
to 0. Distance computation is performed in 2d only

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

in practice this always returns 2 indicating that coordinates are
specified within a two dimensional space. ";

%feature("docstring")  SetCoordinateDimension "void
OGR_G_SetCoordinateDimension(OGRGeometryH hGeom, int nNewDimension) ";

%feature("docstring")  Equals "int OGR_G_Equals(OGRGeometryH hGeom,
OGRGeometryH hOther)

Returns two if two geometries are equivalent.

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

Test if the geometry is empty

This method is the same as the CPP method OGRGeometry::IsEmpty().

TRUE if the geometry has no points, otherwise FALSE. ";

%feature("docstring")  IsValid "int OGR_G_IsValid(OGRGeometryH hGeom)
";

%feature("docstring")  IsSimple "int OGR_G_IsSimple(OGRGeometryH
hGeom)

Returns TRUE if the geometry is simple.

Returns TRUE if the geometry has no anomalous geometric points, such
as self intersection or self tangency. The description of each
instantiable geometric class will include the specific conditions that
cause an instance of that class to be classified as not simple.

This method relates to the SFCOM IGeometry::IsSimple() method.

NOTE: This method is hardcoded to return TRUE at this time.

TRUE if object is simple, otherwise FALSE. ";

%feature("docstring")  IsRing "int OGR_G_IsRing(OGRGeometryH hGeom)
";

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
OGRSetGenerate_DB2_V72_BYTE_ORDER(int bGenerate_DB2_V72_BYTE_ORDER) ";

%feature("docstring")  OGRGetGenerate_DB2_V72_BYTE_ORDER "int
OGRGetGenerate_DB2_V72_BYTE_ORDER() ";

%feature("docstring")  Distance "double OGR_G_Distance(OGRGeometryH
hFirst, OGRGeometryH hOther) ";

%feature("docstring")  ConvexHull "OGRGeometryH
OGR_G_ConvexHull(OGRGeometryH hTarget) ";

%feature("docstring")  GetBoundary "OGRGeometryH
OGR_G_GetBoundary(OGRGeometryH hTarget) ";

%feature("docstring")  Buffer "OGRGeometryH OGR_G_Buffer(OGRGeometryH
hTarget, double dfDist, int nQuadSegs) ";

%feature("docstring")  Intersection "OGRGeometryH
OGR_G_Intersection(OGRGeometryH hThis, OGRGeometryH hOther) ";

%feature("docstring")  Union "OGRGeometryH OGR_G_Union(OGRGeometryH
hThis, OGRGeometryH hOther) ";

%feature("docstring")  Difference "OGRGeometryH
OGR_G_Difference(OGRGeometryH hThis, OGRGeometryH hOther) ";

%feature("docstring")  SymmetricDifference "OGRGeometryH
OGR_G_SymmetricDifference(OGRGeometryH hThis, OGRGeometryH hOther) ";

%feature("docstring")  Disjoint "int OGR_G_Disjoint(OGRGeometryH
hThis, OGRGeometryH hOther) ";

%feature("docstring")  Touches "int OGR_G_Touches(OGRGeometryH hThis,
OGRGeometryH hOther) ";

%feature("docstring")  Crosses "int OGR_G_Crosses(OGRGeometryH hThis,
OGRGeometryH hOther) ";

%feature("docstring")  Within "int OGR_G_Within(OGRGeometryH hThis,
OGRGeometryH hOther) ";

%feature("docstring")  Contains "int OGR_G_Contains(OGRGeometryH
hThis, OGRGeometryH hOther) ";

%feature("docstring")  Overlaps "int OGR_G_Overlaps(OGRGeometryH
hThis, OGRGeometryH hOther) ";

%feature("docstring")  CloseRings "void OGR_G_CloseRings(OGRGeometryH
hGeom) ";

}