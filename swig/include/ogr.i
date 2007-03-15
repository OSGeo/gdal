/******************************************************************************
 * $Id$
 *
 * Project:  OGR Core SWIG Interface declarations.
 * Purpose:  OGR declarations.
 * Author:   Howard Butler, hobu@iastate.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Howard Butler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

%include "exception.i"

#ifdef PERL_CPAN_NAMESPACE
%module "Geo::OGR"
#elif defined(SWIGCSHARP)
%module Ogr
#else
%module ogr
#endif

#ifdef SWIGCSHARP
%include swig_csharp_extensions.i
%implement_class(SWIGTYPE)
#endif

%feature("compactdefaultargs");
%feature("autodoc");

/************************************************************************/
/*                         Enumerated types                             */
/************************************************************************/

#ifndef SWIGCSHARP
typedef int OGRwkbByteOrder;
typedef int OGRwkbGeometryType;
typedef int OGRFieldType;
typedef int OGRJustification;
#else
%rename (wkbByteOrder) OGRwkbByteOrder;
typedef enum 
{
    wkbXDR = 0,         /* MSB/Sun/Motoroloa: Most Significant Byte First   */
    wkbNDR = 1          /* LSB/Intel/Vax: Least Significant Byte First      */
} OGRwkbByteOrder;

%rename (wkbGeometryType) OGRwkbGeometryType;
typedef enum 
{
    wkbUnknown = 0,             /* non-standard */
    wkbPoint = 1,               /* rest are standard WKB type codes */
    wkbLineString = 2,
    wkbPolygon = 3,
    wkbMultiPoint = 4,
    wkbMultiLineString = 5,
    wkbMultiPolygon = 6,
    wkbGeometryCollection = 7,
    wkbNone = 100,              /* non-standard, for pure attribute records */
    wkbLinearRing = 101,        /* non-standard, just for createGeometry() */
    wkbPoint25D = -2147483647,   /* 2.5D extensions as per 99-402 */
    wkbLineString25D = -2147483646,
    wkbPolygon25D = -2147483645,
    wkbMultiPoint25D = -2147483644,
    wkbMultiLineString25D = -2147483643,
    wkbMultiPolygon25D = -2147483642,
    wkbGeometryCollection25D = -2147483641
} OGRwkbGeometryType;

%rename (FieldType) OGRFieldType;
typedef enum 
{
  /** Simple 32bit integer */                   OFTInteger = 0,
  /** List of 32bit integers */                 OFTIntegerList = 1,
  /** Double Precision floating point */        OFTReal = 2,
  /** List of doubles */                        OFTRealList = 3,
  /** String of ASCII chars */                  OFTString = 4,
  /** Array of strings */                       OFTStringList = 5,
  /** Double byte string (unsupported) */       OFTWideString = 6,
  /** List of wide strings (unsupported) */     OFTWideStringList = 7,
  /** Raw Binary data */                        OFTBinary = 8,
  /** Date */                                   OFTDate = 9,
  /** Time */                                   OFTTime = 10,
  /** Date and Time */                          OFTDateTime = 11
} OGRFieldType;

%rename (Justification) OGRJustification;
typedef enum 
{
    OJUndefined = 0,
    OJLeft = 1,
    OJRight = 2
} OGRJustification;
#endif

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

#ifndef SWIGCSHARP
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
%constant OFTDate = 9;
%constant OFTTime = 10;
%constant OFTDateTime = 11;

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
#else
typedef int OGRErr;

#define OGRERR_NONE                0
#define OGRERR_NOT_ENOUGH_DATA     1    /* not enough data to deserialize */
#define OGRERR_NOT_ENOUGH_MEMORY   2
#define OGRERR_UNSUPPORTED_GEOMETRY_TYPE 3
#define OGRERR_UNSUPPORTED_OPERATION 4
#define OGRERR_CORRUPT_DATA        5
#define OGRERR_FAILURE             6
#define OGRERR_UNSUPPORTED_SRS     7

#define wkb25DBit 0x80000000
#define ogrZMarker 0x21125711

#define OGRNullFID            -1
#define OGRUnsetMarker        -21121

#define OLCRandomRead          "RandomRead"
#define OLCSequentialWrite     "SequentialWrite"
#define OLCRandomWrite         "RandomWrite"
#define OLCFastSpatialFilter   "FastSpatialFilter"
#define OLCFastFeatureCount    "FastFeatureCount"
#define OLCFastGetExtent       "FastGetExtent"
#define OLCCreateField         "CreateField"
#define OLCTransactions        "Transactions"
#define OLCDeleteFeature       "DeleteFeature"
#define OLCFastSetNextByIndex  "FastSetNextByIndex"

#define ODsCCreateLayer        "CreateLayer"
#define ODsCDeleteLayer        "DeleteLayer"

#define ODrCCreateDataSource   "CreateDataSource"
#define ODrCDeleteDataSource   "DeleteDataSource"
#endif

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
/*                               OGREnvelope                            */
/************************************************************************/

#if defined(SWIGCSHARP)
%rename (Envelope) OGREnvelope;
typedef struct
{
    double      MinX;
    double      MaxX;
    double      MinY;
    double      MaxY;
} OGREnvelope;
#endif

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
  
#if defined(SWIGCSHARP)
  %feature( "kwargs" ) GetExtent;
  OGRErr GetExtent(OGREnvelope* extent, int force=1) {
    return OGR_L_GetExtent(self, extent, force);
  }
#else
  %feature( "kwargs" ) GetExtent;
  OGRErr GetExtent(double argout[4], int force=1) {
    return OGR_L_GetExtent(self, (OGREnvelope*)argout, force);
  }
#endif

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
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetFieldDefnRef");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            return (OGRFieldDefnShadow *) OGR_F_GetFieldDefnRef(self, i);
    }
    return NULL;
  }
  /* ------------------------------------------- */

  /* ---- GetFieldAsString --------------------- */

  const char* GetFieldAsString(int id) {
    return (const char *) OGR_F_GetFieldAsString(self, id);
  }

  const char* GetFieldAsString(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetFieldAsString");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            return (const char *) OGR_F_GetFieldAsString(self, i);
    }
    return NULL;
  }
  /* ------------------------------------------- */

  /* ---- GetFieldAsInteger -------------------- */

  int GetFieldAsInteger(int id) {
    return OGR_F_GetFieldAsInteger(self, id);
  }

  int GetFieldAsInteger(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetFieldAsInteger");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            return OGR_F_GetFieldAsInteger(self, i);
    }
    return 0;
  }
  /* ------------------------------------------- */  

  /* ---- GetFieldAsDouble --------------------- */

  double GetFieldAsDouble(int id) {
    return OGR_F_GetFieldAsDouble(self, id);
  }

  double GetFieldAsDouble(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetFieldAsDouble");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            return OGR_F_GetFieldAsDouble(self, i);
    }
    return 0;
  }
  /* ------------------------------------------- */  


  
  /* ---- IsFieldSet --------------------------- */
  bool IsFieldSet(int id) {
    return OGR_F_IsFieldSet(self, id);
  }

  bool IsFieldSet(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in IsFieldSet");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            return OGR_F_IsFieldSet(self, i);
    }
    return (bool)0;
  }
  /* ------------------------------------------- */  
      
  int GetFieldIndex(const char* name) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in GetFieldIndex");
    else
        return OGR_F_GetFieldIndex(self, name);
    return 0;
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
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in UnsetField");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            OGR_F_UnsetField(self, i);
    }
  }

  /* ---- SetField ----------------------------- */
  
  %apply ( tostring argin ) { (const char* value) };
  void SetField(int id, const char* value) {
    OGR_F_SetFieldString(self, id, value);
  }

  void SetField(const char* name, const char* value) {
    if (name == NULL)
        CPLError(CE_Failure, 1, "Undefined field name in SetField");
    else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1)
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
        else
            OGR_F_SetFieldString(self, i, value);
    }
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
  
  OGRFieldType GetFieldType(const char* name) {
    if (name == NULL) {
        CPLError(CE_Failure, 1, "Undefined field name in GetFieldType");
	return (OGRFieldType)0;
    } else {
        int i = OGR_F_GetFieldIndex(self, name);
        if (i == -1) {
            CPLError(CE_Failure, 1, "No such field: '%s'", name);
            return (OGRFieldType)0;
        } else
            return (OGRFieldType) OGR_Fld_GetType( 
                            OGR_F_GetFieldDefnRef( self,  i )
                                          );
    }
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
    if (name == NULL) {
        CPLError(CE_Failure, 1, "Undefined field name in GetFieldIndex");
	return 0;
    } else
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
    if (err != 0 ) {
       CPLError(CE_Failure, err, "%s", OGRErrMessages(err));
       return NULL;
    }
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
    if (err != 0 ) {
       CPLError(CE_Failure, err, "%s", OGRErrMessages(err));
       return NULL;
    }
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

#ifndef SWIGCSHARP
  %feature("kwargs") ExportToWkb;
  OGRErr ExportToWkb( int *nLen, char **pBuf, OGRwkbByteOrder byte_order=wkbXDR ) {
    *nLen = OGR_G_WkbSize( self );
    *pBuf = (char *) malloc( *nLen * sizeof(unsigned char) );
    return OGR_G_ExportToWkb(self, byte_order, (unsigned char*) *pBuf );
  }
#endif

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

#if defined(SWIGCSHARP)  
  void GetEnvelope(OGREnvelope *env) {
    OGR_G_GetEnvelope(self, env);
  }
#else
  void GetEnvelope(double argout[4]) {
    OGR_G_GetEnvelope(self, (OGREnvelope*)argout);
  }
#endif  

  %newobject Centroid;
  OGRGeometryShadow* Centroid() {
    OGRGeometryShadow *pt = new_OGRGeometryShadow( wkbPoint );
    OGR_G_Centroid( self, pt );
    return pt;
  }
  
  int WkbSize() {
    return OGR_G_WkbSize(self);
  }
  
  int GetCoordinateDimension() {
    return OGR_G_GetCoordinateDimension(self);
  }

  void SetCoordinateDimension(int dimension) {
    OGR_G_SetCoordinateDimension(self, dimension);
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

//************************************************************************
//
// Language specific extensions
//
//************************************************************************

#ifdef SWIGCSHARP
%include "ogr_csharp_extend.i"
#endif
