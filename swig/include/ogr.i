/******************************************************************************
 * $Id$
 *
 * Name:     ogr.i
 * Project:  GDAL Python Interface
 * Purpose:  OGR Core SWIG Interface declarations.
 * Author:   Howard Butler, hobu@iastate.edu
 *

 *
 * $Log$
 * Revision 1.52  2006/02/15 04:19:51  fwarmerdam
 * Added OFTDate.
 *
 * Revision 1.51  2006/02/02 21:09:24  collinsb
 * Corrected wrong Java typemap filename
 *
 * Revision 1.50  2006/02/02 20:52:40  collinsb
 * Added SWIG JAVA bindings
 *
 * Revision 1.49  2005/10/17 14:38:16  hobu
 * Implemented a poor excuse for a GetDriver method on OGRDataSourceShadow
 *
 * Revision 1.48  2005/10/11 14:47:36  kruland
 * Removed all the manual throws and changed to return OGRErr where appropriate.
 * There are 4 methods which have had serious alterations:
 * Layer.GetExtent() was changed to OGRErr GetExtent( double argout[4], int = 1);
 * Geometry.ExportToWkt() was changed to OGRErr ExportToWkt( char **argout );
 * CreateGeometryFromWkb() and CreateGeometryFromWkt will return a NULL
 * geometry object is the construction fails.  This will need to be handled
 * differently in a subsequent revision.
 *
 * Revision 1.47  2005/10/11 14:10:57  kruland
 * Feature.SetGeomerty() should not throw but should return the OGRErr.
 *
 * Revision 1.46  2005/10/03 20:09:58  cfis
 * Changed various methods to return bool instead of int.  These changes make the methods more natural to work with in scripting languages.
 *
 * Revision 1.45  2005/09/26 05:02:13  cfis
 * Added comments pointing out where parent objects are giving out references to child objects.
 *
 * Revision 1.44  2005/09/21 15:33:02  kruland
 * - Added DISOWN to ReleaseResultSet
 * - Added %newobject to GetSpatialFilter and GetOpenDS
 * - Added kwargs feature to Open and OpenShared - they must have been
 *   dropped during a previous commit.
 *
 * Revision 1.43  2005/09/21 02:21:56  fwarmerdam
 * added comment blocks
 *
 * Revision 1.42  2005/09/19 02:53:23  cfis
 * The two versions of the method OGRFeatureShadow::GetFieldDefnRef were both marked with the %newobject feature.
 *
 * However, this is wrong.  Neither return a newobject so this has been removed.
 *
 * Revision 1.41  2005/09/14 21:28:35  kruland
 * OGRFeatureDefn::GetFieldDefn() returns an internal pointer, not a newobject.
 *
 * Revision 1.40  2005/09/13 16:10:00  kruland
 * Import ogr_perl.i for SWIGPERL.
 * Moved GetFieldTypeName from Geometry to FieldDefn.
 *
 * Revision 1.39  2005/09/07 01:12:49  kruland
 * Have ogr.i include osr.i in order to have access to types defined in the osr
 * module.
 * Removed the typedef for OGRErr.  The %include osr.i caused it to be double
 * defined and caused warnings from swig.
 * Fixed the %constant declarations for strings.
 *
 * Revision 1.38  2005/09/06 01:49:07  kruland
 * Declare %feature("compactdefaultargs") so all bindings use it.
 * Import gdal_typemaps.i if no other language specific file used.
 * Added Geometry::GetFieldTypeName() binding.
 * Moved Open, OpenShared, GetDriver, and GetDriverByName from python
 * language file.
 *
 * Revision 1.37  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 * Revision 1.36  2005/08/25 21:02:30  cfis
 * Added check for SWIGRuby when defining open,openshared,GetDriverByName and GetDriver methods.
 *
 * Also moved all the renames into a preprocessor macro block because they interfere with the Ruby specific %rename directives.
 *
 * Revision 1.35  2005/08/22 19:00:50  kruland
 * Attempt to make ogr library behave well using normal script memory
 * management.
 * - Implemented public destructor for DataSource, Feature, FeatureDefn, FieldDefn,
 *   Geometry
 * - Stubbed Reference() and Dereference() for all classes as "pass"
 *   in python for backward compatibility
 * - Implemented Destroy() and Release() for all classes in python
 *   as calls to destructor along with disowning the C ptr so the destructor
 *   is not called again.  These methods are only for backward compatibility in
 *   python bindings.
 * - Added %newobject to Layer.GetFeature() and Layer.GetNextFeature()
 * - Use DISOWN typemap on Feature.SetGeometryDirectly() and
 *   Geometry.AddGeometryDirectly() because they assume ownership of the
 *   argument.
 * - Removed %newobject from Geometry.GetGeometryRef(), GetOpenDS() because they
 *   return internal references.
 *
 * Revision 1.34  2005/08/10 16:49:43  hobu
 * syntactic sugar to support the __iter__ protocol on Layer
 * for Python.  This allows us to (slowly) do for feature in layer and
 * have it call GetNextFeature for us until we are done.
 *
 * Revision 1.33  2005/08/10 15:44:57  hobu
 * Added some convenience properties for FieldDefn:
 * width, type, precision, name, justify as shortcuts for the
 * setters/getters
 *
 * Revision 1.32  2005/08/09 17:40:09  kruland
 * Added support for ruby.
 *
 * Revision 1.31  2005/08/09 14:38:27  kruland
 * Fixed the FeatureDefn constructor -- their decls don't have a return value.
 *
 * Revision 1.30  2005/08/06 20:51:58  kruland
 * Instead of using double_## defines and SWIG macros, use typemaps with
 * [ANY] specified and use $dim0 to extract the dimension.  This makes the
 * code quite a bit more readable.
 *
 * Revision 1.29  2005/08/05 20:32:11  kruland
 * Pass bytecount into OGR_G_CreateFromWkb.
 *
 * Revision 1.28  2005/08/05 15:49:27  kruland
 * Added __str__ to Geometry class python bindings.
 *
 * Revision 1.27  2005/06/22 18:42:33  kruland
 * Don't apply a typemap for returning OGRErr.
 *
 * Revision 1.26  2005/03/14 21:33:33  hobu
 * rename method names that are common to both gdal and ogr
 * for the C# only
 *
 * Revision 1.25  2005/03/10 17:13:57  hobu
 * #ifdefs for csharp
 *
 * Revision 1.24  2005/02/24 17:20:47  hobu
 * move constants to %constants so they are available to
 * all languages
 *
 * Revision 1.23  2005/02/24 16:15:53  hobu
 * ifdef'd the %pythoncode, conditionally rename some methods
 * if they conflict with existing "object" methods in C#.
 *
 * Revision 1.22  2005/02/22 18:44:34  hobu
 * and the one in the Datasource constructor
 *
 * Revision 1.21  2005/02/22 18:43:31  hobu
 * yank out debug refcount increments
 *
 * Revision 1.20  2005/02/22 18:24:34  hobu
 * apply the double_4 typemap for Layer.GetExtent
 *
 * Revision 1.19  2005/02/22 15:33:37  kruland
 * Removed duplicate defns of SetField that I introduced in previous revision.
 * Added %appy to SetField which will call str() on second arg.
 *
 * Revision 1.18  2005/02/22 02:04:33  kruland
 * Corrected decl of FieldDefn constructor.
 * Implemented first cut at Feature.SetField() -- needs to accept any for value.
 * Added constructor for FeatureDefn.
 * Make Geometry.GetEnvelope return a 4-tuple.
 * Implemented Geometry.Centroid()
 *
 * Revision 1.17  2005/02/21 23:09:33  hobu
 * Added all of the Geometry and FieldDefn classes/methods
 *
 * Revision 1.16  2005/02/21 21:27:31  kruland
 * Added AddPoint, AddGeometryDirectly, Destroy to Geometry.
 * Mucked with some more newobject directives.
 *
 * Revision 1.15  2005/02/21 20:48:11  kruland
 * Intermediate commit.  Changed internal typenames so they no longer use
 * those defined in gdal/ogr library.  Removed newobject from Dataset function
 * which return layers since they are owned by the dataset.  Moved the
 * Geometry factory methods so they are visible to the Geometry object
 * constructor.
 *
 * Revision 1.14  2005/02/21 18:56:54  kruland
 * Fix usage of the Geometry class.  The factory methods were confused by
 * returning voids instead of void*s.
 * Renamed the internal type to OGRGeometryShadow to prevent confusion.
 * Properly implemented Geometry.ExportToWkb() using the buffer typemaps.
 *
 * Revision 1.13  2005/02/21 18:00:44  hobu
 * Started in on Geometry
 *
 * Revision 1.12  2005/02/21 16:53:30  kruland
 * Changed name of SpatialReference shadow type.
 *
 * Revision 1.11  2005/02/21 16:52:39  hobu
 * GetFieldAs* methods that take in string or int input for Feature
 *
 * Revision 1.10  2005/02/18 23:47:11  hobu
 * Feature and FeatureDefn
 * except for the field handling methods on Feature
 *
 * Revision 1.9  2005/02/18 22:03:51  hobu
 * Finished up Layer
 *
 * Revision 1.8  2005/02/18 20:41:19  hobu
 * it compiles and it doesn't blow up.  IMO this is a
 * good stopping point :)
 *
 * Revision 1.7  2005/02/18 18:42:07  kruland
 * Added %feature("autodoc");
 *
 * Revision 1.6  2005/02/18 18:28:16  kruland
 * Rename OGRSpatialReferenceH type to SpatialReference to make the ogr and
 * osr modules compatible.
 *
 * Revision 1.5  2005/02/18 18:01:34  hobu
 * Started working on the geometry constructors with a lot
 * of help from Kevin
 *
 * Revision 1.4  2005/02/17 00:01:37  hobu
 * quick start on the datasource/driver stuff
 *
 * Revision 1.3  2005/02/16 21:55:02  hobu
 * started the OGRDriver class stuff.
 * CopyDataSource, CreateDataSource, and
 * DS Open
 *
 * Revision 1.2  2005/02/16 20:02:57  hobu
 * added constants
 *
 * Revision 1.1  2005/02/16 19:47:03  hobu
 * skeleton OGR interface
 *
 *
 *
 * nmake /f makefile.vc swig_python
 *D:\cvs\gdal\gdalautotest>d:\Python\debug\Python-2.4\PCbuild\python_d.exe run_all.py ogr
*/

%include "exception.i"

%module ogr

%feature("compactdefaultargs");
%feature("autodoc");

typedef int OGRwkbByteOrder;
typedef int OGRwkbGeometryType;
typedef int OGRFieldType;
typedef int OGRJustification;

%{
#include <iostream>
using namespace std;

#include "ogr_api.h"
#include "ogr_core.h"
#include "cpl_port.h"
#include "cpl_string.h"

typedef void OSRSpatialReferenceShadow;
typedef void OGRDriverShadow;
typedef void OGRDataSourceShadow;
typedef void OGRLayerShadow;
typedef void OGRFeatureShadow;
typedef void OGRFeatureDefnShadow;
typedef void OGRGeometryShadow;
typedef void OSRCoordinateTransformationShadow;
typedef void OGRFieldDefnShadow;
%}

%constant wkb25Bit = wkb25DBit;
%constant wkbUnknown = 0;

%constant wkbPoint = 1;
%constant wkbLineString = 2;
%constant wkbPolygon = 3;
%constant wkbMultiPoint = 4;
%constant wkbMultiLineString = 5;
%constant wkbMultiPolygon = 6;
%constant wkbGeometryCollection = 7;
%constant wkbNone = 100;
%constant wkbLinearRing = 101;
%constant wkbPoint25D =              wkbPoint              + wkb25DBit;
%constant wkbLineString25D =         wkbLineString         + wkb25DBit;
%constant wkbPolygon25D =            wkbPolygon            + wkb25DBit;
%constant wkbMultiPoint25D =         wkbMultiPoint         + wkb25DBit;
%constant wkbMultiLineString25D =    wkbMultiLineString    + wkb25DBit;
%constant wkbMultiPolygon25D =       wkbMultiPolygon       + wkb25DBit;
%constant wkbGeometryCollection25D = wkbGeometryCollection + wkb25DBit;

%constant OFTInteger = 0;
%constant OFTIntegerList= 1;
%constant OFTReal = 2;
%constant OFTRealList = 3;
%constant OFTString = 4;
%constant OFTStringList = 5;
%constant OFTWideString = 6;
%constant OFTWideStringList = 7;
%constant OFTBinary = 8;
%constant OFTDate = 8;

%constant OJUndefined = 0;
%constant OJLeft = 1;
%constant OJRight = 2;

%constant wkbXDR = 0;
%constant wkbNDR = 1;

%constant char *OLCRandomRead          = "RandomRead";
%constant char *OLCSequentialWrite     = "SequentialWrite";
%constant char *OLCRandomWrite         = "RandomWrite";
%constant char *OLCFastSpatialFilter   = "FastSpatialFilter";
%constant char *OLCFastFeatureCount    = "FastFeatureCount";
%constant char *OLCFastGetExtent       = "FastGetExtent";
%constant char *OLCCreateField         = "CreateField";
%constant char *OLCTransactions        = "Transactions";
%constant char *OLCDeleteFeature       = "DeleteFeature";
%constant char *OLCFastSetNextByIndex  = "FastSetNextByIndex";

%constant char *ODsCCreateLayer        = "CreateLayer";
%constant char *ODsCDeleteLayer        = "DeleteLayer";

%constant char *ODrCCreateDataSource   = "CreateDataSource";
%constant char *ODrCDeleteDataSource   = "DeleteDataSource";

#if defined(SWIGPYTHON)
%include ogr_python.i
#elif defined(SWIGRUBY)
%include ogr_ruby.i
#elif defined(SWIGPHP4)
%include ogr_php.i
#elif defined(SWIGCSHARP)
%include ogr_csharp.i
#elif defined(SWIGPERL)
%include ogr_perl.i
#elif defined(SWIGJAVA)
%include ogr_java.i
#else
%include gdal_typemaps.i
#endif

/*
 * We need to import osr.i here so the ogr module knows about the
 * wrapper for SpatialReference and CoordinateSystem from osr.
 * These types are used in Geometry::Transform() among others.
 * This was primarily a problem in the perl bindings because
 * perl names things differently when using -proxy (default) argument
 */
%import osr.i

/************************************************************************/
/*                              OGRDriver                               */
/************************************************************************/

%rename (Driver) OGRDriverShadow;

class OGRDriverShadow {
  OGRDriverShadow();
  ~OGRDriverShadow();
public:
%extend {

%immutable;
  char const *name;
%mutable;

%newobject CreateDataSource;
%feature( "kwargs" ) CreateDataSource;
  OGRDataSourceShadow *CreateDataSource( const char *name, 
                                    char **options = 0 ) {
    OGRDataSourceShadow *ds = (OGRDataSourceShadow*) OGR_Dr_CreateDataSource( self, name, options);
    return ds;
  }
  
%newobject CopyDataSource;
%feature( "kwargs" ) CopyDataSource;
  OGRDataSourceShadow *CopyDataSource( OGRDataSourceShadow* copy_ds, 
                                  const char* name, 
                                  char **options = 0 ) {
    OGRDataSourceShadow *ds = (OGRDataSourceShadow*) OGR_Dr_CopyDataSource(self, copy_ds, name, options);
    return ds;
  }
  
%newobject Open;
%feature( "kwargs" ) Open;
  OGRDataSourceShadow *Open( const char* name, 
                        int update=0 ) {
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*) OGR_Dr_Open(self, name, update);
    return ds;
  }

  int DeleteDataSource( const char *name ) {
    return OGR_Dr_DeleteDataSource( self, name );
  }

  bool TestCapability (const char *cap) {
    return OGR_Dr_TestCapability(self, cap);
  }
  
  const char * GetName() {
    return OGR_Dr_GetName( self );
  }


} /* %extend */
}; /* class OGRDriverShadow */


/************************************************************************/
/*                            OGRDataSource                             */
/************************************************************************/


%rename (DataSource) OGRDataSourceShadow;

class OGRDataSourceShadow {
  OGRDataSourceShadow() {
  }
public:
%extend {

%immutable;
  char const *name;
%mutable;

  ~OGRDataSourceShadow() {
    OGRReleaseDataSource(self);
  }

  int GetRefCount() {
    return OGR_DS_GetRefCount(self);
  }
  
  int GetSummaryRefCount() {
    return OGR_DS_GetSummaryRefCount(self);
  }
  
  int GetLayerCount() {
    return OGR_DS_GetLayerCount(self);
  }
  
  OGRDriverShadow * GetDriver() {
    OGRDriverShadow* driver;
    OGRDataSourceShadow* ds;
    ds = (OGRDataSourceShadow*)OGROpen((const char *) OGR_DS_GetName(self),0,&driver);
    OGRReleaseDataSource(ds);
    return driver;
  }
  const char * GetName() {
    return OGR_DS_GetName(self);
  }
  
  OGRErr DeleteLayer(int index){
    return OGR_DS_DeleteLayer(self, index);
  }

  /* Note that datasources own their layers */
  %feature( "kwargs" ) CreateLayer;
  OGRLayerShadow *CreateLayer(const char* name, 
              OSRSpatialReferenceShadow* reference=NULL,
              OGRwkbGeometryType geom_type=wkbUnknown,
              char** options=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_CreateLayer( self,
                                                        name,
                                                        reference,
                                                        geom_type,
                                                        options);
    return layer;
  }

  %feature( "kwargs" ) CopyLayer;
  OGRLayerShadow *CopyLayer(OGRLayerShadow *src_layer,
            const char* new_name,
            char** options=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_CopyLayer( self,
                                                      src_layer,
                                                      new_name,
                                                      options);
    return layer;
  }
  
  %feature( "kwargs" ) GetLayerByIndex;
  OGRLayerShadow *GetLayerByIndex( int index=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_GetLayer(self, index);
    return layer;
  }

  OGRLayerShadow *GetLayerByName( const char* layer_name) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_GetLayerByName(self, layer_name);
    return layer;
  }

  bool TestCapability(const char * cap) {
    return OGR_DS_TestCapability(self, cap);
  }

  %newobject ExecuteSQL;
  %feature( "kwargs" ) ExecuteSQL;
  OGRLayerShadow *ExecuteSQL(const char* statement,
                        OGRGeometryShadow* geom=NULL,
                        const char* dialect="") {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_ExecuteSQL((OGRDataSourceShadow*)self,
                                                      statement,
                                                      geom,
                                                      dialect);
    return layer;
  }
  
%apply SWIGTYPE *DISOWN {OGRLayerShadow *layer};
  void ReleaseResultSet(OGRLayerShadow *layer){
    OGR_DS_ReleaseResultSet(self, layer);
  }
%clear OGRLayerShadow *layer;

} /* %extend */


}; /* class OGRDataSourceShadow */

/************************************************************************/
/*                               OGRLayer                               */
/************************************************************************/

%rename (Layer) OGRLayerShadow;
class OGRLayerShadow {
  OGRLayerShadow();
  ~OGRLayerShadow();
public:
%extend {

  int GetRefCount() {
    return OGR_L_GetRefCount(self);
  }
  
  void SetSpatialFilter(OGRGeometryShadow* filter) {
    OGR_L_SetSpatialFilter (self, filter);
  }
  
  void SetSpatialFilterRect( double minx, double miny,
                             double maxx, double maxy) {
    OGR_L_SetSpatialFilterRect(self, minx, miny, maxx, maxy);                          
  }
  
  OGRGeometryShadow *GetSpatialFilter() {
    return (OGRGeometryShadow *) OGR_L_GetSpatialFilter(self);
  }

  OGRErr SetAttributeFilter(char* filter_string) {
    return OGR_L_SetAttributeFilter((OGRLayerShadow*)self, filter_string);
  }
  
  void ResetReading() {
    OGR_L_ResetReading(self);
  }
  
  const char * GetName() {
    return OGR_FD_GetName(OGR_L_GetLayerDefn(self));
  }
  
%newobject GetFeature;
  OGRFeatureShadow *GetFeature(long fid) {
    return (OGRFeatureShadow*) OGR_L_GetFeature(self, fid);
  }
  
%newobject GetNextFeature;
  OGRFeatureShadow *GetNextFeature() {
    return (OGRFeatureShadow*) OGR_L_GetNextFeature(self);
  }
  
  OGRErr SetNextByIndex(long new_index) {
    return OGR_L_SetNextByIndex(self, new_index);
  }
  
  OGRErr SetFeature(OGRFeatureShadow *feature) {
    return OGR_L_SetFeature(self, feature);
  }
  

  OGRErr CreateFeature(OGRFeatureShadow *feature) {
    return OGR_L_CreateFeature(self, feature);
  }
  
  OGRErr DeleteFeature(long fid) {
    return OGR_L_DeleteFeature(self, fid);
  }
  
  OGRErr SyncToDisk() {
    return OGR_L_SyncToDisk(self);
  }
  
  OGRFeatureDefnShadow *GetLayerDefn() {
    return (OGRFeatureDefnShadow*) OGR_L_GetLayerDefn(self);
  }

  %feature( "kwargs" ) GetFeatureCount;  
  int GetFeatureCount(int force=1) {
    return OGR_L_GetFeatureCount(self, force);
  }
  
  %feature( "kwargs" ) GetExtent;
  OGRErr GetExtent(double argout[4], int force=1) {
    return OGR_L_GetExtent(self, (OGREnvelope*)argout, force);
  }

  bool TestCapability(const char* cap) {
    return OGR_L_TestCapability(self, cap);
  }
  
  %feature( "kwargs" ) CreateField;
  OGRErr CreateField(OGRFieldDefnShadow* field_def, int approx_ok = 1) {
    return OGR_L_CreateField(self, field_def, approx_ok);
  }
  
  OGRErr StartTransaction() {
    return OGR_L_StartTransaction(self);
  }
  
  OGRErr CommitTransaction() {
    return OGR_L_CommitTransaction(self);
  }

  OGRErr RollbackTransaction() {
    return OGR_L_RollbackTransaction(self);
  }
  
  OSRSpatialReferenceShadow *GetSpatialRef() {
    return (OSRSpatialReferenceShadow*) OGR_L_GetSpatialRef(self);
  }
  
  GIntBig GetFeatureRead() {
    return OGR_L_GetFeaturesRead(self);
  }

} /* %extend */


}; /* class OGRLayerShadow */

/************************************************************************/
/*                              OGRFeature                              */
/************************************************************************/

%rename (Feature) OGRFeatureShadow;
class OGRFeatureShadow {
  OGRFeatureShadow();
public:
%extend {

  ~OGRFeatureShadow() {
    OGR_F_Destroy(self);
  }

  %feature("kwargs") OGRFeatureShadow;
  OGRFeatureShadow( OGRFeatureDefnShadow *feature_def = 0 ) {
    return (OGRFeatureShadow*) OGR_F_Create( feature_def );
  }

  OGRFeatureDefnShadow *GetDefnRef() {
    return (OGRFeatureDefnShadow*) OGR_F_GetDefnRef(self);
  }
  
  OGRErr SetGeometry(OGRGeometryShadow* geom) {
    return OGR_F_SetGeometry(self, geom);
  }

/* The feature takes over owernship of the geometry. */
%apply SWIGTYPE *DISOWN {OGRGeometryShadow *geom};
  OGRErr SetGeometryDirectly(OGRGeometryShadow* geom) {
    return OGR_F_SetGeometryDirectly(self, geom);
  }
%clear OGRGeometryShadow *geom;
  
  /* Feature owns its geometry */
  OGRGeometryShadow *GetGeometryRef() {
    return (OGRGeometryShadow*) OGR_F_GetGeometryRef(self);
  }
  
  %newobject Clone;
  OGRFeatureShadow *Clone() {
    return (OGRFeatureShadow*) OGR_F_Clone(self);
  }
  
  bool Equal(OGRFeatureShadow *feature) {
    return OGR_F_Equal(self, feature);
  }
  
  int GetFieldCount() {
    return OGR_F_GetFieldCount(self);
  }

  /* ---- GetFieldDefnRef --------------------- */
  OGRFieldDefnShadow *GetFieldDefnRef(int id) {
    return (OGRFieldDefnShadow *) OGR_F_GetFieldDefnRef(self, id);
  }

  OGRFieldDefnShadow *GetFieldDefnRef(const char* name) {
    return (OGRFieldDefnShadow *) OGR_F_GetFieldDefnRef(self, OGR_F_GetFieldIndex(self, name));
  }
  /* ------------------------------------------- */

  /* ---- GetFieldAsString --------------------- */

  const char* GetFieldAsString(int id) {
    return (const char *) OGR_F_GetFieldAsString(self, id);
  }

  const char* GetFieldAsString(const char* name) {
    return (const char *) OGR_F_GetFieldAsString(self, OGR_F_GetFieldIndex(self, name));
  }
  /* ------------------------------------------- */

  /* ---- GetFieldAsInteger -------------------- */

  int GetFieldAsInteger(int id) {
    return OGR_F_GetFieldAsInteger(self, id);
  }

  int GetFieldAsInteger(const char* name) {
    return OGR_F_GetFieldAsInteger(self, OGR_F_GetFieldIndex(self, name));
  }
  /* ------------------------------------------- */  

  /* ---- GetFieldAsDouble --------------------- */

  double GetFieldAsDouble(int id) {
    return OGR_F_GetFieldAsDouble(self, id);
  }

  double GetFieldAsDouble(const char* name) {
    return OGR_F_GetFieldAsDouble(self, OGR_F_GetFieldIndex(self, name));
  }
  /* ------------------------------------------- */  


  
  /* ---- IsFieldSet --------------------------- */
  bool IsFieldSet(int id) {
    return OGR_F_IsFieldSet(self, id);
  }

  bool IsFieldSet(const char* name) {
    return OGR_F_IsFieldSet(self, OGR_F_GetFieldIndex(self, name));
  }
  /* ------------------------------------------- */  
      
  int GetFieldIndex(const char* name) {
    return OGR_F_GetFieldIndex(self, name);
  }

  int GetFID() {
    return OGR_F_GetFID(self);
  }
  
  OGRErr SetFID(int fid) {
    return OGR_F_SetFID(self, fid);
  }
  
  void DumpReadable() {
    OGR_F_DumpReadable(self, NULL);
  }

  void UnsetField(int id) {
    OGR_F_UnsetField(self, id);
  }


  void UnsetField(const char* name) {
    OGR_F_UnsetField(self, OGR_F_GetFieldIndex(self, name));
  }

  /* ---- SetField ----------------------------- */
  
  %apply ( tostring argin ) { (const char* value) };
  void SetField(int id, const char* value) {
    OGR_F_SetFieldString(self, id, value);
  }

  void SetField(const char* name, const char* value) {
    OGR_F_SetFieldString(self, OGR_F_GetFieldIndex(self, name), value);
  }
  %clear (const char* value );

  /* ------------------------------------------- */  
  
  %feature("kwargs") SetFrom;
  OGRErr SetFrom(OGRFeatureShadow *other, int forgiving=1) {
    return OGR_F_SetFrom(self, other, forgiving);
  }
  
  const char *GetStyleString() {
    return (const char*) OGR_F_GetStyleString(self);
  }
  
  void SetStyleString(const char* the_string) {
    OGR_F_SetStyleString(self, the_string);
  }

  /* ---- GetFieldType ------------------------- */  
  OGRFieldType GetFieldType(int id) {
    return (OGRFieldType) OGR_Fld_GetType( OGR_F_GetFieldDefnRef( self, id));
  }
  
  OGRFieldType GetFieldType(const char* name, const char* value) {
    return (OGRFieldType) OGR_Fld_GetType( 
                            OGR_F_GetFieldDefnRef( self,  
                                                   OGR_F_GetFieldIndex(self, 
                                                                       name)
                                                  )
                                          );
    
  }
  /* ------------------------------------------- */  
  
} /* %extend */


}; /* class OGRFeatureShadow */

/************************************************************************/
/*                            OGRFeatureDefn                            */
/************************************************************************/

%rename (FeatureDefn) OGRFeatureDefnShadow;
class OGRFeatureDefnShadow {
  OGRFeatureDefnShadow();
public:
%extend {
  
  ~OGRFeatureDefnShadow() {
    OGR_FD_Destroy(self);
  }

  %feature("kwargs") OGRFeatureDefnShadow;
  OGRFeatureDefnShadow(const char* name=NULL) {
    return (OGRFeatureDefnShadow* )OGR_FD_Create(name);
  }
  
  const char* GetName(){
    return OGR_FD_GetName(self);
  }
  
  int GetFieldCount(){
    return OGR_FD_GetFieldCount(self);
  }
  
  /* FeatureDefns own their FieldDefns */
  OGRFieldDefnShadow* GetFieldDefn(int i){
    return (OGRFieldDefnShadow*) OGR_FD_GetFieldDefn(self, i);
  }

  int GetFieldIndex(const char* name) {
    return OGR_FD_GetFieldIndex(self, name);
  }
  
  void AddFieldDefn(OGRFieldDefnShadow* defn) {
    OGR_FD_AddFieldDefn(self, defn);
  }
  
  OGRwkbGeometryType GetGeomType() {
    return (OGRwkbGeometryType) OGR_FD_GetGeomType(self);
  }
  
  void SetGeomType(OGRwkbGeometryType geom_type) {
    OGR_FD_SetGeomType(self, geom_type);
  }
  
  int GetReferenceCount(){
    return OGR_FD_GetReferenceCount(self);
  }
  
} /* %extend */


}; /* class OGRFeatureDefnShadow */

/************************************************************************/
/*                             OGRFieldDefn                             */
/************************************************************************/

%rename (FieldDefn) OGRFieldDefnShadow;

class OGRFieldDefnShadow {
  OGRFieldDefnShadow();
public:
%extend {

  ~OGRFieldDefnShadow() {
    OGR_Fld_Destroy(self);
  }

  %feature("kwargs") OGRFieldDefnShadow;
  OGRFieldDefnShadow( const char* name="unnamed", 
                      OGRFieldType field_type=OFTString) {
    return (OGRFieldDefnShadow*) OGR_Fld_Create(name, field_type);
  }

  const char * GetName() {
    return (const char *) OGR_Fld_GetNameRef(self);
  }
  
  const char * GetNameRef() {
    return (const char *) OGR_Fld_GetNameRef(self);
  }
  
  void SetName( const char* name) {
    OGR_Fld_SetName(self, name);
  }
  
  OGRFieldType GetType() {
    return OGR_Fld_GetType(self);
  }

  void SetType(OGRFieldType type) {
    OGR_Fld_SetType(self, type);
  }
  
  OGRJustification GetJustify() {
    return OGR_Fld_GetJustify(self);
  }
  
  void SetJustify(OGRJustification justify) {
    OGR_Fld_SetJustify(self, justify);
  }
  
  int GetWidth () {
    return OGR_Fld_GetWidth(self);
  }
  
  void SetWidth (int width) {
    OGR_Fld_SetWidth(self, width);
  }
  
  int GetPrecision() {
    return OGR_Fld_GetPrecision(self);
  }
  
  void SetPrecision(int precision) {
    OGR_Fld_SetPrecision(self, precision);
  }

  const char * GetFieldTypeName(OGRFieldType type) {
    return OGR_GetFieldTypeName(type);
  }

} /* %extend */


}; /* class OGRFieldDefnShadow */


/* -------------------------------------------------------------------- */
/*      Geometry factory methods.                                       */
/* -------------------------------------------------------------------- */
%feature( "kwargs" ) CreateGeometryFromWkb;
%newobject CreateGeometryFromWkb;
%apply (int nLen, char *pBuf ) { (int len, char *bin_string)};
%inline %{
  OGRGeometryShadow* CreateGeometryFromWkb( int len, char *bin_string, 
                                            OSRSpatialReferenceShadow *reference=NULL ) {
    void *geom;
    OGRErr err = OGR_G_CreateFromWkb( (unsigned char *) bin_string,
                                      reference,
                                      &geom,
                                      len );
    if (err != 0 )
       return NULL;
    return (OGRGeometryShadow*) geom;
  }
 
%}
%clear (int len, char *bin_string);

%feature( "kwargs" ) CreateGeometryFromWkt;
%apply (char **ignorechange) { (char **) };
%newobject CreateGeometryFromWkt;
%inline %{
  OGRGeometryShadow* CreateGeometryFromWkt( char **val, 
                                      OSRSpatialReferenceShadow *reference=NULL ) {
    void *geom;
    OGRErr err = OGR_G_CreateFromWkt(val,
                                      reference,
                                      &geom);
    if (err != 0 )
       return NULL;
    return (OGRGeometryShadow*) geom;
  }
 
%}
%clear (char **);

%newobject CreateGeometryFromGML;
%inline %{
  OGRGeometryShadow *CreateGeometryFromGML( const char * input_string ) {
    OGRGeometryShadow* geom = (OGRGeometryShadow*)OGR_G_CreateFromGML(input_string);
    return geom;
  }
 
%}

/************************************************************************/
/*                             OGRGeometry                              */
/************************************************************************/

%rename (Geometry) OGRGeometryShadow;
class OGRGeometryShadow {
  OGRGeometryShadow();
public:
%extend {
    
  ~OGRGeometryShadow() {
    OGR_G_DestroyGeometry( self );
  }

  %feature("kwargs") OGRGeometryShadow;
  OGRGeometryShadow( OGRwkbGeometryType type = wkbUnknown, char *wkt = 0, int wkb= 0, char *wkb_buf = 0, char *gml = 0 ) {
    if (type != wkbUnknown ) {
      return (OGRGeometryShadow*) OGR_G_CreateGeometry( type );
    }
    else if ( wkt != 0 ) {
      return CreateGeometryFromWkt( &wkt );
    }
    else if ( wkb != 0 ) {
      return CreateGeometryFromWkb( wkb, wkb_buf );
    }
    else if ( gml != 0 ) {
      return CreateGeometryFromGML( gml );
    }
    // throw?
    else return 0;
  }

  OGRErr ExportToWkt( char** argout ) {
    return OGR_G_ExportToWkt(self, argout);
  }

  %feature("kwargs") ExportToWkb;
  OGRErr ExportToWkb( int *nLen, char **pBuf, OGRwkbByteOrder byte_order=wkbXDR ) {
    *nLen = OGR_G_WkbSize( self );
    *pBuf = (char *) malloc( *nLen * sizeof(unsigned char) );
    return OGR_G_ExportToWkb(self, byte_order, (unsigned char*) *pBuf );
  }

  const char * ExportToGML() {
    return (const char *) OGR_G_ExportToGML(self);
  }

  %feature("kwargs") AddPoint;
  void AddPoint(double x, double y, double z = 0) {
    OGR_G_AddPoint( self, x, y, z );
  }

/* The geometry now owns an inner geometry */
%apply SWIGTYPE *DISOWN {OGRGeometryShadow* other};
  OGRErr AddGeometryDirectly( OGRGeometryShadow* other ) {
    return OGR_G_AddGeometryDirectly( self, other );
  }
%clear OGRGeometryShadow* other;

  OGRErr AddGeometry( OGRGeometryShadow* other ) {
    return OGR_G_AddGeometry( self, other );
  }

  %newobject Clone;
  OGRGeometryShadow* Clone() {
    return (OGRGeometryShadow*) OGR_G_Clone(self);
  } 
    
  OGRwkbGeometryType GetGeometryType() {
    return (OGRwkbGeometryType) OGR_G_GetGeometryType(self);
  }

  const char * GetGeometryName() {
    return (const char *) OGR_G_GetGeometryName(self);
  }
  
  double GetArea() {
    return OGR_G_GetArea(self);
  }
  
  int GetPointCount() {
    return OGR_G_GetPointCount(self);
  }

  %feature("kwargs") GetX;  
  double GetX(int point=0) {
    return OGR_G_GetX(self, point);
  }

  %feature("kwargs") GetY;  
  double GetY(int point=0) {
    return OGR_G_GetY(self, point);
  }

  %feature("kwargs") GetZ;  
  double GetZ(int point=0) {
    return OGR_G_GetZ(self, point);
  } 
  
  int GetGeometryCount() {
    return OGR_G_GetGeometryCount(self);
  }

  %feature("kwargs") SetPoint;    
  void SetPoint(int point, double x, double y, double z=0) {
    OGR_G_SetPoint(self, point, x, y, z);
  }
  
  /* Geometries own their internal geometries */
  OGRGeometryShadow* GetGeometryRef(int geom) {
    return (OGRGeometryShadow*) OGR_G_GetGeometryRef(self, geom);
  }

  %newobject GetBoundary;
  OGRGeometryShadow* GetBoundary() {
    return (OGRGeometryShadow*) OGR_G_GetBoundary(self);
  }  

  %newobject ConvexHull;
  OGRGeometryShadow* ConvexHull() {
    return (OGRGeometryShadow*) OGR_G_ConvexHull(self);
  } 

  %newobject Buffer;
  %feature("kwargs") Buffer; 
  OGRGeometryShadow* Buffer( double distance, int quadsecs=30 ) {
    return (OGRGeometryShadow*) OGR_G_Buffer( self, distance, quadsecs );
  }

  %newobject Intersection;
  OGRGeometryShadow* Intersection( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_Intersection( self, other );
  }  
  
  %newobject Union;
  OGRGeometryShadow* Union( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_Union( self, other );
  }  
  
  %newobject Difference;
  OGRGeometryShadow* Difference( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_Difference( self, other );
  }  

  %newobject SymmetricDifference;
  OGRGeometryShadow* SymmetricDifference( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_SymmetricDifference( self, other );
  } 
  
  double Distance( OGRGeometryShadow* other) {
    return OGR_G_Distance(self, other);
  }
  
  void Empty () {
    OGR_G_Empty(self);
  }
  
  bool Intersect (OGRGeometryShadow* other) {
    return OGR_G_Intersect(self, other);
  }

  bool Equal (OGRGeometryShadow* other) {
    return OGR_G_Equal(self, other);
  }
  
  bool Disjoint(OGRGeometryShadow* other) {
    return OGR_G_Disjoint(self, other);
  }

  bool Touches (OGRGeometryShadow* other) {
    return OGR_G_Touches(self, other);
  }

  bool Crosses (OGRGeometryShadow* other) {
    return OGR_G_Crosses(self, other);
  }

  bool Within (OGRGeometryShadow* other) {
    return OGR_G_Within(self, other);
  }

  bool Contains (OGRGeometryShadow* other) {
    return OGR_G_Contains(self, other);
  }
  
  bool Overlaps (OGRGeometryShadow* other) {
    return OGR_G_Overlaps(self, other);
  }

  OGRErr TransformTo(OSRSpatialReferenceShadow* reference) {
    return OGR_G_TransformTo(self, reference);
  }
  
  OGRErr Transform(OSRCoordinateTransformationShadow* trans) {
    return OGR_G_Transform(self, trans);
  }
  
  OSRSpatialReferenceShadow* GetSpatialReference() {
    return (OSRSpatialReferenceShadow*)OGR_G_GetSpatialReference(self);
  }
  
  void AssignSpatialReference(OSRSpatialReferenceShadow* reference) {
    OGR_G_AssignSpatialReference(self, reference);
  }
  
  void CloseRings() {
    OGR_G_CloseRings(self);
  }
  
  void FlattenTo2D() {
    OGR_G_FlattenTo2D(self);
  }
    
  void GetEnvelope(double argout[4]) {
    OGR_G_GetEnvelope(self, (OGREnvelope*)argout);
  }

  %newobject Centroid;
  OGRGeometryShadow* Centroid() {
    OGRGeometryShadow *pt = new_OGRGeometryShadow( wkbPoint );
    OGRErr rcode = OGR_G_Centroid( self, pt );
    return pt;
  }
  
  int WkbSize() {
    return OGR_G_WkbSize(self);
  }
  
  int GetCoordinateDimension() {
    return OGR_G_GetCoordinateDimension(self);
  }
  
  int GetDimension() {
    return OGR_G_GetDimension(self);
  }

} /* %extend */


}; /* class OGRGeometryShadow */

/************************************************************************/
/*                        Other misc functions.                         */
/************************************************************************/

%{
char const *OGRDriverShadow_get_name( OGRDriverShadow *h ) {
  return OGR_Dr_GetName( h );
}

char const *OGRDataSourceShadow_get_name( OGRDataSourceShadow *h ) {
  return OGR_DS_GetName( h );
}

char const *OGRDriverShadow_name_get( OGRDriverShadow *h ) {
  return OGR_Dr_GetName( h );
}

char const *OGRDataSourceShadow_name_get( OGRDataSourceShadow *h ) {
  return OGR_DS_GetName( h );
}
%}

int OGRGetDriverCount();

int OGRGetOpenDSCount();

OGRErr OGRSetGenerate_DB2_V72_BYTE_ORDER(int bGenerate_DB2_V72_BYTE_ORDER);

void OGRRegisterAll();

%inline %{
  OGRDataSourceShadow* GetOpenDS(int ds_number) {
    OGRDataSourceShadow* layer = (OGRDataSourceShadow*) OGRGetOpenDS(ds_number);
    return layer;
  }
%}

%newobject Open;
%feature( "kwargs" ) Open;
%inline %{
  OGRDataSourceShadow* Open( const char *filename, int update =0 ) {
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpen(filename,update,NULL);
    return ds;
  }
%}

%newobject OpenShared;
%feature( "kwargs" ) OpenShared;
%inline %{
  OGRDataSourceShadow* OpenShared( const char *filename, int update =0 ) {
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpenShared(filename,update,NULL);
    return ds;
  }
%}

%inline %{
OGRDriverShadow* GetDriverByName( char const *name ) {
  return (OGRDriverShadow*) OGRGetDriverByName( name );
}

OGRDriverShadow* GetDriver(int driver_number) {
  return (OGRDriverShadow*) OGRGetDriver(driver_number);
}
%}

