.. _rfc-41:

====================================================
RFC 41 : Support for multiple geometry fields in OGR
====================================================

Summary
-------

Add read/write support in the OGR data model for features with multiple
geometry fields.

Motivation
----------

The OGR data model is currently tied to a single geometry field per
feature, feature definition and layer. But a number of data formats
support multiple geometry fields. The OGC Simple Feature Specifications
also do not limit to one geometry field per layer (e.g. §7.1.4 of `OGC
06-104r4 "OpenGIS® Implementation Standard for Geographic information -
Simple feature access -Part 2: SQL
option <http://portal.opengeospatial.org/files/?artifact_id=25354>`__).

There are workarounds : using geometries of type GEOMETRYCOLLECTION, or
advertizing as many layers as there are geometry columns in the layer
(like currently done in the PostGIS or SQLite drivers). All those
approach are at best workarounds that suffer from limitations :

-  GEOMETRYCOLLECTION approach : no way to know the name/semantics of
   each sub-geometry. All sub-geometries must be expressed in the same
   SRS. No way of guaranteeing that the GEOMETRYCOLLECTION has always
   the same number of sub-geometries or that there are of a consistent
   geometry type.
-  one layer per geometry column approach : only appropriate for
   read-only scenarios. Cannot work in write scenarios.

The purpose of this RFC is to make support for multiple geometry fields
per feature to be properly taken into account in the OGR data model.

Proposed solution
-----------------

(Note: alternative solutions have also been studied. They are explained
in a following section of this RFC.)

To sum it up, geometry fields will be treated similarly as attribute
fields are handled at the OGRFeatureDefn and OGRFeature levels, but they
will be kept separate. Attribute fields and geometry fields will have
their own separate indexing in the feature definition.

This choice has been mainly made to maximize backward compatibility,
while offering new capabilities.

Its involves creating a OGRGeomFieldDefn class, and changes in
OGRFieldDefn, OGRFeatureDefn, OGRFeature and OGRLayer classes.

OGRGeomFieldDefn class
~~~~~~~~~~~~~~~~~~~~~~

The OGRGeomFieldDefn is a new class. Its structure is directly inspired
from the OGRFieldDefn class.

::

   class CPL_DLL OGRGeomFieldDefn
   {
   protected:
           char                *pszName;
           OGRwkbGeometryType   eGeomType; /* all values possible except wkbNone */
           OGRSpatialReference* poSRS;

           int                 bIgnore;

   public:
                               OGRGeomFieldDefn(char *pszName,
                                                OGRwkbGeometryType eGeomType);
           virtual            ~OGRGeomFieldDefn();

           void                SetName( const char * );
           const char         *GetNameRef();

           OGRwkbGeometryType  GetType();
           void                SetType( OGRwkbGeometryType eTypeIn );

           virtual OGRSpatialReference* GetSpatialRef();
           void                 SetSpatialRef(OGRSpatialReference* poSRS);

           int                 IsIgnored();
           void                SetIgnored( int bIgnoreIn );
   };

One can notice that the member variables were to be found at OGRLayer
level previously.

The SRS object is ref-counted. The reference count is increased in the
constructor and in SetSpatialRef(), and decreased in the destructor.

GetSpatialRef() is deliberately set virtual, so that lazy evaluation can
be implemented (getting SRS can have a noticeable cost in some driver
implementations, like reading an extra file, or issuing a SQL request).

OGRFeatureDefn class
~~~~~~~~~~~~~~~~~~~~

The OGRFeatureDefn class will be extended as the following :

::

   class CPL_DLL OGRFeatureDefn
   {
     protected:
           // Remove OGRwkbGeometryType eGeomType and bIgnoreGeometry and
           // add instead the following :

           int nGeomFieldCount;
           OGRGeomFieldDefn* papoGeomFieldDefn;
     public:
           virtual int         GetGeomFieldCount();
           virtual OGRGeomFieldDefn *GetGeomFieldDefn( int i );
           virtual int         GetGeomFieldIndex( const char * );

           virtual void        AddGeomFieldDefn( OGRGeomFieldDefn * );
           virtual OGRErr      DeleteGeomFieldDefn( int iGeomField );

           // Route OGRwkbGeometryType GetGeomType() and void SetGeomType() 
           // on the first geometry field definition.

           // Same for IsGeometryIgnored() and SetGeometryIgnored()
   }

At instantiation, OGRFeatureDefn would create a default geometry field
definition of name "" and type wkbUnknown. If SetGeomType() is called,
this will be routed on papoGeomFieldDefn[0]. If only one geometry field
definition exists, SetGeomType(wkbNone) will remove it.

GetGeomType() will be routed on papoGeomFieldDefn[0] if it exists.
Otherwise it will return wkbNone.

It is strongly advised that there is name uniqueness among the combined
set of regular field names and the geometry field names. Failing to do
so will result in unspecified behavior in SQL queries. This advice will
not be checked by the code (it is currently not done for regular
fields).

Another change is to make all the existing methods of OGRFeatureDefn
virtual (and change private visibility to protected), so this class can
be subclassed if needed. This will enable lazy creation of the object.
Justification: establishing the full feature definition can be costly.
But applications may want to list all the layers of a datasource, and
only present some information that is important, but cheap to establish.
In the past, OGRLayer::GetName() and OGRLayer::GetGeomType() have been
introduced in order to workaround for that.

Note also that ReorderGeomFieldDefns() is not foreseen for the moment.
It could be added in a later step, should the need arises.
DeleteGeomFieldDefn() is mostly there for the own benefit of
OGRFeatureDefn itself when calling SetGeomType(wkbNone).

OGRFeature class
~~~~~~~~~~~~~~~~

The OGRFeature class will be extended as following :

::

   class CPL_DLL OGRFeature
   {
     private:
           // Remove poGeometry field and add instead
           OGRGeometry** papoGeometries; /* size is given by poFDefn->GetGeomFieldCount() */

     public:

           int                 GetGeomFieldCount();
           OGRGeomFieldDefn   *GetGeomFieldDefnRef( int iField );
           int                 GetGeomFieldIndex( const char * pszName);

           OGRGeometry*        GetGeomFieldRef(int iField);
           OGRErr              SetGeomFieldDirectly( int iField, OGRGeometry * );
           OGRErr              SetGeomField( int iField, OGRGeometry * );

           // Route SetGeometryDirectly(), SetGeometry(), GetGeometryRef(), 
           // StealGeometry() on the first geometry field in the array

           // Modify implementation of SetFrom() to replicate all geometries
   }

Note: before RFC41, SetGeometry() or SetGeometryDirectly() could work on
a feature whose feature definition had a GetGeomType() == wkbNone (which
was inconsistent). This will be no longer the case since the size of the
papoGeometries array is now based on GetGeomFieldCount(), and when
GetGeomType() == wkbNone, the geometry field count is 0. The VRT and CSV
drivers will be fixed to declare their geometry type consistently.

OGRLayer class
~~~~~~~~~~~~~~

Impact on OGRLayer class :

-  Spatial filter: the option considered is to only allow one spatial
   filter at the time.

   -  the need for spatial filters applied simultaneously on several
      geometry fields is not obvious.
   -  the m_poFilterGeom protected member is used more than 250 times in
      the OGR code base, so turning it into an array would be a tedious
      task...

   Additions:

::

           protected:
               int m_iGeomFieldFilter // specify the index on which the spatial
                                      // filter is active.

           public:
               virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * );
               virtual void        SetSpatialFilterRect( int iGeomField,
                                                       double dfMinX, double dfMinY,
                                                       double dfMaxX, double dfMaxY );

::

   GetNextFeature() implementation must check the m_iGeomFieldFilter index
   in order to select the appropriate geometry field.

-  GetGeomType() : unchanged. For other fields, use
   GetLayerDefn()->GetGeomField(i)->GetType()

-  GetSpatialRef(): Currently the default implementation returns NULL.
   It will be changed to return
   GetLayerDefn()->GetGeomField(0)->GetSpatialRef() (if there is at
   least one geometry field). New drivers are encouraged not to
   specialize GetSpatialRef() anymore, but to appropriately set the SRS
   of their first geometry field. For other fields, use
   GetLayerDefn()->GetGeomField(i)->GetSpatialRef().

   Caveat: as SRS wasn't previously stored at the OGRFeatureDefn level,
   all existing drivers, if not updated, will have
   GetGeomField(0)->GetSpatialRef() returning NULL. The test_ogrsf
   utility will check and warn about this. Update of existing drivers
   will be made progressively. In the mean time, using
   OGRLayer::GetSpatialRef() will be advized to get the SRS of the first
   geometry field in a reliable way.

-  add :

::

           virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                                    int bForce = TRUE);

::

   Default implementation would call GetExtent() if iGeomField == 0

-  add :

::

           virtual OGRErr CreateGeomField(OGRGeomFieldDefn *poField);

-  no DeleteGeomField(), ReorderGeomFields() or AlterGeomFieldDefn() for
   now. Could be added later if the need arises.

-  GetGeometryColumn() : unchanged. Routed onto the first geometry
   field. For other fields, use
   GetLayerDefn()->GetGeomField(i)->GetNameRef()

-  SetIgnoredFields() : iterate over the geometry fields in addition to
   regular fields. The special "OGR_GEOMETRY" value will only apply to
   the first geometry field.

-  Intersection(), Union(), etc... : unchanged. Later improvements could
   use the papszOptions parameter to specify an alternate geometry field

-  TestCapability(): add a OLCCreateGeomField capability to inform if
   CreateGeomField() is implemented.

OGRDataSource class
~~~~~~~~~~~~~~~~~~~

Impact on OGRDataSource class :

-  CreateLayer() : signature will be unchanged. If more than one
   geometry fields are needed, OGRLayer::CreateGeomField() must be used.
   If the name of the first geometry field must be specified, for
   datasources supporting ODsCCreateGeomFieldAfterCreateLayer, using
   code should call CreateLayer() with eGType = wkbNone and then add all
   geometry fields with OGRLayer::CreateGeomField().

-  CopyLayer() : adapted to replicate all geometry fields (if supported
   by target layer)

-  ExecuteSQL() : takes a spatial filter. In the case of the generic OGR
   SQL implementation, this filter is a facility. It could also as well
   be applied on the returned layer object. So there is no real need for
   adding a way of specifying the geometry field at the ExecuteSQL() API
   level.

-  TestCapability(): add a ODsCCreateGeomFieldAfterCreateLayer
   capability to inform if CreateGeomField() is implemented after layer
   creation and that CreateLayer() can be safely called with eGType =
   wkbNone.

Explored alternative solutions
------------------------------

( This paragraph can be skipped if you are totally convinced by the
proposed approach detailed above :-) )

A possible alternative solution would have been to extend the existing
OGRFieldDefn object with information related to the geometry. That would
have involved adding a OFTGeometry value in the OGRFieldType
enumeration, and adding the OGRwkbGeometryType eGeomType and
OGRSpatialReference\* poSRS members to OGRFieldDefn. At OGRFeature class
level, the OGRField union could have been extended with a OGRGeometry\*
field. Similarly at OGRLayer level, CreateField() could have been used
to create new geometry fields.

The main drawback of this approach, which seems the most natural way, is
backward compatibility. This would have affected all places in OGR own
code or external code where fields are retrieved and geometry is not
expected. For example, in code like the following (very common in the
CreateFeature() of most drivers, or in user code consuming features
returned by GetNextFeature()) :

::

   switch( poFieldDefn->GetType() )
   {
           case OFTInteger: something1(poField->GetFieldAsInteger()); break;
           case OFTReal: something2(poField->GetFieldAsDouble()): break;
           default: something3(poField->GetFieldAsString()); break;
   }

This would lead, for legacy code, to geometry being handled as regular
field. We could imagine that GetFieldAsString() converts the geometry as
WKT, but it is doubtfull that this would really be desired.
Fundamentally, the handling of attribute and geometry fields is
different in most use cases.

(On the other side, if we introduce 64bit integer as a OGR type (this is
an RFC that is waiting for implementation...), the above code would
still produce a meaningful result. The string reprentation of a 64bit
integer is not that bad as a default behavior.)

GetFieldCount() would also take into account geometry fields, but in
most cases, you would need to subtract them.

A possible way of avoiding the above compatibility issue would be to
have 2 sets of API at OGRFeatureDefn and OGRFeature level. The current
one, that would ignore the geometry fields, and an "extended" one that
would take them into account. For example,
OGRFeatureDefn::GetFieldCountEx(), OGRFeatureDefn::GetFieldIndexEx(),
OGRFeatureDefn::GetFieldDefnEx(), OGRFeature::GetFieldEx(),
OGRFeature::SetFieldAsXXXEx() would take into account both attribute and
geometry fields. The annoying thing with that approach is the
duplication of the ~ 20 methods GetField() and SetFieldXXX() in
OGRFeature.

C API
-----

The following functions are added to the C API :

::

   /* OGRGeomFieldDefnH */

   typedef struct OGRGeomFieldDefnHS *OGRGeomFieldDefnH;

   OGRGeomFieldDefnH    CPL_DLL OGR_GFld_Create( const char *, OGRwkbGeometryType ) CPL_WARN_UNUSED_RESULT;
   void                 CPL_DLL OGR_GFld_Destroy( OGRGeomFieldDefnH );

   void                 CPL_DLL OGR_GFld_SetName( OGRGeomFieldDefnH, const char * );
   const char           CPL_DLL *OGR_GFld_GetNameRef( OGRGeomFieldDefnH );

   OGRwkbGeometryType   CPL_DLL OGR_GFld_GetType( OGRGeomFieldDefnH );
   void                 CPL_DLL OGR_GFld_SetType( OGRGeomFieldDefnH, OGRwkbGeometryType );

   OGRSpatialReferenceH CPL_DLL OGR_GFld_GetSpatialRef( OGRGeomFieldDefnH );
   void                 CPL_DLL OGR_GFld_SetSpatialRef( OGRGeomFieldDefnH,
                                                        OGRSpatialReferenceH hSRS );

   int                  CPL_DLL OGR_GFld_IsIgnored( OGRGeomFieldDefnH hDefn );
   void                 CPL_DLL OGR_GFld_SetIgnored( OGRGeomFieldDefnH hDefn, int );

   /* OGRFeatureDefnH */

   int               CPL_DLL OGR_FD_GetGeomFieldCount( OGRFeatureDefnH hFDefn );
   OGRGeomFieldDefnH CPL_DLL OGR_FD_GetGeomFieldDefn( OGRFeatureDefnH hFDefn, int i );
   int               CPL_DLL OGR_FD_GetGeomFieldIndex( OGRFeatureDefnH hFDefn, const char * );

   void              CPL_DLL OGR_FD_AddGeomFieldDefn( OGRFeatureDefnH hFDefn, OGRGeomFieldDefnH );
   OGRErr            CPL_DLL OGR_FD_DeleteGeomFieldDefn( OGRFeatureDefnH hFDefn, int iGeomField );

   /* OGRFeatureH */

   int               CPL_DLL OGR_F_GetGeomFieldCount( OGRFeatureH hFeat );
   OGRGeomFieldDefnH CPL_DLL OGR_F_GetGeomFieldDefnRef( OGRFeatureH hFeat, int iField );
   int               CPL_DLL OGR_F_GetGeomFieldIndex( OGRFeatureH hFeat, const char * pszName);

   OGRGeometryH      CPL_DLL OGR_F_GetGeomFieldRef( OGRFeatureH hFeat, int iField );
   OGRErr            CPL_DLL OGR_F_SetGeomFieldDirectly( OGRFeatureH hFeat, int iField, OGRGeometryH );
   OGRErr            CPL_DLL OGR_F_SetGeomField( OGRFeatureH hFeat, int iField, OGRGeometryH );

   /* OGRLayerH */

   void     CPL_DLL OGR_L_SetSpatialFilterEx( OGRLayerH, int iGeomField, OGRGeometryH );
   void     CPL_DLL OGR_L_SetSpatialFilterRectEx( OGRLayerH, int iGeomField,
                                                  double dfMinX, double dfMinY,
                                                  double dfMaxX, double dfMaxY );
   OGRErr   CPL_DLL OGR_L_GetExtentEx( OGRLayerH, int iGeomField,
                                       OGREnvelope *psExtent, int bForce );
   OGRErr   CPL_DLL OGR_L_CreateGeomField( OGRLayerH, OGRGeomFieldDefnH hFieldDefn );

OGR SQL engine
--------------

Currently, "SELECT fieldname1[, ...fieldnameN] FROM layername" returns
the specified fields, as well as the associated geometry. This behavior
is clearly not following the behavior of spatial RDBMS where the
geometry field must be explicitly specified.

The following compromise between backward compatibility and the new
capabilities of this RFC is adopted :

-  if no geometry field is explicitly specified in the SELECT clause,
   and there is only one geometry fields associated with the layer, then
   return it implicitly
-  otherwise, only return the explicitly mentioned geometry fields (or
   all geometry fields if "*" is used).

Limitations
~~~~~~~~~~~

-  Geometries from joined layers will not be fetched, as currently.
-  UNION ALL will only handle the default geometry, as currently. (could
   be extended in later work.)
-  The special fields OGR_GEOMETRY, OGR_GEOM_WKT and OGR_GEOM_AREA will
   operate on the first geometry field. It does not seem wise to extend
   this ad-hoc syntax. A better alternative will be the OGR SQLite
   dialect (with Spatialite support), once it is updated to support
   multi-geometry (not in the scope of this RFC)

Drivers
-------

Updated drivers in the context of this RFC
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  PostGIS:

   -  a ad-hoc form of support already exists. Tables with multiple
      geometries are reported currently as layers called
      "table_name(geometry_col_name)" (as many layers as geometry
      columns). This behavior will be changed so that the table is
      reported only once as a OGR layer.

-  PGDump:

   -  add write support for multi-geometry tables.

-  Memory:

   -  updated as a simple illustration of the new capabilities.

-  Interlis:

   -  updated to support multiple geometry fields (as well as other
      changes unrelated to this RFC)

Other candidate drivers (upgrade not originally covered by this RFC)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  GML driver : currently, only one geometry per feature reported.
   Possibility of changing this by hand-editing of the .gfs file -->
   implemented post RFC in GDAL 1.11
-  SQLite driver :

   -  currently, same behavior as current PostGIS driver.
   -  both the driver and the SQLite dialect could be updated to support
      multi-geometry layers. --> implemented post RFC in GDAL 2.0

-  Google Fusion Tables driver : currently, only the first found
   geometry column used. Possibility of specifying
   "table_name(geometry_column_name)" as the layer name passed to
   GetLayerByName().
-  VRT : some thoughts needed to find the syntax to support multiple
   geometries. Impacted XML syntax : . at OGRVRTLayer element level :
   GeometryType, LayerSRS, GeomField, SrcRegion,
   ExtentXMin/YMin/XMax/YMax, . at OGRVRTWarpedLayer element level : add
   new element to select the geometry field . at OGRVRTUnionLayer
   element level : GeometryType, LayerSRS, ExtentXMin/YMin/XMax/YMax -->
   implemented post RFC in GDAL 1.11
-  CSV : currently, take geometries from column named "WKT". To be
   extended to support multiple geometry columns. Not sure worth the
   effort. Could be done with the extended VRT driver. --> implemented
   post RFC in GDAL 1.11
-  WFS : currently, only single-geometry layers supported. The standard
   allows multi-geometry. Would require GML driver support first.
-  Other RDBMS based drivers: MySQL ?, MSSQLSpatial ? Oracle Spatial ?

Utilities
---------

ogrinfo
~~~~~~~

ogrinfo will be updated to report information related to multi-geometry
support. Output is expected to be unchanged w.r.t current output in the
case of single-geometry datasource.

Expected output for multi-geometry datasource:

::

   $ ogrinfo PG:dbname=mydb
   INFO: Open of `PG:dbname=mydb'
         using driver `PostgreSQL' successful.
   1: test_multi_geom (Polygon, Point)

::

   $ ogrinfo PG:dbname=mydb -al
   INFO: Open of `PG:dbname=mydb'
         using driver `PostgreSQL' successful.

   Layer name: test_multi_geom
   Geometry (polygon_geometry): Polygon
   Geometry (centroid_geometry): Point
   Feature Count: 10
   Extent (polygon_geometry): (400000,4500000) - (500000, 5000000)
   Extent (centroid_geometry): (2,48) - (3,49)
   Layer SRS WKT (polygon_geometry):
   PROJCS["WGS 84 / UTM zone 31N",
       GEOGCS["WGS 84",
           DATUM["WGS_1984",
               SPHEROID["WGS 84",6378137,298.257223563,
                   AUTHORITY["EPSG","7030"]],
               AUTHORITY["EPSG","6326"]],
           PRIMEM["Greenwich",0,
               AUTHORITY["EPSG","8901"]],
           UNIT["degree",0.0174532925199433,
               AUTHORITY["EPSG","9122"]],
           AUTHORITY["EPSG","4326"]],
       PROJECTION["Transverse_Mercator"],
       PARAMETER["latitude_of_origin",0],
       PARAMETER["central_meridian",3],
       PARAMETER["scale_factor",0.9996],
       PARAMETER["false_easting",500000],
       PARAMETER["false_northing",0],
       UNIT["metre",1,
           AUTHORITY["EPSG","9001"]],
       AXIS["Easting",EAST],
       AXIS["Northing",NORTH],
       AUTHORITY["EPSG","32631"]]
   Layer SRS WKT (centroid_geometry):
   GEOGCS["WGS 84",
       DATUM["WGS_1984",
           SPHEROID["WGS 84",6378137,298.257223563,
               AUTHORITY["EPSG","7030"]],
           AUTHORITY["EPSG","6326"]],
       PRIMEM["Greenwich",0,
           AUTHORITY["EPSG","8901"]],
       UNIT["degree",0.0174532925199433,
           AUTHORITY["EPSG","9122"]],
       AUTHORITY["EPSG","4326"]]
   FID Column = ogc_fid
   Geometry Column 1 = polygon_geometry
   Geometry Column 2 = centroid_geometry
   area: Real
   OGRFeature(test_multi_geom):1
     area (Real) = 500
     polygon_geometry = POLYGON ((400000 4500000,400000 5000000,500000 5000000,500000 4500000,400000 4500000))
     centroid_geometry = POINT(2.5 48.5)

A "-geomfield" option will be added to specify on which field the -spat
option applies.

ogr2ogr
~~~~~~~

Enhancements :

-  will translate multi-geometry layers into multi-geometry layers if
   supported by output layer (OLCCreateGeomField capability). In case it
   is not supported, only translates the first geometry.
-  "-select" option. If only attribute field names are specified, all
   input geometries will be implicitly selected (backward compatible
   behavior). If one or several geometry field names are specified,
   only those ones will be selected.
-  add a "-geomfield" option to specify on which field the -spat option
   applies
-  the various geometry transformations (reprojection, clipping, etc.)
   will be applied on all geometry fields.

test_ogrsf
~~~~~~~~~~

Will be enhanced with a few consistency checks :

-  OGRLayer::GetSpatialRef() ==
   OGRFeatureDefn::GetGeomField(0)->GetSpatialRef()
-  OGRLayer::GetGeomType() ==
   OGRFeatureDefn::GetGeomField(0)->GetGeomType()
-  OGRLayer::GetGeometryColumn() ==
   OGRFeatureDefn::GetGeomField(0)->GetNameRef()

Spatial filtering tests will loop over all geometry fields.

Documentation
-------------

In addition to function level documentation, the new capability will be
documented in the :ref:`vector_data_model` and :ref:`vector_api_tut` documents.

Python and other language bindings
----------------------------------

The new C API will be mapped to SWIG bindings. It will be only tested
with the Python bindings. No new typemaps are expected, so this should
work with other languages in a straightforward way.

Compatibility
-------------

-  Changes are only additions to the existing API, and existing
   behavior should be preserved, so this will be backwards compatible.

-  C++ ABI changes

-  Change of behavior in PostGIS driver w.r.t GDAL 1.10 for tables with
   multiple geometries.

Implementation
--------------

Even Rouault will implement the above described changes for GDAL 1.11
release, except the upgrade of the Interlis driver that will be done by
Pirmin Kalberer.

Funding
-------

This work is funded by the `Federal Office of Topography (swisstopo),
COGIS <http://www.swisstopo.admin.ch/internet/swisstopo/en/home/swisstopo/org/kogis.html>`__

Voting history
--------------

+1 from EvenR, FrankW, HowardB, DanielM and TamasS
