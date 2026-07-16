%extend OGRGeometryShadow {
// File: ogrgeometry_8cpp.xml

%feature("docstring")  Area "
Compute geometry area.

The returned area is a 2D Cartesian (planar) area in square units of the
spatial reference system in use, so potentially 'square degrees' for a
geometry expressed in a geographic SRS.

For more details: :cpp:func:`OGR_G_Area`

Returns
-------
float
    the area of the geometry in square units of the spatial reference
    system in use, or 0.0 for unsupported geometry types.

";

%feature("docstring")  GeodesicArea "
Compute geometry area, considered as a surface on the underlying
ellipsoid of the SRS attached to the geometry.

For more details: :cpp:func:`OGR_G_GeodesicArea`

Returns
-------
float
    the area in square meters, or a negative value for unsupported geometry types.

";

%feature("docstring")  Length "
Compute geometry length.

The returned length is a 2D Cartesian (planar) area in units of the
spatial reference system in use, so potentially 'degrees' for a
geometry expressed in a geographic SRS.

For more details: :cpp:func:`OGR_G_Length`

Returns
-------
float
    the length of the geometry in units of the spatial reference
    system in use, or 0.0 for unsupported geometry types.

";

%feature("docstring")  GeodesicLength "
Compute geometry length, considered as a curve on the underlying
ellipsoid of the SRS attached to the geometry.

For more details: :cpp:func:`OGR_G_GeodesicLength`

Returns
-------
float
    the area in meters, or a negative value for unsupported geometry types.

";

%feature("docstring")  DumpReadable "
Dump geometry in well known text format to indicated output file.

For more details: :cpp:func:`OGR_G_DumpReadable`
";

%feature("docstring")  AssignSpatialReference "
Assign spatial reference to this object.

For more details: :cpp:func:`OGR_G_AssignSpatialReference`

Parameters
----------
reference : SpatialReference
    The new spatial reference system to apply.
";

%feature("docstring")  Intersects "
Determines whether two geometries intersect.

For more details: :cpp:func:`OGR_G_Intersects`

Parameters
----------
other : Geometry
    The other geometry to test against.

Returns
-------
int
    True if the geometries intersect, otherwise False.
";

%feature("docstring")  TransformTo "
Transform geometry to new spatial reference system.

For more details: :cpp:func:`OGR_G_TransformTo`

Parameters
----------
reference : SpatialReference
   The spatial reference system to apply.

Returns
-------
int
    :py:const:`osgeo.ogr.OGRERR_NONE` on success, or an error code.
";

%feature("docstring")  Transform "
Apply a coordinate transformation to the geometry.

The behavior of the function depends on the type of the provided argument.

If a :py:class:`osr.CoordinateTransformation` is provided, the geometry will be
transformed in-place, or an error code returned.

If a :py:class:`GeomTransformer` is provided, the geometry will remain unmodified,
and a transformed geometry will be returned.

See :cpp:func:`OGR_G_Transform` and :cpp:func:`OGR_GeomTransformer_Transform`.

Parameters
----------
trans : CoordinateTransformation or GeomTransformer
    The transformation to apply.

Returns
-------
Geometry or int
    The transformed geometry if ``trans`` is a :py:class:`GeomTransformer`, or an error code if ``trans`` is a :py:class:`osr.CoordinateTransformation`.

Examples
--------
>>> # use a CoordinateTransformation to transform a geometry in-place
>>> wgs84 = osr.SpatialReference(epsg=4326)
>>> wgs84.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
>>> albers = osr.SpatialReference(epsg=9473)
>>> xform = osr.CoordinateTransformation(wgs84, albers)
>>> vt = ogr.Geometry(ogr.wkbPoint)
>>> vt.AddPoint_2D(145.195, -37.836)
0
>>> vt.Transform(xform)
0
>>> vt.GetPoint_2D()
(1166651.434325419, -4196676.777645577)

>>> # use a GeomTransformer and return a transformed geometry
>>> polar_stereo = osr.SpatialReference(epsg=3413)
>>> rect = ogr.CreateGeometryFromWkt('POLYGON((-3106890 145265,-3106890 2474514,-672244 2474514,-672244 145265,-3106890 145265))')
>>> xform = osr.CoordinateTransformation(polar_stereo, wgs84)
>>> xformer = ogr.GeomTransformer(xform, {'WRAPDATELINE=TRUE':True, 'DATELINEOFFSET': 180})
>>> rect_transformed = rect.Transform(xformer)
>>> rect_transformed.ExportToWkt()
'MULTIPOLYGON (((150.198564480925 66.6449112280509,180.0 74.7428375082776,180.0 56.6419000989319,150.198564480925 66.6449112280509)),((-173.535923883218 54.4721983980102,-180 56.6419000989319,-180 74.7428375082776,-147.193543096797 83.6573164550226,-137.676958092793 61.8457340532701,-173.535923883218 54.4721983980102)))' # no-check
";

%feature("docstring")  Segmentize "
Modify the geometry such it has no segment longer then the given
distance.

For more details: :cpp:func:`OGR_G_Segmentize`

Parameters
----------
dfMaxLength : float
    the maximum distance between 2 points after segmentization
";

%feature("docstring")  GetDimension "
Get the dimension of this geometry.

For more details: :cpp:func:`OGR_G_GetDimension`

Returns
-------
int
    0 for points, 1 for lines, and 2 for surfaces.
";

%feature("docstring")  GetCoordinateDimension "
Get the dimension of the coordinates in this geometry.

For more details: :cpp:func:`OGR_G_GetCoordinateDimension`

.. warning:: Deprecated. Use :py:func:`CoordinateDimension`,
    :py:func:`Is3D`, or :py:func:`IsMeasured`.

Returns
-------
int
    This will return 2 or 3.
";

%feature("docstring")  CoordinateDimension "
Get the dimension of the coordinates in this geometry.

For more details: :cpp:func:`OGR_G_CoordinateDimension`

.. versionadded:: 2.1

Returns
-------
int
    This will return 2 for XY, 3 for XYZ and XYM, and 4 for XYZM data.
";

%feature("docstring")  Is3D "
See whether this geometry has Z coordinates.

For more details: :cpp:func:`OGR_G_Is3D`

.. versionadded:: 2.1

Returns
-------
int
    True if the geometry has Z coordinates.
";

%feature("docstring")  IsMeasured "
See whether this geometry is measured.

For more details: :cpp:func:`OGR_G_IsMeasured`

.. versionadded:: 2.1

Returns
-------
int
    True if the geometry has M coordinates.
";

%feature("docstring")  SetCoordinateDimension "
Set the coordinate dimension.

For more details: :cpp:func:`OGR_G_SetCoordinateDimension`

.. warning:: Deprecated. Use :py:func:`Set3D` or :py:func:`SetMeasured`.

Parameters
----------
dimension : int
    New coordinate dimension value, either 2 or 3.
";

%feature("docstring")  Set3D "
Add or remove the Z coordinate dimension.

For more details: :cpp:func:`OGR_G_Set3D`

.. versionadded:: 2.1

Parameters
----------
bIs3D : bool
    Should the geometry have a Z dimension, either True or False.
";

%feature("docstring")  SetMeasured "
Add or remove the M coordinate dimension.

For more details: :cpp:func:`OGR_G_SetMeasured`

.. versionadded:: 2.1

Parameters
----------
bIsMeasured : bool
    Should the geometry have a M dimension, either True or False.
";

%feature("docstring")  Equals "
Returns True if two geometries are equivalent.

For more details: :cpp:func:`OGR_G_Equals`

Parameters
----------
other : Geometry
    The other geometry to test against.

Returns
-------
int
    True if equivalent or False otherwise.
";

%feature("docstring")  WkbSize "
Returns size of related binary representation.

For more details: :cpp:func:`OGR_G_WkbSize`

Returns
-------
int
";

%feature("docstring")  WkbSizeEx "
Returns size of related binary representation.

For more details: :cpp:func:`OGR_G_WkbSizeEx`

.. versionadded:: 3.3

Returns
-------
int
";

%feature("docstring")  GetEnvelope "
Computes and returns the bounding envelope for this geometry in the
passed psEnvelope structure.

For more details: :cpp:func:`OGR_G_GetEnvelope`

.. warning:: Check the return order of the bounds.

Returns
-------
tuple of float
    (minx, maxx, miny, maxy)
";

%feature("docstring")  GetEnvelope3D "
Computes and returns the bounding envelope (3D) for this geometry in
the passed psEnvelope structure.

For more details: :cpp:func:`OGR_G_GetEnvelope3D`

.. warning:: Check the return order of the bounds.

Returns
-------
tuple of float
    (minx, maxx, miny, maxy, minz, maxz)
";


%feature("docstring")  ExportToWkb "
Convert a geometry well known binary format.

For more details: :cpp:func:`OGR_G_ExportToWkb`

Parameters
----------
byte_order : int, optional
    One of :py:const:`osgeo.ogr.wkbXDR` or :py:const:`osgeo.ogr.wkbNDR`,
    indicating MSB (most significant byte) or LSB (least significant byte)
    byte order. Defaults to :py:const:`osgeo.ogr.wkbNDR`.

Returns
-------
bytes
";

%feature("docstring")  ExportToIsoWkb "
Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known
binary format.

For more details: :cpp:func:`OGR_G_ExportToIsoWkb`

.. versionadded:: 2.0

Parameters
----------
byte_order : int, optional
    One of :py:const:`osgeo.ogr.wkbXDR` or :py:const:`osgeo.ogr.wkbNDR`,
    indicating MSB (most significant byte) or LSB (least significant byte)
    byte order. Defaults to :py:const:`osgeo.ogr.wkbNDR`.

Returns
-------
bytes
";


%feature("docstring")  ExportToWkt "
Convert a geometry into well known text format.

For more details: :cpp:func:`OGR_G_ExportToWkt`

Returns
-------
str
";

%feature("docstring")  ExportToIsoWkt "
Convert a geometry into SFSQL 1.2 / ISO SQL/MM Part 3 well known text
format.

For more details: :cpp:func:`OGR_G_ExportToIsoWkt`

.. versionadded:: 2.0

Returns
-------
str
";

%feature("docstring")  GetGeometryType "
Fetch geometry type.

For more details: :cpp:func:`OGR_G_GetGeometryType`

Returns
-------
int
    The geometry type code. The types can be found with
    'osgeo.ogr.wkb' prefix. For example :py:const:`osgeo.ogr.wkbPolygon`.
";

%feature("docstring")  GetGeometryName "
Fetch WKT name for geometry type.

For more details: :cpp:func:`OGR_G_GetGeometryName`

geometry to get name from.

Returns
-------
str
";

%feature("docstring")  Clone "
Make a copy of this object.

For more details: :cpp:func:`OGR_G_Clone`

Returns
-------
Geometry
    The copy of the geometry with the same spatial reference system
    as the original.
";

%feature("docstring")  GetSpatialReference "
For more details: :cpp:func:`OGR_G_GetSpatialReference`

Returns spatial reference system for geometry.

Returns
-------
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
-------
int
    True if the geometry has no points, otherwise False.
";

%feature("docstring")  IsValid "
Test if the geometry is valid.

For more details: :cpp:func:`OGR_G_IsValid`

Returns
-------
int
    True if the geometry has no points, otherwise False.
";

%feature("docstring")  IsSimple "
Returns True if the geometry is simple.

For more details: :cpp:func:`OGR_G_IsSimple`

Returns
-------
int
    True if object is simple, otherwise False.
";

%feature("docstring")  IsRing "
Test if the geometry is a ring.

For more details: :cpp:func:`OGR_G_IsRing`

Returns
-------
int
    True if the coordinates of the geometry form a ring, by checking length
    and closure (self-intersection is not checked), otherwise False.
";


%feature("docstring")  FlattenTo2D "
Convert geometry to strictly 2D.

For more details: :cpp:func:`OGR_G_FlattenTo2D`
";

%feature("docstring")  Distance "
Compute distance between two geometries.

For more details: :cpp:func:`OGR_G_Distance`

Parameters
----------
other : Geometry
    The other geometry to compare against.

Returns
-------
float
    The distance between the geometries or -1 if an error occurs.
";

%feature("docstring")  Distance3D "
Returns the 3D distance between two geometries.

For more details: :cpp:func:`OGR_G_Distance3D`

.. versionadded:: 2.2

Parameters
----------
other : Geometry
    The other geometry to compare against.

Returns
-------
float
    The distance between the geometries or -1 if an error occurs.
";

%feature("docstring")  MakeValid "
Attempts to make an invalid geometry valid without losing vertices.

For more details: :cpp:func:`OGR_G_MakeValidEx`

.. versionadded:: 3.0
.. versionadded:: 3.4 options

Parameters
----------
options : list[str], optional
    papszOptions to be passed in. For example: [\"METHOD=STRUCTURE\"].

Returns
-------
Geometry
    A newly allocated geometry now owned by the caller, or None on
    failure.
";


%feature("docstring")  Normalize "
Attempts to bring geometry into normalized/canonical form.

For more details: :cpp:func:`OGR_G_Normalize`

.. versionadded:: 3.3

Returns
-------
Geometry
    A newly allocated geometry now owned by the caller, or None on
    failure.
";

%feature("docstring")  ConvexHull "
Compute convex hull.

For more details: :cpp:func:`OGR_G_ConvexHull`

Returns
-------
Geometry
    a handle to A newly allocated geometry now owned by the caller, or
    None on failure.
";

%feature("docstring")  Boundary "
Compute boundary.

For more details: :cpp:func:`OGR_G_Boundary`

Returns
-------
Geometry
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
----------
distance : float
    The buffer distance to be applied. Should be expressed into
    the same unit as the coordinates of the geometry.
quadsecs : int, default=30
    The number of segments used to approximate a 90 degree
    (quadrant) of curvature.
options : list or dict, optional
    An optional list of options to control the buffer output.
    See :cpp:func:`OGR_G_BufferEx`.

Returns
-------
Geometry
    The newly created geometry or None if an error occurs.
";

%feature("docstring")  Intersection "
Compute intersection.

For more details: :cpp:func:`OGR_G_Intersection`

Parameters
----------
other : Geometry
    The other geometry.

Returns
-------
Geometry
    A new geometry representing the intersection or None if there is no
    intersection or an error occurs.
";

%feature("docstring")  Union "
Compute union.

For more details: :cpp:func:`OGR_G_Union`

Parameters
----------
other : Geometry
    The other geometry.

Returns
-------
Geometry
    A new geometry representing the union or None if an error occurs.
";

%feature("docstring")  UnionCascaded "
Compute union using cascading.

For more details: :cpp:func:`OGR_G_UnionCascaded`

Returns
-------
Geometry
    A new geometry representing the union or None if an error occurs.
";

%feature("docstring")  Difference "
Compute difference.

For more details: :cpp:func:`OGR_G_Difference`

Parameters
----------
other : Geometry
    The other geometry.

Returns
-------
Geometry
    A new geometry representing the difference or None if the difference
    is empty or an error occurs.
";

%feature("docstring")  SymDifference "
Compute symmetric difference.

For more details: :cpp:func:`OGR_G_SymDifference`

Parameters
----------
other : Geometry
    the other geometry.

Returns
-------
Geometry
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
----------
other : Geometry
    The other geometry to compare.

Returns
-------
int
    True if they are disjoint, otherwise False.
";

%feature("docstring")  Touches "
Test for touching.

For more details: :cpp:func:`OGR_G_Touches`

Parameters
----------
other : Geometry
    the other geometry to compare.

Returns
-------
int
    True if they are touching, otherwise False.
";

%feature("docstring")  Crosses "
Test for crossing.

For more details: :cpp:func:`OGR_G_Crosses`

Parameters
----------
other : Geometry
    the other geometry to compare.

Returns
-------
int
    True if they are crossing, otherwise False.
";

%feature("docstring")  Within "
Test for containment.

For more details: :cpp:func:`OGR_G_Within`

Parameters
----------
other : Geometry
    the other geometry to compare.

Returns
-------
int
    True if this is within other, otherwise False.
";

%feature("docstring")  Contains "
Test for containment.

For more details: :cpp:func:`OGR_G_Contains`

Parameters
----------
other : Geometry
    the other geometry to compare.

Returns
-------
int
    True if this contains the other geometry, otherwise False.
";

%feature("docstring")  Overlaps "
Test for overlap.

For more details: :cpp:func:`OGR_G_Overlaps`

Parameters
----------
other : Geometry
    the other geometry to compare.

Returns
-------
int
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
-------
Geometry
";

%feature("docstring")  PointOnSurface "
Returns a point guaranteed to lie on the surface.

For more details: :cpp:func:`OGR_G_PointOnSurface`

Returns
-------
Geometry
    A point guaranteed to lie on the surface or None if an error occurred.
";

%feature("docstring")  Simplify "
Compute a simplified geometry.

For more details: :cpp:func:`OGR_G_Simplify`

Parameters
----------
tolerance : float
    The distance tolerance for the simplification.

Returns
-------
Geometry
    The simplified geometry or None if an error occurs.
";

%feature("docstring")  SimplifyPreserveTopology "
Simplify the geometry while preserving topology.

For more details: :cpp:func:`OGR_G_SimplifyPreserveTopology`

Parameters
----------
tolerance : float
    The distance tolerance for the simplification.

Returns
-------
Geometry
    The simplified geometry or None if an error occurs.
";

%feature("docstring")  DelaunayTriangulation "
Return a Delaunay triangulation of the vertices of the geometry.

For more details: :cpp:func:`OGR_G_DelaunayTriangulation`

.. versionadded:: 2.1

Parameters
----------
dfTolerance : float
    optional snapping tolerance to use for improved robustness
bOnlyEdges : bool
    If True, will return a MULTILINESTRING, otherwise it will
    return a GEOMETRYCOLLECTION containing triangular POLYGONs.

Returns
-------
Geometry
    The geometry resulting from the Delaunay triangulation or None if an
    error occurs.
";

%feature("docstring")  ConstrainedDelaunayTriangulation "
Return a constrained Delaunay triangulation of the vertices of the given
polygon(s). For non-polygonal inputs, silently returns an empty geometry
collection.

For more details: :cpp:func:`OGR_G_ConstrainedDelaunayTriangulation`

.. versionadded:: 3.12

Returns
-------
Geometry
    The geometry collection resulting from the constrained Delaunay
    triangulation or None if an error occurs.
";

%feature("docstring")  Polygonize "
Polygonizes a set of sparse edges.

For more details: :cpp:func:`OGR_G_Polygonize`

Returns
-------
Geometry
    A new geometry or None on failure.
";

%feature("docstring")  BuildArea "
Polygonize a linework assuming inner polygons are holes.

For more details: :cpp:func:`OGR_G_BuildArea`

Returns
-------
Geometry
    A new geometry or None on failure.
";

%feature("docstring")  SwapXY "
Swap x and y coordinates.

For more details: :cpp:func:`OGR_G_SwapXY`

.. versionadded:: 2.3.0

";


%feature("docstring")  AddPoint "
Add a point to a geometry (line string or point).

The vertex count of the line string is increased by one, and assigned from
the passed location value.

The geometry is promoted to include a Z component, if it does not already
have one, even if the Z parameter is not explicitly specified. To avoid that
use AddPoint_2D.

This is the same as :cpp:func:`OGR_G_AddPoint`

Parameters
----------
X : float
    x coordinate of point to add.
Y : float
    y coordinate of point to add.
Z : float
    z coordinate of point to add. Defaults to 0

Examples
--------
>>> pt = ogr.Geometry(ogr.wkbPoint)
>>> ogr.GeometryTypeToName(pt.GetGeometryType())
'Point'
>>> pt.AddPoint(3, 7)
0
>>> ogr.GeometryTypeToName(pt.GetGeometryType())
'3D Point'
";

%feature("docstring")  AddPoint_2D "
Add a point to a geometry (line string or point).

The vertex count of the line string is increased by one, and assigned from
the passed location value.

If the geometry includes a Z or M component, the value for those components
for the added point will be 0.

This is the same as :cpp:func:`OGR_G_AddPoint_2D`

Parameters
----------
X : float
    x coordinate of point to add.
Y : float
    y coordinate of point to add.

Examples
--------
>>> pt = ogr.Geometry(ogr.wkbPoint)
>>> ogr.GeometryTypeToName(pt.GetGeometryType())
'Point'
>>> pt.AddPoint_2D(3, 7)
0
>>> ogr.GeometryTypeToName(pt.GetGeometryType())
'Point'
";

}
