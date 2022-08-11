%extend OGRGeometryShadow {
// File: ogrgeometry_8cpp.xml
%feature("docstring")  DumpReadable "void
:cpp:func:`OGR_G_DumpReadable`

Dump geometry in well known text format to indicated output file.

This method is the same as the CPP method :cpp:func:`OGRGeometry::dumpReadable`.

Parameters
-----------
hGeom:
    handle on the geometry to dump.
fp:
    the text file to write the geometry to.
pszPrefix:
    the prefix to put on each line of output.
";

%feature("docstring")  AssignSpatialReference "
:cpp:func:`OGR_G_AssignSpatialReference`

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
:cpp:func:`OGRGeometry::assignSpatialReference`.

Parameters
-----------
hGeom:
    handle on the geometry to apply the new spatial reference system.
hSRS:
    handle on the new spatial reference system to apply.
";

%feature("docstring")  Intersects "
:cpp:func:`OGR_G_Intersects`

Do these features intersect?

Determines whether two geometries intersect. If GEOS is enabled, then
this is done in rigorous fashion otherwise TRUE is returned if the
envelopes (bounding boxes) of the two geometries overlap.

This function is the same as the CPP method :cpp:func:`OGRGeometry::Intersects`.

Parameters
-----------
hGeom:
    handle on the first geometry.
hOtherGeom:
    handle on the other geometry to test against.

Returns
--------
int:
    TRUE if the geometries intersect, otherwise FALSE.
";

%feature("docstring")  TransformTo "
:cpp:func:`OGR_G_TransformTo`

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

This function is the same as the CPP method :cpp:func:`OGRGeometry::transformTo`.

Parameters
-----------
hGeom:
    handle on the geometry to apply the transform to.
hSRS:
    handle on the spatial reference system to apply.

Returns
--------
OGRErr:
    OGRERR_NONE on success, or an error code.
";

%feature("docstring")  Transform "
:cpp:func:`OGR_G_Transform`

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

This function is the same as the CPP method :cpp:func:`OGRGeometry::transform`.

Parameters
-----------
hGeom:
    handle on the geometry to apply the transform to.
hTransform:
    handle on the transformation to apply.

Returns
--------
OGRErr:
    OGRERR_NONE on success or an error code.
";

%feature("docstring")  Segmentize "
:cpp:func:`OGR_G_Segmentize`

Modify the geometry such it has no segment longer then the given
distance.

Interpolated points will have Z and M values (if needed) set to 0.
Distance computation is performed in 2d only.

This function is the same as the CPP method :cpp:func:`OGRGeometry::segmentize`.

Parameters
-----------
hGeom:
    handle on the geometry to segmentize
dfMaxLength:
    the maximum distance between 2 points after segmentization
";

%feature("docstring")  GetDimension "
:cpp:func:`OGR_G_GetDimension`

Get the dimension of this geometry.

This function corresponds to the SFCOM IGeometry::GetDimension()
method. It indicates the dimension of the geometry, but does not
indicate the dimension of the underlying space (as indicated by
:cpp:func:`OGR_G_GetCoordinateDimension` function).

This function is the same as the CPP method
:cpp:func:`OGRGeometry::getDimension`.

Parameters
-----------
hGeom:
    handle on the geometry to get the dimension from.

Returns
--------
int:
    0 for points, 1 for lines and 2 for surfaces.
";

%feature("docstring")  GetCoordinateDimension "
:cpp:func:`OGR_G_GetCoordinateDimension`

Get the dimension of the coordinates in this geometry.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::getCoordinateDimension`.

Deprecated use :py:func:`CoordinateDimension`, :py:func:`Is3D`, or
:py:func:`IsMeasured`.

Parameters
-----------
hGeom:
    handle on the geometry to get the dimension of the coordinates from.

Returns
--------
int:
    this will return 2 or 3.
";

%feature("docstring")  CoordinateDimension "
:cpp:func:`OGR_G_CoordinateDimension`

Get the dimension of the coordinates in this geometry.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::CoordinateDimension`.

.. versionadded:: 2.1

Parameters
-----------
hGeom:
    handle on the geometry to get the dimension of the coordinates from.

Returns
--------
int:
    this will return 2 for XY, 3 for XYZ and XYM, and 4 for XYZM data.
";

%feature("docstring")  Is3D "
:cpp:func:`OGR_G_Is3D`

See whether this geometry has Z coordinates.

This function is the same as the CPP method :cpp:func:`OGRGeometry::Is3D`.

.. versionadded:: 2.1

Parameters
-----------
hGeom:
    handle on the geometry to check whether it has Z coordinates.

Returns
--------
int:
    TRUE if the geometry has Z coordinates.
";

%feature("docstring")  IsMeasured "
:cpp:func:`OGR_G_IsMeasured`

See whether this geometry is measured.

This function is the same as the CPP method :cpp:func:`OGRGeometry::IsMeasured`.

.. versionadded:: 2.1

Parameters
-----------
hGeom:
    handle on the geometry to check whether it is measured.

Returns
--------
int:
    TRUE if the geometry has M coordinates.
";

%feature("docstring")  SetCoordinateDimension "
:cpp:func:`OGR_G_SetCoordinateDimension`

Set the coordinate dimension.

This method sets the explicit coordinate dimension. Setting the
coordinate dimension of a geometry to 2 should zero out any existing Z
values. Setting the dimension of a geometry collection, a compound
curve, a polygon, etc. will affect the children geometries. This will
also remove the M dimension if present before this call.

Deprecated use :py:func:`Set3D` or :py:func:`SetMeasured`.

Parameters
-----------
hGeom:
    handle on the geometry to set the dimension of the coordinates.
nNewDimension:
    New coordinate dimension value, either 2 or 3.
";

%feature("docstring")  Set3D "
:cpp:func:`OGR_G_Set3D`

Add or remove the Z coordinate dimension.

This method adds or removes the explicit Z coordinate dimension.
Removing the Z coordinate dimension of a geometry will remove any
existing Z values. Adding the Z dimension to a geometry collection, a
compound curve, a polygon, etc. will affect the children geometries.

.. versionadded:: 2.1

Parameters
-----------
hGeom:
    handle on the geometry to set or unset the Z dimension.
bIs3D:
    Should the geometry have a Z dimension, either TRUE or FALSE.
";

%feature("docstring")  SetMeasured "
:cpp:func:`OGR_G_SetMeasured`

Add or remove the M coordinate dimension.

This method adds or removes the explicit M coordinate dimension.
Removing the M coordinate dimension of a geometry will remove any
existing M values. Adding the M dimension to a geometry collection, a
compound curve, a polygon, etc. will affect the children geometries.

.. versionadded:: 2.1

Parameters
-----------
hGeom:
    handle on the geometry to set or unset the M dimension.
bIsMeasured:
    Should the geometry have a M dimension, either TRUE or FALSE.
";

%feature("docstring")  Equals "
:cpp:func:`OGR_G_Equals`

Returns TRUE if two geometries are equivalent.

This operation implements the SQL/MM ST_OrderingEquals() operation.

The comparison is done in a structural way, that is to say that the
geometry types must be identical, as well as the number and ordering
of sub-geometries and vertices. Or equivalently, two geometries are
considered equal by this method if their WKT/WKB representation is
equal. Note: this must be distinguished for equality in a spatial way
(which is the purpose of the ST_Equals() operation).

This function is the same as the CPP method :cpp:func:`OGRGeometry::Equals`.

Parameters
-----------
hGeom:
    handle on the first geometry.
hOther:
    handle on the other geometry to test against.

Returns
--------
int:
    TRUE if equivalent or FALSE otherwise.
";

%feature("docstring")  WkbSize "
:cpp:func:`OGR_G_WkbSize`

Returns size of related binary representation.

This function returns the exact number of bytes required to hold the
well known binary representation of this geometry object. Its
computation may be slightly expensive for complex geometries.

This function relates to the SFCOM IWks::WkbSize() method.

This function is the same as the CPP method :cpp:func:`OGRGeometry::WkbSize`.

Use :py:func:`WkbSizeEx` if called on huge geometries (> 2 GB serialized)

Parameters
-----------
hGeom:
    handle on the geometry to get the binary size from.

Returns
--------
int:
    size of binary representation in bytes.
";

%feature("docstring")  WkbSizeEx "
:cpp:func:`OGR_G_WkbSizeEx`

Returns size of related binary representation.

This function returns the exact number of bytes required to hold the
well known binary representation of this geometry object. Its
computation may be slightly expensive for complex geometries.

This function relates to the SFCOM IWks::WkbSize() method.

This function is the same as the CPP method :cpp:func:`OGRGeometry::WkbSize`.

.. versionadded:: 3.3

Parameters
-----------
hGeom:
    handle on the geometry to get the binary size from.

Returns
--------
int:
    size of binary representation in bytes.
";

%feature("docstring")  GetEnvelope "
:cpp:func:`OGR_G_GetEnvelope`

Computes and returns the bounding envelope for this geometry in the
passed psEnvelope structure.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::getEnvelope`.

Parameters
-----------
hGeom:
    handle of the geometry to get envelope from.
psEnvelope:
    the structure in which to place the results.
";

%feature("docstring")  GetEnvelope3D "
:cpp:func:`OGR_G_GetEnvelope3D`

Computes and returns the bounding envelope (3D) for this geometry in
the passed psEnvelope structure.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::getEnvelope`.

.. versionadded:: 1.9.0

Parameters
-----------
hGeom:
    handle of the geometry to get envelope from.
psEnvelope:
    the structure in which to place the results.
";

%feature("docstring")  ImportFromWkb "
:cpp:func:`OGR_G_ImportFromWkb`

Assign geometry from well known binary data.

The object must have already been instantiated as the correct derived
type of geometry object to match the binaries type.

This function relates to the SFCOM IWks::ImportFromWKB() method.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::importFromWkb`.

Parameters
-----------
hGeom:
    handle on the geometry to assign the well know binary data to.
pabyData:
    the binary input data.
nSize:
    the size of pabyData in bytes, or -1 if not known.

Returns
--------
OGRErr:
    OGRERR_NONE if all goes well, otherwise any of OGRERR_NOT_ENOUGH_DATA,
    OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or OGRERR_CORRUPT_DATA may be
    returned.
";

%feature("docstring")  ExportToWkb "
:cpp:func:`OGR_G_ExportToWkb`

Convert a geometry well known binary format.

This function relates to the SFCOM IWks::ExportToWKB() method.

For backward compatibility purposes, it exports the Old-style 99-402
extended dimension (Z) WKB types for types Point, LineString, Polygon,
MultiPoint, MultiLineString, MultiPolygon and GeometryCollection. For
other geometry types, it is equivalent to :py:func:`ExportToIsoWkb`.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::exportToWkb` with eWkbVariant = wkbVariantOldOgc.

Parameters
-----------
hGeom:
    handle on the geometry to convert to a well know binary data from.
eOrder:
    One of wkbXDR or wkbNDR indicating MSB or LSB byte order respectively.
pabyDstBuffer:
    a buffer into which the binary representation is
    written. This buffer must be at least :cpp:func:`OGR_G_WkbSize` byte in size.

Returns
--------
OGRErr:
    Currently OGRERR_NONE is always returned.
";

%feature("docstring")  ExportToIsoWkb "
:cpp:func:`OGR_G_ExportToIsoWkb`

Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known
binary format.

This function relates to the SFCOM IWks::ExportToWKB() method. It
exports the SFSQL 1.2 and ISO SQL/MM Part 3 extended dimension (Z&M)
WKB types.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::exportToWkb` with eWkbVariant = wkbVariantIso.

.. versionadded:: 2.0

Parameters
-----------
hGeom:
    handle on the geometry to convert to a well know binary data from.
eOrder:
    One of wkbXDR or wkbNDR indicating MSB or LSB byte order respectively.
pabyDstBuffer:
    a buffer into which the binary representation is written.
    This buffer must be at least :cpp:func:`OGR_G_WkbSize` byte in size.

Returns
--------
OGRErr:
    Currently OGRERR_NONE is always returned.
";

%feature("docstring")  ImportFromWkt "OGRErr
:cpp:func:`OGR_G_ImportFromWkt`

Assign geometry from well known text data.

The object must have already been instantiated as the correct derived
type of geometry object to match the text type.

This function relates to the SFCOM IWks::ImportFromWKT() method.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::importFromWkt`.

Parameters
-----------
hGeom:
    handle on the geometry to assign well know text data to.
ppszSrcText:
    pointer to a pointer to the source text. The pointer is
    updated to pointer after the consumed text.

Returns
--------
OGRErr:
    OGRERR_NONE if all goes well, otherwise any of OGRERR_NOT_ENOUGH_DATA,
    OGRERR_UNSUPPORTED_GEOMETRY_TYPE, or OGRERR_CORRUPT_DATA may be
    returned.
";

%feature("docstring")  ExportToWkt "
:cpp:func:`OGR_G_ExportToWkt`

Convert a geometry into well known text format.

This function relates to the SFCOM IWks::ExportToWKT() method.

For backward compatibility purposes, it exports the Old-style 99-402
extended dimension (Z) WKB types for types Point, LineString, Polygon,
MultiPoint, MultiLineString, MultiPolygon and GeometryCollection. For
other geometry types, it is equivalent to :py:func:`ExportToIsoWkt`.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::exportToWkt`.

Parameters
-----------
hGeom:
    handle on the geometry to convert to a text format from.
ppszSrcText:
    a text buffer is allocated by the program, and assigned
    to the passed pointer. After use, \\*ppszDstText should be freed with
    :cpp:func:`CPLFree`.

Returns
--------
OGRErr:
    Currently OGRERR_NONE is always returned.
";

%feature("docstring")  ExportToIsoWkt "
:cpp:func:`OGR_G_ExportToIsoWkt`

Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known text
format.

This function relates to the SFCOM IWks::ExportToWKT() method. It
exports the SFSQL 1.2 and ISO SQL/MM Part 3 extended dimension (Z&M)
WKB types.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::exportToWkt`.

.. versionadded:: 2.0

Parameters
-----------
hGeom:
    handle on the geometry to convert to a text format from.
ppszSrcText:
    a text buffer is allocated by the program, and assigned
    to the passed pointer. After use, \\*ppszDstText should be freed with
    :cpp:func:`CPLFree`.

Returns
--------
OGRErr:
    Currently OGRERR_NONE is always returned.
";

%feature("docstring")  GetGeometryType "
:cpp:func:`OGR_G_GetGeometryType`

Fetch geometry type.

Note that the geometry type may include the 2.5D flag. To get a 2D
flattened version of the geometry type apply the wkbFlatten() macro to
the return result.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::getGeometryType`.

Parameters
-----------
hGeom:
    handle on the geometry to get type from.

Returns
--------
OGRwkbGeometryType:
    the geometry type code.
";

%feature("docstring")  GetGeometryName "
:cpp:func:`OGR_G_GetGeometryName`

Fetch WKT name for geometry type.

There is no SFCOM analog to this function.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::getGeometryName`.

Parameters
-----------
hGeom:
    handle on the geometry to get name from.

Returns
--------
str:
    name used for this geometry type in well known text format.
";

%feature("docstring")  Clone "
:cpp:func:`OGR_G_Clone`

Make a copy of this object.

This function relates to the SFCOM IGeometry::clone() method.

This function is the same as the CPP method :cpp:func:`OGRGeometry::clone`.

Parameters
-----------
hGeom:
    handle on the geometry to clone from.

Returns
--------
OGRGeometryH:
    a handle on the copy of the geometry with the spatial reference system
    as the original.
";

%feature("docstring")  GetSpatialReference "
:cpp:func:`OGR_G_GetSpatialReference`

Returns spatial reference system for geometry.

This function relates to the SFCOM IGeometry::get_SpatialReference()
method.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::getSpatialReference`.

Parameters
-----------
hGeom:
    handle on the geometry to get spatial reference from.

Returns
--------
OGRSpatialReferenceH:
    a reference to the spatial reference geometry.
";

%feature("docstring")  Empty "
:cpp:func:`OGR_G_Empty`

Clear geometry information.

This restores the geometry to its initial state after construction,
and before assignment of actual geometry.

This function relates to the SFCOM IGeometry::Empty() method.

This function is the same as the CPP method :cpp:func:`OGRGeometry::empty`.

Parameters
-----------
hGeom:
    handle on the geometry to empty.
";

%feature("docstring")  IsEmpty "
:cpp:func:`OGR_G_IsEmpty`

Test if the geometry is empty.

This method is the same as the CPP method :cpp:func:`OGRGeometry::IsEmpty`.

Parameters
-----------
hGeom:
    The Geometry to test.

Returns
--------
int:
    TRUE if the geometry has no points, otherwise FALSE.
";

%feature("docstring")  IsValid "
:cpp:func:`OGR_G_IsValid`

Test if the geometry is valid.

This function is the same as the C++ method :cpp:func:`OGRGeometry::IsValid`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always return FALSE.

Parameters
-----------
hGeom:
    The Geometry to test.

Returns
--------
int:
    TRUE if the geometry has no points, otherwise FALSE.
";

%feature("docstring")  IsSimple "
:cpp:func:`OGR_G_IsSimple`

Returns TRUE if the geometry is simple.

Returns TRUE if the geometry has no anomalous geometric points, such
as self intersection or self tangency. The description of each
instantiable geometric class will include the specific conditions that
cause an instance of that class to be classified as not simple.

This function is the same as the C++ method :cpp:func:`OGRGeometry::IsSimple`.

If OGR is built without the GEOS library, this function will always
return FALSE.

Parameters
-----------
hGeom:
    The Geometry to test.

Returns
--------
int:
    TRUE if object is simple, otherwise FALSE.
";

%feature("docstring")  IsRing "
:cpp:func:`OGR_G_IsRing`

Test if the geometry is a ring.

This function is the same as the C++ method :cpp:func:`OGRGeometry::IsRing`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always return FALSE.

Parameters
-----------
hGeom:
    The Geometry to test.

Returns
--------
int:
    TRUE if the geometry has no points, otherwise FALSE.
";

%feature("docstring")  OGRFromOGCGeomType "
:cpp:func:`OGRFromOGCGeomType`

Map OGCgeometry format type to corresponding OGR constants.

Parameters
-----------
pszGeomType:
    POINT[ ][Z][M], LINESTRING[ ][Z][M], etc...

Returns
--------
OGRwkbGeometryType:
    OGR constant.
";

%feature("docstring")  OGRToOGCGeomType "
:cpp:func:`OGRToOGCGeomType`

Map OGR geometry format constants to corresponding OGC geometry type.

Parameters
-----------
eGeomType:
    OGR geometry type

Returns
--------
str:
    string with OGC geometry type (without dimensionality)
";

%feature("docstring")  OGRGeometryTypeToName "
:cpp:func:`OGRGeometryTypeToName`

Fetch a human readable name corresponding to an OGRwkbGeometryType
value.

The returned value should not be modified, or freed by the
application.

This function is C callable.

Parameters
-----------
eType:
    the geometry type.

Returns
--------
str:
    internal human readable string, or NULL on failure.
";

%feature("docstring")  OGRMergeGeometryTypes "
:cpp:func:`OGRMergeGeometryTypes`

Find common geometry type.

Given two geometry types, find the most specific common type. Normally
used repeatedly with the geometries in a layer to try and establish
the most specific geometry type that can be reported for the layer.

NOTE: wkbUnknown is the \'worst case\' indicating a mixture of
geometry types with nothing in common but the base geometry type.
wkbNone should be used to indicate that no geometries have been
encountered yet, and means the first geometry encountered will
establish the preliminary type.

Parameters
-----------
eMain:
    the first input geometry type.
eExtra:
    the second input geometry type.

Returns
--------
OGRwkbGeometryType:
    the merged geometry type.
";

%feature("docstring")  OGRMergeGeometryTypesEx "
:cpp:func:`OGRMergeGeometryTypesEx`

Find common geometry type.

Given two geometry types, find the most specific common type. Normally
used repeatedly with the geometries in a layer to try and establish
the most specific geometry type that can be reported for the layer.

NOTE: wkbUnknown is the \'worst case\' indicating a mixture of
geometry types with nothing in common but the base geometry type.
wkbNone should be used to indicate that no geometries have been
encountered yet, and means the first geometry encountered will
establish the preliminary type.

If bAllowPromotingToCurves is set to TRUE, mixing Polygon and
CurvePolygon will return CurvePolygon. Mixing LineString,
CircularString, CompoundCurve will return CompoundCurve. Mixing
MultiPolygon and MultiSurface will return MultiSurface. Mixing
MultiCurve and MultiLineString will return MultiCurve.

.. versionadded:: 2.0

Parameters
-----------
eMain:
    the first input geometry type.
eExtra:
    the second input geometry type.
bAllowPromotingToCurves:
    determine if promotion to curve type must be done.

Returns
--------
OGRwkbGeometryType:
    the merged geometry type.
";

%feature("docstring")  FlattenTo2D "
:cpp:func:`OGR_G_FlattenTo2D`

Convert geometry to strictly 2D.

In a sense this converts all Z coordinates to 0.0.

This function is the same as the CPP method
:cpp:func:`OGRGeometry::flattenTo2D`.

Parameters
-----------
hGeom:
    handle on the geometry to convert.
";

%feature("docstring")  OGRSetGenerate_DB2_V72_BYTE_ORDER "
:cpp:func:`OGRSetGenerate_DB2_V72_BYTE_ORDER`

Special entry point to enable the hack for generating DB2 V7.2 style
WKB.

DB2 seems to have placed (and require) an extra 0x30 or'ed with the
byte order in WKB. This entry point is used to turn on or off the
generation of such WKB. ";

%feature("docstring")  OGRGetGenerate_DB2_V72_BYTE_ORDER "
:cpp:func:`OGRGetGenerate_DB2_V72_BYTE_ORDER`";

%feature("docstring")  Distance "
:cpp:func:`OGR_G_Distance`

Compute distance between two geometries.

Returns the shortest distance between the two geometries. The distance
is expressed into the same unit as the coordinates of the geometries.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Distance`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hFirst:
    the first geometry to compare against.
hOther:
    the other geometry to compare against.

Returns
--------
float:
    the distance between the geometries or -1 if an error occurs.
";

%feature("docstring")  Distance3D "
:cpp:func:`OGR_G_Distance3D`

Returns the 3D distance between two geometries.

The distance is expressed into the same unit as the coordinates of the
geometries.

This method is built on the SFCGAL library, check it for the
definition of the geometry operation. If OGR is built without the
SFCGAL library, this method will always return -1.0

This function is the same as the C++ method :cpp:func:`OGRGeometry::Distance3D`.

.. versionadded:: 2.2

Parameters
-----------
hFirst:
    the first geometry to compare against.
hOther:
    the other geometry to compare against.

Returns
--------
float:
    the distance between the geometries or -1 if an error occurs.
";

%feature("docstring")  MakeValid "
:cpp:func:`OGR_G_MakeValid`

Attempts to make an invalid geometry valid without losing vertices.

Already-valid geometries are cloned without further intervention.

This function is the same as the C++ method :cpp:func:`OGRGeometry::MakeValid`.

This function is built on the GEOS >= 3.8 library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
>= 3.8 library, this function will return a clone of the input
geometry if it is valid, or NULL if it is invalid

.. versionadded:: 3.0

Parameters
-----------
hGeom:
    The Geometry to make valid.

Returns
--------
OGRGeometryH:
    a newly allocated geometry now owned by the caller, or NULL on
    failure.
";

%feature("docstring")  MakeValidEx "
:cpp:func:`OGR_G_MakeValidEx`

Attempts to make an invalid geometry valid without losing vertices.

Already-valid geometries are cloned without further intervention.

This function is the same as the C++ method :cpp:func:`OGRGeometry::MakeValid`.

See documentation of that method for possible options.

.. versionadded:: 3.4

Parameters
-----------
hGeom:
    The Geometry to make valid.
papszOptions:
    Options.

Returns
--------
OGRGeometryH:
    a newly allocated geometry now owned by the caller, or NULL on
    failure.
";

%feature("docstring")  Normalize "
:cpp:func:`OGR_G_Normalize`

Attempts to bring geometry into normalized/canonical form.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Normalize`.

This function is built on the GEOS library; check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

.. versionadded:: 3.3

Parameters
-----------
hGeom:
    The Geometry to normalize.

Returns
--------
OGRGeometryH:
    a newly allocated geometry now owned by the caller, or NULL on
    failure.
";

%feature("docstring")  ConvexHull "
:cpp:func:`OGR_G_ConvexHull`

Compute convex hull.

A new geometry object is created and returned containing the convex
hull of the geometry on which the method is invoked.

This function is the same as the C++ method :cpp:func:`OGRGeometry::ConvexHull`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hTarget:
    The Geometry to calculate the convex hull of.

Returns
--------
OGRGeometryH:
    a handle to a newly allocated geometry now owned by the caller, or
    NULL on failure.
";

%feature("docstring")  Boundary "
:cpp:func:`OGR_G_Boundary`

Compute boundary.

A new geometry object is created and returned containing the boundary
of the geometry on which the method is invoked.

This function is the same as the C++ method :cpp:func:`OGR_G_Boundary`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

.. versionadded:: 1.8.0

Parameters
-----------
hTarget:
    The Geometry to calculate the boundary of.

Returns
--------
OGRGeometryH:
    a handle to a newly allocated geometry now owned by the caller, or
    NULL on failure.
";

%feature("docstring")  GetBoundary "
:cpp:func:`OGR_G_GetBoundary`

Compute boundary (deprecated)

Deprecated

See: :cpp:func:`OGR_G_Boundary`
";

%feature("docstring")  Buffer "
:cpp:func:`OGR_G_Buffer`

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

This function is the same as the C++ method :cpp:func:`OGRGeometry::Buffer`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hTarget:
    the geometry.
dfDist:
    the buffer distance to be applied. Should be expressed into
    the same unit as the coordinates of the geometry.
nQuadSegs:
    the number of segments used to approximate a 90 degree
    (quadrant) of curvature.

Returns
--------
OGRGeometryH:
    the newly created geometry, or NULL if an error occurs.
";

%feature("docstring")  Intersection "
:cpp:func:`OGR_G_Intersection`

Compute intersection.

Generates a new geometry which is the region of intersection of the
two geometries operated on. The :py:func:`Intersects` function can be
used to test if two geometries intersect.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method
:cpp:func:`OGRGeometry::Intersection`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry.
hOther:
    the other geometry.

Returns
--------
OGRGeometryH:
    a new geometry representing the intersection or NULL if there is no
    intersection or an error occurs.
";

%feature("docstring")  Union "
:cpp:func:`OGR_G_Union`

Compute union.

Generates a new geometry which is the region of union of the two
geometries operated on.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Union`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry.
hOther:
    the other geometry.

Returns
--------
OGRGeometryH:
    a new geometry representing the union or NULL if an error occurs.
";

%feature("docstring")  UnionCascaded "
:cpp:func:`OGR_G_UnionCascaded`

Compute union using cascading.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method
:cpp:func:`OGRGeometry::UnionCascaded`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry.

Returns
--------
OGRGeometryH:
    a new geometry representing the union or NULL if an error occurs.
";

%feature("docstring")  Difference "
:cpp:func:`OGR_G_Difference`

Compute difference.

Generates a new geometry which is the region of this geometry with the
region of the other geometry removed.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Difference`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry.
hOther:
    the other geometry.

Returns
--------
OGRGeometryH:
    a new geometry representing the difference or NULL if the difference
    is empty or an error occurs.
";

%feature("docstring")  SymDifference "
:cpp:func:`OGR_G_SymDifference`

Compute symmetric difference.

Generates a new geometry which is the symmetric difference of this
geometry and the other geometry.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method
:cpp:func:`OGRGeometry::SymmetricDifference`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

.. versionadded:: 1.8.0

Parameters
-----------
hThis:
    the geometry.
hOther:
    the other geometry.

Returns
--------
OGRGeometryH:
    a new geometry representing the symmetric difference or NULL if the
    difference is empty or an error occurs.
";

%feature("docstring")  SymmetricDifference "
:cpp:func:`OGR_G_SymmetricDifference`

Compute symmetric difference (deprecated)

Deprecated

See: :cpp:func:`OGR_G_SymmetricDifference`
";

%feature("docstring")  Disjoint "
:cpp:func:`OGR_G_Disjoint`

Test for disjointness.

Tests if this geometry and the other geometry are disjoint.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Disjoint`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry to compare.
hOther:
    the other geometry to compare.

Returns
--------
int:
    TRUE if they are disjoint, otherwise FALSE.
";

%feature("docstring")  Touches "
:cpp:func:`OGR_G_Touches`

Test for touching.

Tests if this geometry and the other geometry are touching.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Touches`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry to compare.
hOther:
    the other geometry to compare.

Returns
--------
int:
    TRUE if they are touching, otherwise FALSE.
";

%feature("docstring")  Crosses "
:cpp:func:`OGR_G_Crosses`

Test for crossing.

Tests if this geometry and the other geometry are crossing.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Crosses`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry to compare.
hOther:
    the other geometry to compare.

Returns
--------
int:
    TRUE if they are crossing, otherwise FALSE.
";

%feature("docstring")  Within "
:cpp:func:`OGR_G_Within`

Test for containment.

Tests if this geometry is within the other geometry.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Within`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry to compare.
hOther:
    the other geometry to compare.

Returns
--------
int:
    TRUE if hThis is within hOther, otherwise FALSE.
";

%feature("docstring")  Contains "
:cpp:func:`OGR_G_Contains`

Test for containment.

Tests if this geometry contains the other geometry.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Contains`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry to compare.
hOther:
    the other geometry to compare.

Returns
--------
int:
    TRUE if hThis contains hOther geometry, otherwise FALSE.
";

%feature("docstring")  Overlaps "
:cpp:func:`OGR_G_Overlaps`

Test for overlap.

Tests if this geometry and the other geometry overlap, that is their
intersection has a non-zero area.

Geometry validity is not checked. In case you are unsure of the
validity of the input geometries, call :py:func:`IsValid` before, otherwise the
result might be wrong.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Overlaps`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Parameters
-----------
hThis:
    the geometry to compare.
hOther:
    the other geometry to compare.

Returns
--------
int:
    TRUE if they are overlapping, otherwise FALSE.
";

%feature("docstring")  CloseRings "
:cpp:func:`OGR_G_CloseRings`

Force rings to be closed.

If this geometry, or any contained geometries has polygon rings that
are not closed, they will be closed by adding the starting point at
the end.

Parameters
-----------
hGeom:
    handle to the geometry.
";

%feature("docstring")  Centroid "
:cpp:func:`OGR_G_Centroid`

Compute the geometry centroid.

The centroid location is applied to the passed in OGRPoint object. The
centroid is not necessarily within the geometry.

This method relates to the SFCOM ISurface::get_Centroid() method
however the current implementation based on GEOS can operate on other
geometry types such as multipoint, linestring, geometrycollection such
as multipolygons. OGC SF SQL 1.1 defines the operation for surfaces
(polygons). SQL/MM-Part 3 defines the operation for surfaces and
multisurfaces (multipolygons).

This function is the same as the C++ method :cpp:func:`OGRGeometry::Centroid`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

Returns
--------
int:
    OGRERR_NONE on success or OGRERR_FAILURE on error.
";

%feature("docstring")  PointOnSurface "
:cpp:func:`OGR_G_PointOnSurface`

Returns a point guaranteed to lie on the surface.

This method relates to the SFCOM ISurface::get_PointOnSurface() method
however the current implementation based on GEOS can operate on other
geometry types than the types that are supported by SQL/MM-Part 3 :
surfaces (polygons) and multisurfaces (multipolygons).

This method is built on the GEOS library, check it for the definition
of the geometry operation. If OGR is built without the GEOS library,
this method will always fail, issuing a CPLE_NotSupported error.

.. versionadded:: 1.10

Parameters
-----------
hGeom:
    the geometry to operate on.

Returns
--------
OGRGeometryH:
    a point guaranteed to lie on the surface or NULL if an error occurred.
";

%feature("docstring")  Simplify "
:cpp:func:`OGR_G_Simplify`

Compute a simplified geometry.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Simplify`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

.. versionadded:: 1.8.0

Parameters
-----------
hThis:
    the geometry.
dTolerance:
    the distance tolerance for the simplification.

Returns
--------
OGRGeometryH:
    the simplified geometry or NULL if an error occurs.
";

%feature("docstring")  SimplifyPreserveTopology "
:cpp:func:`OGR_G_SimplifyPreserveTopology`

Simplify the geometry while preserving topology.

This function is the same as the C++ method
:cpp:func:`OGRGeometry::SimplifyPreserveTopology`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

.. versionadded:: 1.9.0

Parameters
-----------
hThis:
    the geometry.
dTolerance:
    the distance tolerance for the simplification.

Returns
--------
OGRGeometryH:
    the simplified geometry or NULL if an error occurs.
";

%feature("docstring")  DelaunayTriangulation "
:cpp:func:`OGR_G_DelaunayTriangulation`

Return a Delaunay triangulation of the vertices of the geometry.

This function is the same as the C++ method
:cpp:func:`OGRGeometry::DelaunayTriangulation`.

This function is built on the GEOS library, v3.4 or above. If OGR is
built without the GEOS library, this function will always fail,
issuing a CPLE_NotSupported error.

.. versionadded:: 2.1

Parameters
-----------
hThis:
    the geometry.
dfTolerance:
    optional snapping tolerance to use for improved robustness
bOnlyEdges:
    if TRUE, will return a MULTILINESTRING, otherwise it will
    return a GEOMETRYCOLLECTION containing triangular POLYGONs.

Returns
--------
OGRGeometryH:
    the geometry resulting from the Delaunay triangulation or NULL if an
    error occurs.
";

%feature("docstring")  Polygonize "
:cpp:func:`OGR_G_Polygonize`

Polygonizes a set of sparse edges.

A new geometry object is created and returned containing a collection
of reassembled Polygons: NULL will be returned if the input collection
doesn't corresponds to a MultiLinestring, or when reassembling Edges
into Polygons is impossible due to topological inconsistencies.

This function is the same as the C++ method :cpp:func:`OGRGeometry::Polygonize`.

This function is built on the GEOS library, check it for the
definition of the geometry operation. If OGR is built without the GEOS
library, this function will always fail, issuing a CPLE_NotSupported
error.

.. versionadded:: 1.9.0

Parameters
-----------
hTarget:
    The Geometry to be polygonized.

Returns
--------
OGRGeometryH:
    a handle to a newly allocated geometry now owned by the caller, or
    NULL on failure.
";

%feature("docstring")  SwapXY "
:cpp:func:`OGR_G_SwapXY`

Swap x and y coordinates.

.. versionadded:: 2.3.0

Parameters
-----------
hGeom:
    geometry.
";

%feature("docstring")  OGRHasPreparedGeometrySupport "
:cpp:func:`OGRHasPreparedGeometrySupport`

Returns if GEOS has prepared geometry support.

Returns
--------
int:
    TRUE or FALSE
";

%feature("docstring")  OGRCreatePreparedGeometry "
:cpp:func:`OGRCreatePreparedGeometry`

Creates a prepared geometry.

To free with :py:func:`DestroyPreparedGeometry`

.. versionadded:: 3.3

Parameters
-----------
hGeom:
    input geometry to prepare.

Returns
--------
OGRPreparedGeometryH:
    handle to a prepared geometry.
";

%feature("docstring")  OGRDestroyPreparedGeometry "
:cpp:func:`OGRDestroyPreparedGeometry`

Destroys a prepared geometry.

.. versionadded:: 3.3

Parameters
-----------
hPreparedGeom:
    preprated geometry.
";

%feature("docstring")  OGRPreparedGeometryIntersects "
:cpp:func:`OGRPreparedGeometryIntersects`

Returns whether a prepared geometry intersects with a geometry.

.. versionadded:: 3.3

Parameters
-----------
hPreparedGeom:
    prepared geometry.
hOtherGeom:
    other geometry.

Returns
--------
int:
    TRUE or FALSE.
";

%feature("docstring")  OGRPreparedGeometryContains "
:cpp:func:`OGRPreparedGeometryContains`

Returns whether a prepared geometry contains a geometry.

Parameters
-----------
hPreparedGeom:
    prepared geometry.
hOtherGeom:
    other geometry.

Returns
--------
int:
    TRUE or FALSE.
";

%feature("docstring")  OGRGeometryFromEWKB "
:cpp:func:`OGRGeometryFromEWKB
";

%feature("docstring")  OGRGeometryFromHexEWKB "
:cpp:func:`GRGeometryFromHexEWKB`
";

%feature("docstring")  OGRGeometryToHexEWKB "
:cpp:func:`OGRGeometryToHexEWKB
";

%feature("docstring")  OGR_GT_Flatten "
:cpp:func:`OGR_GT_Flatten`

Returns the 2D geometry type corresponding to the passed geometry
type.

This function is intended to work with geometry types as old-style
99-402 extended dimension (Z) WKB types, as well as with newer SFSQL
1.2 and ISO SQL/MM Part 3 extended dimension (Z&M) WKB types.

.. versionadded:: 2.0

Parameters
-----------
eType:
    Input geometry type

Returns
--------
OGRwkbGeometryType:
    2D geometry type corresponding to the passed geometry type.
";

%feature("docstring")  OGR_GT_HasZ "
:cpp:func:`OGR_GT_HasZ`

Return if the geometry type is a 3D geometry type.

.. versionadded:: 2.0

Parameters
-----------
eType:
    Input geometry type

Returns
--------
int:
    TRUE if the geometry type is a 3D geometry type.
";

%feature("docstring")  OGR_GT_HasM "
:cpp:func:`OGR_GT_HasM`

Return if the geometry type is a measured type.

.. versionadded:: 2.1

Parameters
-----------
eType:
    Input geometry type

Returns
--------
int:
    TRUE if the geometry type is a measured type.
";

%feature("docstring")  OGR_GT_SetZ "
:cpp:func:`OGR_GT_SetZ`

Returns the 3D geometry type corresponding to the passed geometry
type.

.. versionadded:: 2.0

Parameters
-----------
eType:
    Input geometry type

Returns
--------
OGRwkbGeometryType:
    3D geometry type corresponding to the passed geometry type.
";

%feature("docstring")  OGR_GT_SetM "
:cpp:func:`OGR_GT_SetM`

Returns the measured geometry type corresponding to the passed
geometry type.

.. versionadded:: 2.1

Parameters
-----------
eType:
    Input geometry type

Returns
--------
OGRwkbGeometryType:
    measured geometry type corresponding to the passed geometry type.
";

%feature("docstring")  OGR_GT_SetModifier "
:cpp:func:`OGR_GT_SetModifier`

Returns a XY, XYZ, XYM or XYZM geometry type depending on parameter.

.. versionadded:: 2.0

Parameters
-----------
eType:
    Input geometry type
bHasZ:
    TRUE if the output geometry type must be 3D.
bHasM:
    TRUE if the output geometry type must be measured.

Returns
--------
OGRwkbGeometryType:
    Output geometry type.
";

%feature("docstring")  OGR_GT_IsSubClassOf "
:cpp:func:`OGR_GT_IsSubClassOf`

Returns if a type is a subclass of another one.

.. versionadded:: 2.0

Parameters
-----------
eType:
    Type.
eSuperType:
    Super type

Returns
--------
int:
    TRUE if eType is a subclass of eSuperType.
";

%feature("docstring")  OGR_GT_GetCollection "
:cpp:func:`OGR_GT_GetCollection`

Returns the collection type that can contain the passed geometry type.

Handled conversions are : wkbNone->wkbNone, wkbPoint -> wkbMultiPoint,
wkbLineString->wkbMultiLineString,
wkbPolygon/wkbTriangle/wkbPolyhedralSurface/wkbTIN->wkbMultiPolygon,
wkbCircularString->wkbMultiCurve, wkbCompoundCurve->wkbMultiCurve,
wkbCurvePolygon->wkbMultiSurface. In other cases, wkbUnknown is
returned

Passed Z, M, ZM flag is preserved.

.. versionadded:: 2.0

Parameters
-----------
eType:
    Input geometry type

Returns
--------
OGRwkbGeometryType:
    the collection type that can contain the passed geometry type or
    wkbUnknown
";

%feature("docstring")  OGR_GT_GetCurve "
:cpp:func:`OGR_GT_GetCurve`

Returns the curve geometry type that can contain the passed geometry
type.

Handled conversions are : wkbPolygon -> wkbCurvePolygon,
wkbLineString->wkbCompoundCurve, wkbMultiPolygon->wkbMultiSurface and
wkbMultiLineString->wkbMultiCurve. In other cases, the passed geometry
is returned.

Passed Z, M, ZM flag is preserved.

.. versionadded:: 2.0

Parameters
-----------
eType:
    Input geometry type

Returns
--------
OGRwkbGeometryType:
    the curve type that can contain the passed geometry type
";

%feature("docstring")  OGR_GT_GetLinear "
:cpp:func:`OGR_GT_GetLinear`

Returns the non-curve geometry type that can contain the passed
geometry type.

Handled conversions are : wkbCurvePolygon -> wkbPolygon,
wkbCircularString->wkbLineString, wkbCompoundCurve->wkbLineString,
wkbMultiSurface->wkbMultiPolygon and
wkbMultiCurve->wkbMultiLineString. In other cases, the passed geometry
is returned.

Passed Z, M, ZM flag is preserved.

.. versionadded:: 2.0

Parameters
-----------
eType:
    Input geometry type

Returns
--------
OGRwkbGeometryType:
    the non-curve type that can contain the passed geometry type
";

%feature("docstring")  OGR_GT_IsCurve "
:cpp:func:`OGR_GT_IsCurve`

Return if a geometry type is an instance of Curve.

Such geometry type are wkbLineString, wkbCircularString,
wkbCompoundCurve and their Z/M/ZM variant.

.. versionadded:: 2.0

Parameters
-----------
eGeomType:
    the geometry type

Returns
--------
int:
    TRUE if the geometry type is an instance of Curve
";

%feature("docstring")  OGR_GT_IsSurface "
:cpp:func:`OGR_GT_IsSurface`

Return if a geometry type is an instance of Surface.

Such geometry type are wkbCurvePolygon and wkbPolygon and their Z/M/ZM
variant.

.. versionadded:: 2.0

Parameters
-----------
eGeomType:
    the geometry type

Returns
--------
int:
    TRUE if the geometry type is an instance of Surface
";

%feature("docstring")  OGR_GT_IsNonLinear "
:cpp:func:`OGR_GT_IsNonLinear`

Return if a geometry type is a non-linear geometry type.

Such geometry type are wkbCurve, wkbCircularString, wkbCompoundCurve,
wkbSurface, wkbCurvePolygon, wkbMultiCurve, wkbMultiSurface and their
Z/M variants.

.. versionadded:: 2.0

Parameters
-----------
eGeomType:
    the geometry type

Returns
--------
int:
    TRUE if the geometry type is a non-linear geometry type.
";

}