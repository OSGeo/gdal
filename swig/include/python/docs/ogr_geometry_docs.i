%extend OGRGeometryShadow {
// File: ogrgeometry_8cpp.xml

%feature("docstring")  Area "
Compute geometry area.

The returned area is a 2D Cartesian (planar) area in square units of the
spatial reference system in use, so potentially 'square degrees' for a
geometry expressed in a geographic SRS.

For more details: :cpp:func:`OGR_G_Area`

Returns
--------
float:
    the area of the geometry in square units of the spatial reference
    system in use, or 0.0 for unsupported geometry types.

";

%feature("docstring")  GeodesicArea "
Compute geometry area, considered as a surface on the underlying
ellipsoid of the SRS attached to the geometry.

For more details: :cpp:func:`OGR_G_GeodesicArea`

Returns
--------
float:
    the area in square meters, or a negative value for unsupported geometry types.

";

%feature("docstring")  DumpReadable "
Dump geometry in well known text format to indicated output file.

For more details: :cpp:func:`OGR_G_DumpReadable`
";

%feature("docstring")  AssignSpatialReference "
Assign spatial reference to this object.

For more details: :cpp:func:`OGR_G_AssignSpatialReference`

Parameters
-----------
reference: SpatialReference
    The new spatial reference system to apply.
";

%feature("docstring")  Intersects "
Determines whether two geometries intersect.

For more details: :cpp:func:`OGR_G_Intersects`

Parameters
-----------
other: Geometry
    The other geometry to test against.

Returns
--------
int:
    True if the geometries intersect, otherwise False.
";

%feature("docstring")  TransformTo "
Transform geometry to new spatial reference system.

For more details: :cpp:func:`OGR_G_TransformTo`

Parameters
-----------
reference: SpatialReference
   The spatial reference system to apply.

Returns
--------
int:
    :py:const:`osgeo.ogr.OGRERR_NONE` on success, or an error code.
";

%feature("docstring")  Transform "
Apply arbitrary coordinate transformation to geometry.

For more details: :cpp:func:`OGR_G_Transform`

Parameters
-----------
trans: CoordinateTransform
    The transformation to apply.

Returns
--------
Geometry:
    The transformed geometry.
";

%feature("docstring")  Segmentize "
Modify the geometry such it has no segment longer then the given
distance.

For more details: :cpp:func:`OGR_G_Segmentize`

Parameters
-----------
dfMaxLength: float
    the maximum distance between 2 points after segmentization
";

%feature("docstring")  GetDimension "
Get the dimension of this geometry.

For more details: :cpp:func:`OGR_G_GetDimension`

Returns
--------
int:
    0 for points, 1 for lines, and 2 for surfaces.
";

%feature("docstring")  GetCoordinateDimension "
Get the dimension of the coordinates in this geometry.

For more details: :cpp:func:`OGR_G_GetCoordinateDimension`

.. warning:: Deprecated. Use :py:func:`CoordinateDimension`,
    :py:func:`Is3D`, or :py:func:`IsMeasured`.

Returns
--------
int:
    This will return 2 or 3.
";

%feature("docstring")  CoordinateDimension "
Get the dimension of the coordinates in this geometry.

For more details: :cpp:func:`OGR_G_CoordinateDimension`

.. versionadded:: 2.1

Returns
--------
int:
    This will return 2 for XY, 3 for XYZ and XYM, and 4 for XYZM data.
";

%feature("docstring")  Is3D "
See whether this geometry has Z coordinates.

For more details: :cpp:func:`OGR_G_Is3D`

.. versionadded:: 2.1

Returns
--------
int:
    True if the geometry has Z coordinates.
";

%feature("docstring")  IsMeasured "
See whether this geometry is measured.

For more details: :cpp:func:`OGR_G_IsMeasured`

.. versionadded:: 2.1

Returns
--------
int:
    True if the geometry has M coordinates.
";

%feature("docstring")  SetCoordinateDimension "
Set the coordinate dimension.

For more details: :cpp:func:`OGR_G_SetCoordinateDimension`

.. warning:: Deprecated. Use :py:func:`Set3D` or :py:func:`SetMeasured`.

Parameters
-----------
dimension: int
    New coordinate dimension value, either 2 or 3.
";

%feature("docstring")  Set3D "
Add or remove the Z coordinate dimension.

For more details: :cpp:func:`OGR_G_Set3D`

.. versionadded:: 2.1

Parameters
-----------
bIs3D: bool
    Should the geometry have a Z dimension, either True or False.
";

%feature("docstring")  SetMeasured "
Add or remove the M coordinate dimension.

For more details: :cpp:func:`OGR_G_SetMeasured`

.. versionadded:: 2.1

Parameters
-----------
bIsMeasured: bool
    Should the geometry have a M dimension, either True or False.
";

%feature("docstring")  Equals "
Returns True if two geometries are equivalent.

For more details: :cpp:func:`OGR_G_Equals`

Parameters
-----------
other: Geometry
    The other geometry to test against.

Returns
--------
int:
    True if equivalent or False otherwise.
";

%feature("docstring")  WkbSize "
Returns size of related binary representation.

For more details: :cpp:func:`OGR_G_WkbSize`

Returns
--------
int
";

%feature("docstring")  WkbSizeEx "
Returns size of related binary representation.

For more details: :cpp:func:`OGR_G_WkbSizeEx`

.. versionadded:: 3.3

Returns
--------
int
";

%feature("docstring")  GetEnvelope "
Computes and returns the bounding envelope for this geometry in the
passed psEnvelope structure.

For more details: :cpp:func:`OGR_G_GetEnvelope`

.. warning:: Check the return order of the bounds.

Returns
--------
minx: float
maxx: float
miny: float
maxy: float
";

%feature("docstring")  GetEnvelope3D "
Computes and returns the bounding envelope (3D) for this geometry in
the passed psEnvelope structure.

For more details: :cpp:func:`OGR_G_GetEnvelope3D`

.. warning:: Check the return order of the bounds.

Returns
--------
minx: float
maxx: float
miny: float
maxy: float
minz: float
maxz: float
";


%feature("docstring")  ExportToWkb "
Convert a geometry well known binary format.

For more details: :cpp:func:`OGR_G_ExportToWkb`

Parameters
-----------
byte_order: osgeo.ogr.wkbXDR | osgeo.ogr.wkbNDR, default=osgeo.ogr.wkbNDR
    One of wkbXDR or wkbNDR indicating MSB or LSB byte order respectively.

Returns
--------
bytes
";

%feature("docstring")  ExportToIsoWkb "
Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known
binary format.

For more details: :cpp:func:`OGR_G_ExportToIsoWkb`

.. versionadded:: 2.0

Parameters
-----------
byte_order: osgeo.ogr.wkbXDR | osgeo.ogr.wkbNDR, default=osgeo.ogr.wkbNDR
    One of wkbXDR or wkbNDR indicating MSB or LSB byte order respectively.

Returns
--------
bytes
";


%feature("docstring")  ExportToWkt "
Convert a geometry into well known text format.

For more details: :cpp:func:`OGR_G_ExportToWkt`

Returns
--------
str
";

%feature("docstring")  ExportToIsoWkt "
Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known text
format.

For more details: :cpp:func:`OGR_G_ExportToIsoWkt`

.. versionadded:: 2.0

Returns
--------
str
";

%feature("docstring")  GetGeometryType "
Fetch geometry type.

For more details: :cpp:func:`OGR_G_GetGeometryType`

Returns
--------
int:
    The geometry type code. The types can be found with
    'osgeo.ogr.wkb' prefix. For example :py:const:`osgeo.ogr.wkbPolygon`.
";

%feature("docstring")  GetGeometryName "
Fetch WKT name for geometry type.

For more details: :cpp:func:`OGR_G_GetGeometryName`

geometry to get name from.

Returns
--------
str
";

%feature("docstring")  Clone "
Make a copy of this object.

For more details: :cpp:func:`OGR_G_Clone`

Returns
--------
Geometry:
    The copy of the geometry with the same spatial reference system
    as the original.
";

%feature("docstring")  GetSpatialReference "
For more details: :cpp:func:`OGR_G_GetSpatialReference`

Returns spatial reference system for geometry.

Returns
--------
SpatialReference
";

%feature("docstring")  Empty "
Clear geometry information.

For more details: :cpp:func:`OGR_G_Empty`
";

%feature("docstring")  IsEmpty "
Test if the geometry is empty.

For more details: :cpp:func:`OGR_G_IsEmpty`

Returns
--------
int:
    True if the geometry has no points, otherwise False.
";

%feature("docstring")  IsValid "
Test if the geometry is valid.

For more details: :cpp:func:`OGR_G_IsValid`

Returns
--------
int:
    True if the geometry has no points, otherwise False.
";

%feature("docstring")  IsSimple "
Returns True if the geometry is simple.

For more details: :cpp:func:`OGR_G_IsSimple`

Returns
--------
int:
    True if object is simple, otherwise False.
";

%feature("docstring")  IsRing "
Test if the geometry is a ring.

For more details: :cpp:func:`OGR_G_IsRing`

Returns
--------
int:
    True if the geometry has no points, otherwise False.
";


%feature("docstring")  FlattenTo2D "
Convert geometry to strictly 2D.

For more details: :cpp:func:`OGR_G_FlattenTo2D`
";

%feature("docstring")  Distance "
Compute distance between two geometries.

For more details: :cpp:func:`OGR_G_Distance`

Parameters
-----------
other: Geometry
    The other geometry to compare against.

Returns
--------
float:
    The distance between the geometries or -1 if an error occurs.
";

%feature("docstring")  Distance3D "
Returns the 3D distance between two geometries.

For more details: :cpp:func:`OGR_G_Distance3D`

.. versionadded:: 2.2

Parameters
-----------
other: Geometry
    The other geometry to compare against.

Returns
--------
float:
    The distance between the geometries or -1 if an error occurs.
";

%feature("docstring")  MakeValid "
Attempts to make an invalid geometry valid without losing vertices.

For more details: :cpp:func:`OGR_G_MakeValidEx`

.. versionadded:: 3.0
.. versionadded:: 3.4 options

Parameters
-----------
options: list[str], optional
    papszOptions to be passed in. For example: [\"METHOD=STRUCTURE\"].

Returns
--------
Geometry:
    A newly allocated geometry now owned by the caller, or None on
    failure.
";


%feature("docstring")  Normalize "
Attempts to bring geometry into normalized/canonical form.

For more details: :cpp:func:`OGR_G_Normalize`

.. versionadded:: 3.3

Returns
--------
Geometry:
    A newly allocated geometry now owned by the caller, or None on
    failure.
";

%feature("docstring")  ConvexHull "
Compute convex hull.

For more details: :cpp:func:`OGR_G_ConvexHull`

Returns
--------
Geometry:
    a handle to A newly allocated geometry now owned by the caller, or
    None on failure.
";

%feature("docstring")  Boundary "
Compute boundary.

For more details: :cpp:func:`OGR_G_Boundary`

Returns
--------
Geometry:
    A new geometry or None on failure.
";

%feature("docstring")  GetBoundary "
Compute boundary (deprecated)

For more details: :cpp:func:`OGR_G_GetBoundary`

..warning:: Deprecated

See: :cpp:func:`OGR_G_Boundary`
";

%feature("docstring")  Buffer "
Compute buffer of geometry.

For more details: :cpp:func:`OGR_G_Buffer`

Parameters
-----------
distance: float
    The buffer distance to be applied. Should be expressed into
    the same unit as the coordinates of the geometry.
quadsecs: int, default=30
    The number of segments used to approximate a 90 degree
    (quadrant) of curvature.

Returns
--------
Geometry:
    The newly created geometry or None if an error occurs.
";

%feature("docstring")  Intersection "
Compute intersection.

For more details: :cpp:func:`OGR_G_Intersection`

Parameters
-----------
other: Geometry
    The other geometry.

Returns
--------
Geometry:
    A new geometry representing the intersection or None if there is no
    intersection or an error occurs.
";

%feature("docstring")  Union "
Compute union.

For more details: :cpp:func:`OGR_G_Union`

Parameters
-----------
other: Geometry
    The other geometry.

Returns
--------
Geometry:
    A new geometry representing the union or None if an error occurs.
";

%feature("docstring")  UnionCascaded "
Compute union using cascading.

For more deails: :cpp:func:`OGR_G_UnionCascaded`

Returns
--------
Geometry:
    A new geometry representing the union or None if an error occurs.
";

%feature("docstring")  Difference "
Compute difference.

For more details: :cpp:func:`OGR_G_Difference`

Parameters
-----------
other: Geometry
    The other geometry.

Returns
--------
Geometry:
    A new geometry representing the difference or None if the difference
    is empty or an error occurs.
";

%feature("docstring")  SymDifference "
Compute symmetric difference.

For more details: :cpp:func:`OGR_G_SymDifference`

Parameters
-----------
other:
    the other geometry.

Returns
--------
Geometry:
    A new geometry representing the symmetric difference or None if the
    difference is empty or an error occurs.
";

%feature("docstring")  SymmetricDifference "
Compute symmetric difference (deprecated)

For more details: :cpp:func:`OGR_G_SymmetricDifference`

.. warning:: Deprecated

";

%feature("docstring")  Disjoint "
Test for disjointness.

For more details: :cpp:func:`OGR_G_Disjoint`

Parameters
-----------
other: Geometry
    The other geometry to compare.

Returns
--------
int:
    True if they are disjoint, otherwise False.
";

%feature("docstring")  Touches "
Test for touching.

For more details: :cpp:func:`OGR_G_Touches`

Parameters
-----------
other:
    the other geometry to compare.

Returns
--------
int:
    True if they are touching, otherwise False.
";

%feature("docstring")  Crosses "
Test for crossing.

For more details: :cpp:func:`OGR_G_Crosses`

Parameters
-----------
other: Geometry
    the other geometry to compare.

Returns
--------
int:
    True if they are crossing, otherwise False.
";

%feature("docstring")  Within "
Test for containment.

For more details: :cpp:func:`OGR_G_Within`

Parameters
-----------
other: Geometry
    the other geometry to compare.

Returns
--------
int:
    True if this is within other, otherwise False.
";

%feature("docstring")  Contains "
Test for containment.

For more details: :cpp:func:`OGR_G_Contains`

Parameters
-----------
other: Geometry
    the other geometry to compare.

Returns
--------
int:
    True if this contains the other geometry, otherwise False.
";

%feature("docstring")  Overlaps "
Test for overlap.

For more details: :cpp:func:`OGR_G_Overlaps`

Parameters
-----------
other: Geometry
    the other geometry to compare.

Returns
--------
int:
    True if they are overlapping, otherwise False.
";

%feature("docstring")  CloseRings "
Force rings to be closed.

For more details: :cpp:func:`OGR_G_CloseRings`
";

%feature("docstring")  Centroid "
Compute the geometry centroid.

For more details: :cpp:func:`OGR_G_Centroid`

Returns
--------
Geometry
";

%feature("docstring")  PointOnSurface "
Returns a point guaranteed to lie on the surface.

For more details: :cpp:func:`OGR_G_PointOnSurface`

Returns
--------
Geometry:
    A point guaranteed to lie on the surface or None if an error occurred.
";

%feature("docstring")  Simplify "
Compute a simplified geometry.

For more details: :cpp:func:`OGR_G_Simplify`

Parameters
-----------
tolerance: float
    The distance tolerance for the simplification.

Returns
--------
Geometry:
    The simplified geometry or None if an error occurs.
";

%feature("docstring")  SimplifyPreserveTopology "
Simplify the geometry while preserving topology.

For more details: :cpp:func:`OGR_G_SimplifyPreserveTopology`

Parameters
-----------
tolerance: float
    The distance tolerance for the simplification.

Returns
--------
Geometry:
    The simplified geometry or None if an error occurs.
";

%feature("docstring")  DelaunayTriangulation "
Return a Delaunay triangulation of the vertices of the geometry.

For more details: :cpp:func:`OGR_G_DelaunayTriangulation`

.. versionadded:: 2.1

Parameters
-----------
dfTolerance: float
    optional snapping tolerance to use for improved robustness
bOnlyEdges: bool
    If True, will return a MULTILINESTRING, otherwise it will
    return a GEOMETRYCOLLECTION containing triangular POLYGONs.

Returns
--------
Geometry:
    The geometry resulting from the Delaunay triangulation or None if an
    error occurs.
";

%feature("docstring")  Polygonize "
Polygonizes a set of sparse edges.

For more details: :cpp:func:`OGR_G_Polygonize`

Returns
--------
Geometry:
    A new geometry or None on failure.
";

%feature("docstring")  SwapXY "
Swap x and y coordinates.

For more details: :cpp:func:`OGR_G_SwapXY`

.. versionadded:: 2.3.0

";

}
