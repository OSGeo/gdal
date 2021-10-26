.. _rfc-61:

=======================================================================================
RFC 61 : Support for measured geometries
=======================================================================================

Author: Ari Jolma

Contact: ari.jolma at gmail.com

Status: Adopted

Implementation in version: 2.1

Summary
-------

This RFC defines how to implement measured geometries (geometries, where
the points have M coordinate, i.e., they are XYM or XYZM).

Rationale
---------

An M coordinate, which is also known as "measure", is an additional
value that can be stored for each point of a geometry (IBM Technical
Note,
`https://www-304.ibm.com/support/docview.wss?uid=swg21054384 <https://www-304.ibm.com/support/docview.wss?uid=swg21054384>`__).

M coordinate is in the OGC simple feature model and it is used in many
vector data formats.

Changes
-------

Changes are required into the C++ API and the C API needs to be
enhanced. Several drivers need to be changed to take advantage of this
enhancement but also due to the changes in the C++ API.

Common API
~~~~~~~~~~

New OGRwkbGeometryType values are needed. SFSQL 1.2 and ISO SQL/MM Part
3 will be used, i.e., 2D type + 2000 for M and 2D type + 3000 for ZM.
(Also types such as Tin, PolyhedralSurface and Triangle types can be
added for completeness, even if unimplemented currently). wkbCurve and
wkbSurface have been moved from #define to the OGRwkbGeometryType
enumerations, and their Z/M/ZM variants have been added as well (per
#6401)

On a more general note, there could (should?) be a path to using a clean
set of values and have legacy support as an exception.

Abstract types are defined and not part of the enum.

::

   // additions to enum OGRwkbGeometryType
       wkbCurve = 13,          /**< Curve (abstract type). ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbSurface = 14,        /**< Surface (abstract type). ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbPolyhedralSurface = 15,/**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbTIN = 16,              /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbTriangle = 17,         /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */

       wkbCurveZ = 1013,           /**< wkbCurve with Z component. ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbSurfaceZ = 1014,         /**< wkbSurface with Z component. ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbPolyhedralSurfaceZ = 1015,  /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbTINZ = 1016,                /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbTriangleZ = 1017,           /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */

       wkbPointM = 2001,              /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbLineStringM = 2002,         /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbPolygonM = 2003,            /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiPointM = 2004,         /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiLineStringM = 2005,    /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiPolygonM = 2006,       /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbGeometryCollectionM = 2007, /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbCircularStringM = 2008,     /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbCompoundCurveM = 2009,      /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbCurvePolygonM = 2010,       /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiCurveM = 2011,         /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiSurfaceM = 2012,       /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbCurveM = 2013,              /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbSurfaceM = 2014,            /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbPolyhedralSurfaceM = 2015,  /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbTINM = 2016,                /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbTriangleM = 2017,           /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */

       wkbPointZM = 3001,              /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbLineStringZM = 3002,         /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbPolygonZM = 3003,            /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiPointZM = 3004,         /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiLineStringZM = 3005,    /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiPolygonZM = 3006,       /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbGeometryCollectionZM = 3007, /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbCircularStringZM = 3008,     /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbCompoundCurveZM = 3009,      /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbCurvePolygonZM = 3010,       /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiCurveZM = 3011,         /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbMultiSurfaceZM = 3012,       /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbCurveZM = 3013,              /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbSurfaceZM = 3014,            /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbPolyhedralSurfaceZM = 3015,  /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbTINZM = 3016,                /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */
       wkbTriangleZM = 3017,           /**< ISO SQL/MM Part 3. GDAL &gt;= 2.1 */

   // add tests for M
   #define wkbHasM(x)     OGR_GT_HasM(x)
   #define wkbSetM(x)     OGR_GT_SetM(x)

   OGRwkbGeometryType CPL_DLL OGR_GT_SetM( OGRwkbGeometryType eType );
   int                CPL_DLL OGR_GT_HasM( OGRwkbGeometryType eType );
           

C++ API
~~~~~~~

The property int nCoordinateDimension in class OGRGeometry will be
replaced by int flags. It may have the following flags:

::

   #define OGR_G_NOT_EMPTY_POINT 0x1
   #define OGR_G_3D 0x2
   #define OGR_G_MEASURED 0x4
   #define OGR_G_IGNORE_MEASURED 0x8

The "ignore" flag is needed internally for backwards compatibility. The
flag OGR_G_NOT_EMPTY_POINT is used only to denote the emptiness of an
OGRPoint object.

Currently a hack to set nCoordDimension negative is used to denote an
empty point.

The removal of nCoordinateDimension may imply changes to drivers etc.
which get or set it.

The tests are

::

   Is3D = flags & OGR_G_3D
   IsMeasured = flags & OGR_G_MEASURED

The setters and getters are implemented with \|= and &=.

When any of these flags is set or unset, the corresponding data becomes
invalid and may be discarded.

Keep the following methods with original semantics, i.e., coordinate
dimension is 2 or 3, but deprecate. There is some discrepancy in
documentation. Their documentation says that they may return zero for
empty points while in ogrpoint.cpp it says that negative nCoordDimension
values are used for empty points and the getCoordinateDimension method
of point returns absolute value of nCoordDimension - thus not zero. A
fix to the doc is probably enough.

::

   int getCoordinateDimension();
   void setCoordinateDimension(int nDimension);
   void flattenTo2D()

It is proposed to possibly add a new method to replace
getCoordinateDimension. set3D and setMeasured would replace
setCoordinateDimension and flattenTo2D. See below.

class OGRGeometry:

::

   //Possibly add methods (SF Common Architecture):
   int Dimension(); // -1 for empty geometries (to denote undefined), 0 for points, 1 for curves, 2 for surfaces, max of components for collections
   char *GeometryType(); // calls OGRToOGCGeomType (which needs to be enhanced)

   //Add methods (SF Common Architecture) see above for implementation:
   int CoordinateDimension(); // 2 if not 3D and not measured, 3 if 3D or measured, 4 if 3D and measured
   OGRBoolean Is3D() const;
   OGRBoolean IsMeasured() const;

   //Add methods (non-standard; note the use of one method instead of second unset* method):
   virtual void set3D(OGRBoolean bIs3D);
   virtual void setMeasured(OGRBoolean bIsMeasured);

   //Add now or later methods:
   virtual OGRGeometry *LocateAlong(double mValue);
   virtual OGRGeometry *LocateBetween(double mStart, double mEnd);

   //Remove b3D from importPreambleFromWkb: it is not used, the flags are managed within the method.

int CoordinateDimension() should have the new semantics. The method name
in simple features documents actually is without prefix get.

Whether set3D and setMeasured should affect the children geometries in a
collection is an issue. Currently doc for setCoordinateDimension says
"Setting the dimension of a geometry collection will affect the children
geometries.", thus we have already committed to maintaining dimensions
of children in collections. It is proposed that set3D and setMeasured
either add or strip Z or M values to or from the geometry (including
possible children). In general the strategy should be to follow the
existing strategy regarding Z (i.e., to strip or add).

Add property double m to class OGRPoint. Add constructor, getters, and
setters for it.

Add property double \*padfM to class OGRSimpleCurve. Add constructor,
getters, and setters for it. New setters with postfix M are needed for
XYM data since the object may be upgraded to XYZ from XY in setters. Add
also methods RemoveM() and AddM() with similar semantics as Make3D and
Make2D.

Override methods set3D and setMeasured in those classes where
setCoordinateDimension is overridden.

Change the semantics of methods whose name begins with \_ and have a
parameter "int b3D". The parameter will be "int coordinates", i.e., a
flags like int, which tells about Z and M.

C API
~~~~~

ogr_core.h:

::

   OGRwkbGeometryType CPL_DLL OGR_GT_SetM( OGRwkbGeometryType eType );
   int                CPL_DLL OGR_GT_HasM( OGRwkbGeometryType eType );

The current behavior is that calling SetPoint on a geometry with
coordinate dimension 2 upgrades the coordinate dimension 3. To keep 2D
points 2D SetPoint_2D must be used. Thus we need separate functions for
M and ZM geometries. The proposal is to use postfixes M and ZM, i.e.,
SetPointM, SetPointZM. Similarly for AddPoint.

Currently there is no SetPoints_2D function. The doc at pabyZ param at
SetPoints comments that "defaults to NULL for 2D objects" but that does
not seem to be the case. See #6344. If that is fixed as written there,
then only SetPointsZM is needed.

GetPoint and GetPoints do not have a 2D version, so only \*ZM version is
needed.

ogr_api.h:

::

   void   CPL_DLL OGR_G_Is3D( OGRGeometryH );
   void   CPL_DLL OGR_G_IsMeasured( OGRGeometryH );

   void   CPL_DLL OGR_G_Set3D( OGRGeometryH, int );
   void   CPL_DLL OGR_G_SetMeasured( OGRGeometryH, int );

   double CPL_DLL OGR_G_GetM( OGRGeometryH, int );

ogr_p.h (This is public header, so new functions are needed)

::

   const char CPL_DLL * OGRWktReadPointsM( const char * pszInput,
                                          OGRRawPoint **ppaoPoints, 
                                          double **ppadfZ,
                                           double **ppadfM,
                                          int * pnMaxPoints,
                                          int * pnReadPoints );
   void CPL_DLL OGRMakeWktCoordinateM( char *, double, double, double, double, int ); // int = flags OGR_G_3D OGR_G_MEASURED
   // Change the semantics of OGRReadWKBGeometryType: b3D is not used and the returned eGeometryType may may any valid type

pggeometry.h is internal, so we can change the function prototype

::

   void OGRCreateFromMultiPatchPart(OGRMultiPolygon *poMP,
                                    OGRPolygon*& poLastPoly,
                                    int nPartType,
                                    int nPartPoints,
                                    double* padfX,
                                    double* padfY,
                                    double* padfZ,
                                    double* padfM);

Use of padfM requires changes to openfilegdb driver.

GEOS, filters, and other issues
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When a geometry with measures is sent to GEOS or used as a filter the M
coordinate is ignored.

LocateAlong and LocateBetween are the only standard methods, which use M
but there could be others, which for example get the extent of M. Such
are not intended to be added now but they can be added later.

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

The new C API functions need to be exposed through swig. Further changes
depend on whether the language bindings are aware of coordinates. At
least Python and Perl are.

The new geometry types will be included into the i files.

Some new setters and getters are needed for M. Is3D, IsMeasured, Set3D
and SetMeasured methods should be added. Also OGR_GT_HasM.

Drivers
-------

Drivers that are probably affected by the C++ changes are at least
(these use the CoordinateDimension API) pg, mssqlspatial, sqlite, db2,
mysql, gml, pgdump, geojson, libkml, gpkg, wasp, gpx, filegdb, vfk, bna,
dxf.

The now deprecated CoordinateDimension API is proposed to be replaced
with calls to \*3D and \*Measured.

Once the support for M coordinates is in place the driver will advertise
the support.

Within the work of this RFC the support is built into memory, shape and
pg drivers. Support for other drivers are left for further work.

Utilities
---------

There is a minimum requirement and new possibilities.

ogrinfo: report measured geom type, report measures

ogr2ogr: support measured geom types

ogrlineref: seems to deal specifically with measures, needs more thought

gdal_rasterize: measure could be used for the burn-in value

gdal_contour: measure could be used as the "elevation" value

gdal_grid: measure could be used as the "Z" value

Documentation
-------------

All new methods/functions are documented.

Test Suite
----------

At least the initial tests will be done with Perl unit tests
(swi/perl/t/measures-\*.t). Later autotest suite will be extended.
Existing tests should not fail.

Compatibility Issues
--------------------

Many drivers (actually datasets and layers) which support measures need
to have the support added. Support should be advertised using

::

   #define ODsCMeasuredGeometries   "MeasuredGeometries"
   #define OLCMeasuredGeometries    "MeasuredGeometries"

The entry point for a creating a layer is CreateLayer method in
GDALDataset. If the dataset does not support measured geometries it will
strip the measured flag from the geometry type it gets as a parameter.
This is in line with current behavior non linear geometry types and
datasets not supporting them.

ICreateLayer, which all drivers that have create layer capability
implement, have geometry type as an argument. The method should call
CPLError() with CPLE_NotSupported and return NULL if the driver does not
support measures. Similarly for ICreateFeature and ISetFeature.

The user-oriented API functions (CreateLayer, CreateFeature, and
SetFeature) should (silently) strip out the measures before continuing
to the I\* methods in drivers that do not support measures. This (side
effect) may not be what is wanted in some usage scenarios but it would
follow the pattern of what is already done with nonlinear geometries.
This should be documented.

An alternative would be to store M value(s) (or WKT or WKB) as attribute
(scalar or vector, depending on the geometry type).

Needs a decision.

Some incompatibilities will necessarily be introduced. For example when
the current XYM-as-XYZ hack in shape will be replaced by proper XYM.

Related tickets
---------------

`https://trac.osgeo.org/gdal/ticket/6063 <https://trac.osgeo.org/gdal/ticket/6063>`__
`https://trac.osgeo.org/gdal/ticket/6331 <https://trac.osgeo.org/gdal/ticket/6331>`__

Implementation
--------------

The implementation will be done by Ari Jolma.

The proposed implementation will be in
`https://github.com/ajolma/GDAL-XYZM <https://github.com/ajolma/GDAL-XYZM>`__

Voting history
--------------

+1 from Even, Tamas, Jukka and Daniel
