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

%inline %{
typedef char retStringAndCPLFree;
%}

#ifdef SWIGCSHARP
%include swig_csharp_extensions.i
#endif

#ifndef SWIGJAVA
%feature("compactdefaultargs");
#endif

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
#include "ogr_p.h"
#include "ogr_core.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

#ifdef DEBUG 
typedef struct OGRSpatialReferenceHS OSRSpatialReferenceShadow;
typedef struct OGRDriverHS OGRDriverShadow;
typedef struct OGRDataSourceHS OGRDataSourceShadow;
typedef struct OGRLayerHS OGRLayerShadow;
typedef struct OGRFeatureHS OGRFeatureShadow;
typedef struct OGRFeatureDefnHS OGRFeatureDefnShadow;
typedef struct OGRGeometryHS OGRGeometryShadow;
typedef struct OGRCoordinateTransformationHS OSRCoordinateTransformationShadow;
typedef struct OGRCoordinateTransformationHS OGRCoordinateTransformationShadow;
typedef struct OGRFieldDefnHS OGRFieldDefnShadow;
#else
typedef void OSRSpatialReferenceShadow;
typedef void OGRDriverShadow;
typedef void OGRDataSourceShadow;
typedef void OGRLayerShadow;
typedef void OGRFeatureShadow;
typedef void OGRFeatureDefnShadow;
typedef void OGRGeometryShadow;
typedef void OSRCoordinateTransformationShadow;
typedef void OGRFieldDefnShadow;
#endif
%}

#ifdef SWIGJAVA
%{
typedef void retGetPoints;
%}
#endif

#ifndef SWIGCSHARP
#ifdef SWIGJAVA
%javaconst(1);
#endif
/* Interface constant added for GDAL 1.7.0 */
%constant wkb25DBit = 0x80000000;

/* typo : deprecated */
%constant wkb25Bit = 0x80000000;

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

%constant NullFID = -1;

%constant ALTER_NAME_FLAG = 1;
%constant ALTER_TYPE_FLAG = 2;
%constant ALTER_WIDTH_PRECISION_FLAG = 4;
%constant ALTER_ALL_FLAG = 1 + 2 + 4;

%constant char *OLCRandomRead          = "RandomRead";
%constant char *OLCSequentialWrite     = "SequentialWrite";
%constant char *OLCRandomWrite         = "RandomWrite";
%constant char *OLCFastSpatialFilter   = "FastSpatialFilter";
%constant char *OLCFastFeatureCount    = "FastFeatureCount";
%constant char *OLCFastGetExtent       = "FastGetExtent";
%constant char *OLCCreateField         = "CreateField";
%constant char *OLCDeleteField         = "DeleteField";
%constant char *OLCReorderFields       = "ReorderFields";
%constant char *OLCAlterFieldDefn      = "AlterFieldDefn";
%constant char *OLCTransactions        = "Transactions";
%constant char *OLCDeleteFeature       = "DeleteFeature";
%constant char *OLCFastSetNextByIndex  = "FastSetNextByIndex";
%constant char *OLCStringsAsUTF8       = "StringsAsUTF8";
%constant char *OLCIgnoreFields        = "IgnoreFields";

%constant char *ODsCCreateLayer        = "CreateLayer";
%constant char *ODsCDeleteLayer        = "DeleteLayer";

%constant char *ODrCCreateDataSource   = "CreateDataSource";
%constant char *ODrCDeleteDataSource   = "DeleteDataSource";
#else
typedef int OGRErr;

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
#define OLCDeleteField         "DeleteField"
#define OLCReorderFields       "ReorderFields"
#define OLCAlterFieldDefn      "AlterFieldDefn"
#define OLCTransactions        "Transactions"
#define OLCDeleteFeature       "DeleteFeature"
#define OLCFastSetNextByIndex  "FastSetNextByIndex"
#define OLCStringsAsUTF8       "StringsAsUTF8"

#define ODsCCreateLayer        "CreateLayer"
#define ODsCDeleteLayer        "DeleteLayer"

#define ODrCCreateDataSource   "CreateDataSource"
#define ODrCDeleteDataSource   "DeleteDataSource"
#endif

#if defined(SWIGCSHARP) || defined(SWIGJAVA)

#define OGRERR_NONE                0
#define OGRERR_NOT_ENOUGH_DATA     1    /* not enough data to deserialize */
#define OGRERR_NOT_ENOUGH_MEMORY   2
#define OGRERR_UNSUPPORTED_GEOMETRY_TYPE 3
#define OGRERR_UNSUPPORTED_OPERATION 4
#define OGRERR_CORRUPT_DATA        5
#define OGRERR_FAILURE             6
#define OGRERR_UNSUPPORTED_SRS     7

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

%rename (Envelope3D) OGREnvelope3D;
typedef struct
{
    double      MinX;
    double      MaxX;
    double      MinY;
    double      MaxY;
    double      MinZ;
    double      MaxZ;
} OGREnvelope3D;
#endif

#ifndef GDAL_BINDINGS
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
#ifndef SWIGJAVA
%feature( "kwargs" ) CreateDataSource;
#endif
  OGRDataSourceShadow *CreateDataSource( const char *utf8_path, 
                                    char **options = 0 ) {
    OGRDataSourceShadow *ds = (OGRDataSourceShadow*) OGR_Dr_CreateDataSource( self, utf8_path, options);
    return ds;
  }
  
%newobject CopyDataSource;
#ifndef SWIGJAVA
%feature( "kwargs" ) CopyDataSource;
#endif
  OGRDataSourceShadow *CopyDataSource( OGRDataSourceShadow* copy_ds, 
                                  const char* utf8_path, 
                                  char **options = 0 ) {
    OGRDataSourceShadow *ds = (OGRDataSourceShadow*) OGR_Dr_CopyDataSource(self, copy_ds, utf8_path, options);
    return ds;
  }
  
%newobject Open;
#ifndef SWIGJAVA
%feature( "kwargs" ) Open;
#endif
  OGRDataSourceShadow *Open( const char* utf8_path, 
                        int update=0 ) {
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*) OGR_Dr_Open(self, utf8_path, update);
    return ds;
  }

#ifdef SWIGJAVA
  OGRErr DeleteDataSource( const char *utf8_path ) {
#else
  int DeleteDataSource( const char *utf8_path ) {
#endif
    return OGR_Dr_DeleteDataSource( self, utf8_path );
  }

%apply Pointer NONNULL {const char * cap};
  bool TestCapability (const char *cap) {
    return (OGR_Dr_TestCapability(self, cap) > 0);
  }
  
  const char * GetName() {
    return OGR_Dr_GetName( self );
  }

  /* Added in GDAL 1.8.0 */
  void Register() {
    OGRRegisterDriver( self );
  }

  /* Added in GDAL 1.8.0 */
  void Deregister() {
    OGRDeregisterDriver( self );
  }

} /* %extend */
}; /* class OGRDriverShadow */
#endif /* GDAL_BINDINGS */


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
    return (OGRDriverShadow *) OGR_DS_GetDriver( self );
  }

  const char * GetName() {
    return OGR_DS_GetName(self);
  }
  
  OGRErr DeleteLayer(int index){
    return OGR_DS_DeleteLayer(self, index);
  }

  OGRErr SyncToDisk() {
    return OGR_DS_SyncToDisk(self);
  }
  
  /* Note that datasources own their layers */
#ifndef SWIGJAVA
  %feature( "kwargs" ) CreateLayer;
#endif
  OGRLayerShadow *CreateLayer(const char* name, 
              OSRSpatialReferenceShadow* srs=NULL,
              OGRwkbGeometryType geom_type=wkbUnknown,
              char** options=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_CreateLayer( self,
								  name,
								  srs,
								  geom_type,
								  options);
    return layer;
  }

#ifndef SWIGJAVA
  %feature( "kwargs" ) CopyLayer;
#endif
%apply Pointer NONNULL {OGRLayerShadow *src_layer};
  OGRLayerShadow *CopyLayer(OGRLayerShadow *src_layer,
            const char* new_name,
            char** options=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_CopyLayer( self,
                                                      src_layer,
                                                      new_name,
                                                      options);
    return layer;
  }

#ifdef SWIGJAVA
  OGRLayerShadow *GetLayerByIndex( int index ) {
#else
  OGRLayerShadow *GetLayerByIndex( int index=0) {
#endif
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_GetLayer(self, index);
    return layer;
  }

  OGRLayerShadow *GetLayerByName( const char* layer_name) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_GetLayerByName(self, layer_name);
    return layer;
  }

  bool TestCapability(const char * cap) {
    return (OGR_DS_TestCapability(self, cap) > 0);
  }

#ifndef SWIGJAVA
  %feature( "kwargs" ) ExecuteSQL;
#endif
  %apply Pointer NONNULL {const char * statement};
  OGRLayerShadow *ExecuteSQL(const char* statement,
                        OGRGeometryShadow* spatialFilter=NULL,
                        const char* dialect="") {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_ExecuteSQL((OGRDataSourceShadow*)self,
                                                      statement,
                                                      spatialFilter,
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
    return OGR_L_GetName(self);
  }

  /* Added in OGR 1.8.0 */
  OGRwkbGeometryType GetGeomType() {
    return (OGRwkbGeometryType) OGR_L_GetGeomType(self);
  }
 
  const char * GetGeometryColumn() {
    return OGR_L_GetGeometryColumn(self);
  }
  
  const char * GetFIDColumn() {
    return OGR_L_GetFIDColumn(self);
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
  
%apply Pointer NONNULL {OGRFeatureShadow *feature};
  OGRErr SetFeature(OGRFeatureShadow *feature) {
    return OGR_L_SetFeature(self, feature);
  }

  OGRErr CreateFeature(OGRFeatureShadow *feature) {
    return OGR_L_CreateFeature(self, feature);
  }
%clear OGRFeatureShadow *feature;

  OGRErr DeleteFeature(long fid) {
    return OGR_L_DeleteFeature(self, fid);
  }
  
  OGRErr SyncToDisk() {
    return OGR_L_SyncToDisk(self);
  }
  
  OGRFeatureDefnShadow *GetLayerDefn() {
    return (OGRFeatureDefnShadow*) OGR_L_GetLayerDefn(self);
  }

#ifndef SWIGJAVA
  %feature( "kwargs" ) GetFeatureCount;  
#endif
  int GetFeatureCount(int force=1) {
    return OGR_L_GetFeatureCount(self, force);
  }
  
#if defined(SWIGCSHARP)
  %feature( "kwargs" ) GetExtent;
  OGRErr GetExtent(OGREnvelope* extent, int force=1) {
    return OGR_L_GetExtent(self, extent, force);
  }
#elif defined(SWIGPYTHON)
  %feature( "kwargs" ) GetExtent;
  void GetExtent(double argout[4], int* isvalid = NULL, int force = 1, int can_return_null = 0 ) {
    OGRErr eErr = OGR_L_GetExtent(self, (OGREnvelope*)argout, force);
    if (can_return_null)
        *isvalid = (eErr == OGRERR_NONE);
    else
        *isvalid = TRUE;
    return;
  }
#else
#ifndef SWIGJAVA
  %feature( "kwargs" ) GetExtent;
  OGRErr GetExtent(double argout[4], int force = 1) {
#else
  OGRErr GetExtent(double argout[4], int force) {
#endif
    return OGR_L_GetExtent(self, (OGREnvelope*)argout, force);
  }
#endif

  bool TestCapability(const char* cap) {
    return (OGR_L_TestCapability(self, cap) > 0);
  }
  
#ifndef SWIGJAVA
  %feature( "kwargs" ) CreateField;
#endif
%apply Pointer NONNULL {OGRFieldDefnShadow *field_def};
  OGRErr CreateField(OGRFieldDefnShadow* field_def, int approx_ok = 1) {
    return OGR_L_CreateField(self, field_def, approx_ok);
  }
%clear OGRFieldDefnShadow *field_def;

  OGRErr DeleteField(int iField)
  {
    return OGR_L_DeleteField(self, iField);
  }

  OGRErr ReorderField(int iOldFieldPos, int iNewFieldPos)
  {
    return OGR_L_ReorderField(self, iOldFieldPos, iNewFieldPos);
  }

  OGRErr ReorderFields(int nList, int *pList)
  {
    if (nList != OGR_FD_GetFieldCount(OGR_L_GetLayerDefn(self)))
    {
      CPLError(CE_Failure, CPLE_IllegalArg,
               "List should have %d elements",
               OGR_FD_GetFieldCount(OGR_L_GetLayerDefn(self)));
      return OGRERR_FAILURE;
    }
    return OGR_L_ReorderFields(self, pList);
  }

%apply Pointer NONNULL {OGRFieldDefnShadow *field_def};
  OGRErr AlterFieldDefn(int iField, OGRFieldDefnShadow* field_def, int nFlags)
  {
    return OGR_L_AlterFieldDefn(self, iField, field_def, nFlags);
  }
%clear OGRFieldDefnShadow *field_def;

  OGRErr StartTransaction() {
    return OGR_L_StartTransaction(self);
  }
  
  OGRErr CommitTransaction() {
    return OGR_L_CommitTransaction(self);
  }

  OGRErr RollbackTransaction() {
    return OGR_L_RollbackTransaction(self);
  }
  
  %newobject GetSpatialRef;
  OSRSpatialReferenceShadow *GetSpatialRef() {
    OGRSpatialReferenceH ref =  OGR_L_GetSpatialRef(self);
    if( ref )
        OSRReference(ref);
    return (OSRSpatialReferenceShadow*) ref;
  }
  
  GIntBig GetFeaturesRead() {
    return OGR_L_GetFeaturesRead(self);
  }

  OGRErr SetIgnoredFields( const char **options ) {
    return OGR_L_SetIgnoredFields( self, options );
  }

} /* %extend */


}; /* class OGRLayerShadow */

/************************************************************************/
/*                              OGRFeature                              */
/************************************************************************/

#ifdef SWIGJAVA
%{
typedef int* retIntArray;
typedef double* retDoubleArray;
%}
#endif

%rename (Feature) OGRFeatureShadow;
class OGRFeatureShadow {
  OGRFeatureShadow();
public:
%extend {

  ~OGRFeatureShadow() {
    OGR_F_Destroy(self);
  }

#ifndef SWIGJAVA
  %feature("kwargs") OGRFeatureShadow;
#endif
%apply Pointer NONNULL {OGRFeatureDefnShadow *feature_def};
  OGRFeatureShadow( OGRFeatureDefnShadow *feature_def ) {
      return (OGRFeatureShadow*) OGR_F_Create( feature_def );
  }

  OGRFeatureDefnShadow *GetDefnRef() {
    return (OGRFeatureDefnShadow*) OGR_F_GetDefnRef(self);
  }
  
  OGRErr SetGeometry(OGRGeometryShadow* geom) {
    return OGR_F_SetGeometry(self, geom);
  }

/* The feature takes over owernship of the geometry. */
/* Don't change the 'geom' name as Java bindings depends on it */
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
  
%apply Pointer NONNULL {OGRFeatureShadow *feature};
  bool Equal(OGRFeatureShadow *feature) {
    return (OGR_F_Equal(self, feature) > 0);
  }
%clear OGRFeatureShadow *feature;
  
  int GetFieldCount() {
    return OGR_F_GetFieldCount(self);
  }

  /* ---- GetFieldDefnRef --------------------- */
  OGRFieldDefnShadow *GetFieldDefnRef(int id) {
    return (OGRFieldDefnShadow *) OGR_F_GetFieldDefnRef(self, id);
  }

  OGRFieldDefnShadow *GetFieldDefnRef(const char* name) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return (OGRFieldDefnShadow *) OGR_F_GetFieldDefnRef(self, i);
      return NULL;
  }
  /* ------------------------------------------- */

  /* ---- GetFieldAsString --------------------- */

  const char* GetFieldAsString(int id) {
    return (const char *) OGR_F_GetFieldAsString(self, id);
  }

#ifndef SWIGPERL
  const char* GetFieldAsString(const char* name) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return (const char *) OGR_F_GetFieldAsString(self, i);
      return NULL;
  }
#endif
  /* ------------------------------------------- */

  /* ---- GetFieldAsInteger -------------------- */

  int GetFieldAsInteger(int id) {
    return OGR_F_GetFieldAsInteger(self, id);
  }

#ifndef SWIGPERL
  int GetFieldAsInteger(const char* name) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return OGR_F_GetFieldAsInteger(self, i);
      return 0;
  }
#endif
  /* ------------------------------------------- */  

  /* ---- GetFieldAsDouble --------------------- */

  double GetFieldAsDouble(int id) {
    return OGR_F_GetFieldAsDouble(self, id);
  }

#ifndef SWIGPERL
  double GetFieldAsDouble(const char* name) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return OGR_F_GetFieldAsDouble(self, i);
      return 0;
  }
#endif
  /* ------------------------------------------- */  

  %apply (int *OUTPUT) {(int *)};
  void GetFieldAsDateTime(int id, int *pnYear, int *pnMonth, int *pnDay,
			  int *pnHour, int *pnMinute, int *pnSecond,
			  int *pnTZFlag) {
      OGR_F_GetFieldAsDateTime(self, id, pnYear, pnMonth, pnDay,
			       pnHour, pnMinute, pnSecond,
			       pnTZFlag);
  }
  %clear (int *);

#if defined(SWIGJAVA)
  retIntArray GetFieldAsIntegerList(int id, int *nLen, const int **pList) {
      *pList = OGR_F_GetFieldAsIntegerList(self, id, nLen);
      return (retIntArray)*pList;
  }
#elif defined(SWIGCSHARP)
  %apply (int *intList) {const int *};
  %apply (int *hasval) {int *count};
  const int *GetFieldAsIntegerList(int id, int *count) {
      return OGR_F_GetFieldAsIntegerList(self, id, count);
  }
  %clear (const int *);
  %clear (int *count);
#else
  void GetFieldAsIntegerList(int id, int *nLen, const int **pList) {
      *pList = OGR_F_GetFieldAsIntegerList(self, id, nLen);
  }
#endif

#if defined(SWIGJAVA)
  retDoubleArray GetFieldAsDoubleList(int id, int *nLen, const double **pList) {
      *pList = OGR_F_GetFieldAsDoubleList(self, id, nLen);
      return (retDoubleArray)*pList;
  }
#elif defined(SWIGCSHARP)
  %apply (double *doubleList) {const double *};
  %apply (int *hasval) {int *count};
  const double *GetFieldAsDoubleList(int id, int *count) {
      return OGR_F_GetFieldAsDoubleList(self, id, count);
  }
  %clear (const double *);
  %clear (int *count);
#else
  void GetFieldAsDoubleList(int id, int *nLen, const double **pList) {
      *pList = OGR_F_GetFieldAsDoubleList(self, id, nLen);
  }
#endif

#if defined(SWIGJAVA)
  %apply (char** retAsStringArrayNoFree) {(char**)};
  char** GetFieldAsStringList(int id) {
      return OGR_F_GetFieldAsStringList(self, id);
  }
  %clear char**;
#elif defined(SWIGCSHARP) || defined(SWIGPYTHON)
  %apply (char **options) {char **};
  char **GetFieldAsStringList(int id) {
      return OGR_F_GetFieldAsStringList(self, id);
  }
  %clear (char **);
#else
  void GetFieldAsStringList(int id, char ***pList) {
      *pList = OGR_F_GetFieldAsStringList(self, id);
  }
#endif

  /* ---- IsFieldSet --------------------------- */
  bool IsFieldSet(int id) {
    return (OGR_F_IsFieldSet(self, id) > 0);
  }

  bool IsFieldSet(const char* name) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return (OGR_F_IsFieldSet(self, i) > 0);
      return false;
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

#ifndef SWIGPERL
  void UnsetField(const char* name) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_UnsetField(self, i);
  }
#endif

  /* ---- SetField ----------------------------- */
  
  %apply ( tostring argin ) { (const char* value) };
  void SetField(int id, const char* value) {
    OGR_F_SetFieldString(self, id, value);
  }

#ifndef SWIGPERL
  void SetField(const char* name, const char* value) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_SetFieldString(self, i, value);
  }
#endif
  %clear (const char* value );
  
  void SetField(int id, int value) {
    OGR_F_SetFieldInteger(self, id, value);
  }
  
#ifndef SWIGPERL
  void SetField(const char* name, int value) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_SetFieldInteger(self, i, value);
  }
#endif
  
  void SetField(int id, double value) {
    OGR_F_SetFieldDouble(self, id, value);
  }
  
#ifndef SWIGPERL
  void SetField(const char* name, double value) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_SetFieldDouble(self, i, value);
  }
#endif
  
  void SetField( int id, int year, int month, int day,
                             int hour, int minute, int second, 
                             int tzflag ) {
    OGR_F_SetFieldDateTime(self, id, year, month, day,
                             hour, minute, second, 
                             tzflag);
  }

#ifndef SWIGPERL  
  void SetField(const char* name, int year, int month, int day,
                             int hour, int minute, int second, 
                             int tzflag ) {
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_SetFieldDateTime(self, i, year, month, day,
				 hour, minute, second, 
				 tzflag);
  }
#endif

  void SetFieldIntegerList(int id, int nList, int *pList) {
      OGR_F_SetFieldIntegerList(self, id, nList, pList);
  }

  void SetFieldDoubleList(int id, int nList, double *pList) {
      OGR_F_SetFieldDoubleList(self, id, nList, pList);
  }

%apply (char **options) {char **pList};
  void SetFieldStringList(int id, char **pList) {
      OGR_F_SetFieldStringList(self, id, pList);
  }
%clear char**pList;

  /* ------------------------------------------- */  
  
#ifndef SWIGJAVA
  %feature("kwargs") SetFrom;
#endif
%apply Pointer NONNULL {OGRFeatureShadow *other};
  OGRErr SetFrom(OGRFeatureShadow *other, int forgiving=1) {
    return OGR_F_SetFrom(self, other, forgiving);
  }
%clear OGRFeatureShadow *other;

%apply Pointer NONNULL {OGRFeatureShadow *other};
  OGRErr SetFromWithMap(OGRFeatureShadow *other, int forgiving, int nList, int *pList) {
    if (nList != OGR_F_GetFieldCount(other))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The size of map doesn't match with the field count of the source feature");
        return OGRERR_FAILURE;
    }
    return OGR_F_SetFromWithMap(self, other, forgiving, pList);
  }
%clear OGRFeatureShadow *other;

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
      int i = OGR_F_GetFieldIndex(self, name);
      if (i == -1) {
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
	  return (OGRFieldType)0;
      } else
	  return (OGRFieldType) OGR_Fld_GetType( 
	      OGR_F_GetFieldDefnRef( self,  i )
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
    /*OGR_FD_Destroy(self);*/
    OGR_FD_Release( OGRFeatureDefnH(self) );
  }

#ifndef SWIGJAVA
  %feature("kwargs") OGRFeatureDefnShadow;
#endif
  OGRFeatureDefnShadow(const char* name_null_ok=NULL) {
    OGRFeatureDefnH h = OGR_FD_Create(name_null_ok);
    OGR_FD_Reference(h);
    return (OGRFeatureDefnShadow* )h;
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
  
%apply Pointer NONNULL {OGRFieldDefnShadow* defn};
  void AddFieldDefn(OGRFieldDefnShadow* defn) {
    OGR_FD_AddFieldDefn(self, defn);
  }
%clear OGRFieldDefnShadow* defn;
  
  OGRwkbGeometryType GetGeomType() {
    return (OGRwkbGeometryType) OGR_FD_GetGeomType(self);
  }
  
  void SetGeomType(OGRwkbGeometryType geom_type) {
    OGR_FD_SetGeomType(self, geom_type);
  }
  
  int GetReferenceCount(){
    return OGR_FD_GetReferenceCount(self);
  }

  int IsGeometryIgnored() {
    return OGR_FD_IsGeometryIgnored(self);
  }
  
  void SetGeometryIgnored( int bIgnored ) {
    return OGR_FD_SetGeometryIgnored(self,bIgnored);
  }
  
  int IsStyleIgnored() {
    return OGR_FD_IsStyleIgnored(self);
  }
  
  void SetStyleIgnored( int bIgnored ) {
    return OGR_FD_SetStyleIgnored(self,bIgnored);
  }
  
} /* %extend */


}; /* class OGRFeatureDefnShadow */

/************************************************************************/
/*                             OGRFieldDefn                             */
/************************************************************************/

%rename (FieldDefn) OGRFieldDefnShadow;

%{
    static int ValidateOGRFieldType(OGRFieldType field_type)
    {
        switch(field_type)
        {
            case OFTInteger:
            case OFTIntegerList:
            case OFTReal:
            case OFTRealList:
            case OFTString:
            case OFTStringList:
            case OFTBinary:
            case OFTDate:
            case OFTTime:
            case OFTDateTime:
                return TRUE;
            default:
                CPLError(CE_Failure, CPLE_IllegalArg, "Illegal field type value");
                return FALSE;
        }
    }
%}


class OGRFieldDefnShadow {
  OGRFieldDefnShadow();
public:
%extend {

  ~OGRFieldDefnShadow() {
    OGR_Fld_Destroy(self);
  }

#ifndef SWIGJAVA
  %feature("kwargs") OGRFieldDefnShadow;
#endif
  OGRFieldDefnShadow( const char* name_null_ok="unnamed", 
                      OGRFieldType field_type=OFTString) {
    if (ValidateOGRFieldType(field_type))
        return (OGRFieldDefnShadow*) OGR_Fld_Create(name_null_ok, field_type);
    else
        return NULL;
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
    if (ValidateOGRFieldType(type))
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

  /* Interface method added for GDAL 1.7.0 */
  const char * GetTypeName()
  {
      return OGR_GetFieldTypeName(OGR_Fld_GetType(self));
  }

  /* Should be static */
  const char * GetFieldTypeName(OGRFieldType type) {
    return OGR_GetFieldTypeName(type);
  }

  int IsIgnored() {
    return OGR_Fld_IsIgnored( self );
  }

  void SetIgnored(int bIgnored ) {
    return OGR_Fld_SetIgnored( self, bIgnored );
  }

} /* %extend */


}; /* class OGRFieldDefnShadow */


/* -------------------------------------------------------------------- */
/*      Geometry factory methods.                                       */
/* -------------------------------------------------------------------- */

#ifndef SWIGJAVA
%feature( "kwargs" ) CreateGeometryFromWkb;
%newobject CreateGeometryFromWkb;
#ifndef SWIGCSHARP
%apply (int nLen, char *pBuf ) { (int len, char *bin_string)};
#else
%apply (void *buffer_ptr) {char *bin_string};
#endif
%inline %{
  OGRGeometryShadow* CreateGeometryFromWkb( int len, char *bin_string, 
                                            OSRSpatialReferenceShadow *reference=NULL ) {
    OGRGeometryH geom = NULL;
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
#endif
#ifndef SWIGCSHARP
%clear (int len, char *bin_string);
#else
%clear (char *bin_string);
#endif

#ifdef SWIGJAVA
%newobject CreateGeometryFromWkb;
%inline {
OGRGeometryShadow* CreateGeometryFromWkb(int nLen, unsigned char *pBuf, 
                                            OSRSpatialReferenceShadow *reference=NULL ) {
    OGRGeometryH geom = NULL;
    OGRErr err = OGR_G_CreateFromWkb((unsigned char*) pBuf, reference, &geom, nLen);
    if (err != 0 ) {
       CPLError(CE_Failure, err, "%s", OGRErrMessages(err));
       return NULL;
    }
    return (OGRGeometryShadow*) geom;
  }
}
#endif

#ifndef SWIGJAVA
%feature( "kwargs" ) CreateGeometryFromWkt;
#endif
%apply (char **ignorechange) { (char **) };
%newobject CreateGeometryFromWkt;
%inline %{
  OGRGeometryShadow* CreateGeometryFromWkt( char **val, 
                                      OSRSpatialReferenceShadow *reference=NULL ) {
    OGRGeometryH geom = NULL;
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

%newobject CreateGeometryFromJson;
%inline %{
  OGRGeometryShadow *CreateGeometryFromJson( const char * input_string ) {
    OGRGeometryShadow* geom = (OGRGeometryShadow*)OGR_G_CreateGeometryFromJson(input_string);
    return geom;
  }
 
%}

%newobject BuildPolygonFromEdges;
#ifndef SWIGJAVA
%feature( "kwargs" ) BuildPolygonFromEdges;
#endif
%inline %{
  OGRGeometryShadow* BuildPolygonFromEdges( OGRGeometryShadow*  hLineCollection,  
                                            int bBestEffort = 0, 
                                            int bAutoClose = 0, 
                                            double dfTolerance=0) {
  
  OGRGeometryH hPolygon = NULL;
  
  OGRErr eErr;

  hPolygon = OGRBuildPolygonFromEdges( hLineCollection, bBestEffort, 
                                       bAutoClose, dfTolerance, &eErr );

  if (eErr != OGRERR_NONE ) {
    CPLError(CE_Failure, eErr, "%s", OGRErrMessages(eErr));
    return NULL;
  }

  return (OGRGeometryShadow* )hPolygon;
  }
%}

%newobject ApproximateArcAngles;
#ifndef SWIGJAVA
%feature( "kwargs" ) ApproximateArcAngles;
#endif
%inline %{
  OGRGeometryShadow* ApproximateArcAngles( 
        double dfCenterX, double dfCenterY, double dfZ,
  	double dfPrimaryRadius, double dfSecondaryAxis, double dfRotation, 
        double dfStartAngle, double dfEndAngle,
        double dfMaxAngleStepSizeDegrees ) {
  
  return (OGRGeometryShadow* )OGR_G_ApproximateArcAngles( 
             dfCenterX, dfCenterY, dfZ, 
             dfPrimaryRadius, dfSecondaryAxis, dfRotation,
             dfStartAngle, dfEndAngle, dfMaxAngleStepSizeDegrees );
  }
%}

%newobject ForceToPolygon;
/* Contrary to the C/C++ method, the passed geometry is preserved */
/* This avoids dirty trick for Java */
%inline %{
OGRGeometryShadow* ForceToPolygon( OGRGeometryShadow *geom_in ) {
 if (geom_in == NULL)
     return NULL;
 return (OGRGeometryShadow* )OGR_G_ForceToPolygon( OGR_G_Clone(geom_in) );
}
%}

%newobject ForceToMultiPolygon;
/* Contrary to the C/C++ method, the passed geometry is preserved */
/* This avoids dirty trick for Java */
%inline %{
OGRGeometryShadow* ForceToMultiPolygon( OGRGeometryShadow *geom_in ) {
 if (geom_in == NULL)
     return NULL;
 return (OGRGeometryShadow* )OGR_G_ForceToMultiPolygon( OGR_G_Clone(geom_in) );
}
%}

%newobject ForceToMultiPoint;
/* Contrary to the C/C++ method, the passed geometry is preserved */
/* This avoids dirty trick for Java */
%inline %{
OGRGeometryShadow* ForceToMultiPoint( OGRGeometryShadow *geom_in ) {
 if (geom_in == NULL)
     return NULL;
 return (OGRGeometryShadow* )OGR_G_ForceToMultiPoint( OGR_G_Clone(geom_in) );
}
%}

%newobject ForceToMultiLineString;
/* Contrary to the C/C++ method, the passed geometry is preserved */
/* This avoids dirty trick for Java */
%inline %{
OGRGeometryShadow* ForceToMultiLineString( OGRGeometryShadow *geom_in ) {
 if (geom_in == NULL)
     return NULL;
 return (OGRGeometryShadow* )OGR_G_ForceToMultiLineString( OGR_G_Clone(geom_in) );
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
  
#ifndef SWIGJAVA
#ifdef SWIGCSHARP
%apply (void *buffer_ptr) {char *wkb_buf};
#else
%apply (int nLen, char *pBuf) {(int wkb, char *wkb_buf)};
#endif
  %feature("kwargs") OGRGeometryShadow;
  OGRGeometryShadow( OGRwkbGeometryType type = wkbUnknown, char *wkt = 0, int wkb = 0, char *wkb_buf = 0, char *gml = 0 ) {
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
    else {
        CPLError(CE_Failure, 1, "Empty geometries cannot be constructed");
        return NULL;}

  }  
#ifdef SWIGCSHARP
%clear (char *wkb_buf);
#else
%clear (int wkb, char *wkb_buf);
#endif
#endif

  OGRErr ExportToWkt( char** argout ) {
    return OGR_G_ExportToWkt(self, argout);
  }

#ifndef SWIGCSHARP
#ifdef SWIGJAVA
%apply (GByte* outBytes) {GByte*};
  GByte* ExportToWkb( int *nLen, char **pBuf, OGRwkbByteOrder byte_order=wkbXDR ) {
    *nLen = OGR_G_WkbSize( self );
    *pBuf = (char *) malloc( *nLen * sizeof(unsigned char) );
    OGR_G_ExportToWkb(self, byte_order, (unsigned char*) *pBuf );
    return (GByte*)*pBuf;
  }
%clear GByte*;
#else
  %feature("kwargs") ExportToWkb;
  OGRErr ExportToWkb( int *nLen, char **pBuf, OGRwkbByteOrder byte_order=wkbXDR ) {
    *nLen = OGR_G_WkbSize( self );
    *pBuf = (char *) malloc( *nLen * sizeof(unsigned char) );
    return OGR_G_ExportToWkb(self, byte_order, (unsigned char*) *pBuf );
  }
#endif
#endif

#if defined(SWIGCSHARP)
  retStringAndCPLFree* ExportToGML() {
    return (retStringAndCPLFree*) OGR_G_ExportToGMLEx(self, NULL);
  }

  retStringAndCPLFree* ExportToGML(char** options) {
    return (retStringAndCPLFree*) OGR_G_ExportToGMLEx(self, options);
  }
#elif defined(SWIGJAVA) || defined(SWIGPYTHON) || defined(SWIGPERL)
#ifndef SWIGJAVA
  %feature("kwargs") ExportToGML;
#endif
  retStringAndCPLFree* ExportToGML(char** options=0) {
    return (retStringAndCPLFree*) OGR_G_ExportToGMLEx(self, options);
  }
#else
  /* FIXME : wrong typemap. The string should be freed */
  const char * ExportToGML() {
    return (const char *) OGR_G_ExportToGML(self);
  }
#endif

#if defined(SWIGJAVA) || defined(SWIGPYTHON) || defined(SWIGCSHARP) || defined(SWIGPERL)
  retStringAndCPLFree* ExportToKML(const char* altitude_mode=NULL) {
    return (retStringAndCPLFree *) OGR_G_ExportToKML(self, altitude_mode);
  }
#else
  /* FIXME : wrong typemap. The string should be freed */
  const char * ExportToKML(const char* altitude_mode=NULL) {
    return (const char *) OGR_G_ExportToKML(self, altitude_mode);
  }
#endif

#if defined(SWIGJAVA) || defined(SWIGPYTHON) || defined(SWIGCSHARP) || defined(SWIGPERL)
#ifndef SWIGJAVA
  %feature("kwargs") ExportToJson;
#endif
  retStringAndCPLFree* ExportToJson(char** options=0) {
    return (retStringAndCPLFree *) OGR_G_ExportToJsonEx(self, options);
  }
#else
  /* FIXME : wrong typemap. The string should be freed */
  const char * ExportToJson() {
    return (const char *) OGR_G_ExportToJson(self);
  }
#endif

#ifndef SWIGJAVA
  %feature("kwargs") AddPoint;
#endif
  void AddPoint(double x, double y, double z = 0) {
    OGR_G_AddPoint( self, x, y, z );
  }
  
  void AddPoint_2D(double x, double y) {
    OGR_G_AddPoint_2D( self, x, y );
  }

/* The geometry now owns an inner geometry */
/* Don't change the 'other_disown' name as Java bindings depends on it */
%apply SWIGTYPE *DISOWN {OGRGeometryShadow* other_disown};
%apply Pointer NONNULL {OGRGeometryShadow* other_disown};
  OGRErr AddGeometryDirectly( OGRGeometryShadow* other_disown ) {
    return OGR_G_AddGeometryDirectly( self, other_disown );
  }
%clear OGRGeometryShadow* other_disown;

%apply Pointer NONNULL {OGRGeometryShadow* other};
  OGRErr AddGeometry( OGRGeometryShadow* other ) {
    return OGR_G_AddGeometry( self, other );
  }
%clear OGRGeometryShadow* other;

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

  double Length () {
    return OGR_G_Length(self);
  }
  
  double Area() {
    return OGR_G_Area(self);
  }

  /* old, non-standard API */
  double GetArea() {
    return OGR_G_Area(self);
  }
  
  int GetPointCount() {
    return OGR_G_GetPointCount(self);
  }

  /* since GDAL 1.9.0 */
#if defined(SWIGPYTHON) || defined(SWIGJAVA)
#ifdef SWIGJAVA
  retGetPoints* GetPoints(int* pnCount, double** ppadfXY, double** ppadfZ, int nCoordDimension = 0)
  {
    int nPoints = OGR_G_GetPointCount(self);
    *pnCount = nPoints;
    if (nPoints == 0)
    {
        *ppadfXY = NULL;
        *ppadfZ = NULL;
    }
    *ppadfXY = (double*)VSIMalloc(2 * sizeof(double) * nPoints);
    if (*ppadfXY == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate resulting array");
        *pnCount = 0;
        return NULL;
    }
    if (nCoordDimension <= 0)
        nCoordDimension = OGR_G_GetCoordinateDimension(self);
    *ppadfZ = (nCoordDimension == 3) ? (double*)VSIMalloc(sizeof(double) * nPoints) : NULL;
    OGR_G_GetPoints(self,
                    *ppadfXY, 2 * sizeof(double),
                    (*ppadfXY) + 1, 2 * sizeof(double),
                    *ppadfZ, sizeof(double));
    return NULL;
  }
#else
  %feature("kwargs") GetPoints;
  void GetPoints(int* pnCount, double** ppadfXY, double** ppadfZ, int nCoordDimension = 0)
  {
    int nPoints = OGR_G_GetPointCount(self);
    *pnCount = nPoints;
    if (nPoints == 0)
    {
        *ppadfXY = NULL;
        *ppadfZ = NULL;
    }
    *ppadfXY = (double*)VSIMalloc(2 * sizeof(double) * nPoints);
    if (*ppadfXY == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate resulting array");
        *pnCount = 0;
        return;
    }
    if (nCoordDimension <= 0)
        nCoordDimension = OGR_G_GetCoordinateDimension(self);
    *ppadfZ = (nCoordDimension == 3) ? (double*)VSIMalloc(sizeof(double) * nPoints) : NULL;
    OGR_G_GetPoints(self,
                    *ppadfXY, 2 * sizeof(double),
                    (*ppadfXY) + 1, 2 * sizeof(double),
                    *ppadfZ, sizeof(double));
  }
#endif
#endif

#ifndef SWIGJAVA
  %feature("kwargs") GetX;  
#endif
  double GetX(int point=0) {
    return OGR_G_GetX(self, point);
  }

#ifndef SWIGJAVA
  %feature("kwargs") GetY;  
#endif
  double GetY(int point=0) {
    return OGR_G_GetY(self, point);
  }

#ifndef SWIGJAVA
  %feature("kwargs") GetZ;  
#endif
  double GetZ(int point=0) {
    return OGR_G_GetZ(self, point);
  } 

#ifdef SWIGJAVA
  void GetPoint(int iPoint, double argout[3]) {
#else
  void GetPoint(int iPoint = 0, double argout[3] = NULL) {
#endif
    OGR_G_GetPoint( self, iPoint, argout+0, argout+1, argout+2 );
  }

#ifdef SWIGJAVA
  void GetPoint_2D(int iPoint, double argout[2]) {
#else
  void GetPoint_2D(int iPoint = 0, double argout[2] = NULL) {
#endif
    OGR_G_GetPoint( self, iPoint, argout+0, argout+1, NULL );
  }

  int GetGeometryCount() {
    return OGR_G_GetGeometryCount(self);
  }

#ifndef SWIGJAVA
  %feature("kwargs") SetPoint;    
#endif
  void SetPoint(int point, double x, double y, double z=0) {
    OGR_G_SetPoint(self, point, x, y, z);
  }

#ifndef SWIGJAVA
  %feature("kwargs") SetPoint_2D;
#endif
  void SetPoint_2D(int point, double x, double y) {
    OGR_G_SetPoint_2D(self, point, x, y);
  }
  
  /* Geometries own their internal geometries */
  OGRGeometryShadow* GetGeometryRef(int geom) {
    return (OGRGeometryShadow*) OGR_G_GetGeometryRef(self, geom);
  }

  %newobject Simplify;
  OGRGeometryShadow* Simplify(double tolerance) {
    return (OGRGeometryShadow*) OGR_G_Simplify(self, tolerance);
  }

  %newobject Boundary;
  OGRGeometryShadow* Boundary() {
    return (OGRGeometryShadow*) OGR_G_Boundary(self);
  }  

  %newobject GetBoundary;
  OGRGeometryShadow* GetBoundary() {
    return (OGRGeometryShadow*) OGR_G_Boundary(self);
  }  

  %newobject ConvexHull;
  OGRGeometryShadow* ConvexHull() {
    return (OGRGeometryShadow*) OGR_G_ConvexHull(self);
  } 

  %newobject Buffer;
#ifndef SWIGJAVA
  %feature("kwargs") Buffer; 
#endif
  OGRGeometryShadow* Buffer( double distance, int quadsecs=30 ) {
    return (OGRGeometryShadow*) OGR_G_Buffer( self, distance, quadsecs );
  }

%apply Pointer NONNULL {OGRGeometryShadow* other};
  %newobject Intersection;
  OGRGeometryShadow* Intersection( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_Intersection( self, other );
  }  
  
  %newobject Union;
  OGRGeometryShadow* Union( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_Union( self, other );
  }
  
  %newobject UnionCascaded;
  OGRGeometryShadow* UnionCascaded() {
    return (OGRGeometryShadow*) OGR_G_UnionCascaded( self );
  }  
  
  %newobject Difference;
  OGRGeometryShadow* Difference( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_Difference( self, other );
  }  

  %newobject SymDifference;
  OGRGeometryShadow* SymDifference( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_SymDifference( self, other );
  } 

  /* old, non-standard API */
  %newobject SymmetricDifference;
  OGRGeometryShadow* SymmetricDifference( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_SymDifference( self, other );
  } 
  
  double Distance( OGRGeometryShadow* other) {
    return OGR_G_Distance(self, other);
  }
%clear OGRGeometryShadow* other;
  
  void Empty () {
    OGR_G_Empty(self);
  }

  bool IsEmpty () {
    return (OGR_G_IsEmpty(self) > 0);
  }
  
  bool IsValid () {
    return (OGR_G_IsValid(self) > 0);
  }  
  
  bool IsSimple () {
    return (OGR_G_IsSimple(self) > 0);
  }  
  
  bool IsRing () {
    return (OGR_G_IsRing(self) > 0);
  }  
  
%apply Pointer NONNULL {OGRGeometryShadow* other};

  bool Intersects (OGRGeometryShadow* other) {
    return (OGR_G_Intersects(self, other) > 0);
  }

  /* old, non-standard API */
  bool Intersect (OGRGeometryShadow* other) {
    return (OGR_G_Intersects(self, other) > 0);
  }

  bool Equals (OGRGeometryShadow* other) {
    return (OGR_G_Equals(self, other) > 0);
  }
  
  /* old, non-standard API */
  bool Equal (OGRGeometryShadow* other) {
    return (OGR_G_Equals(self, other) > 0);
  }
  
  bool Disjoint(OGRGeometryShadow* other) {
    return (OGR_G_Disjoint(self, other) > 0);
  }

  bool Touches (OGRGeometryShadow* other) {
    return (OGR_G_Touches(self, other) > 0);
  }

  bool Crosses (OGRGeometryShadow* other) {
    return (OGR_G_Crosses(self, other) > 0);
  }

  bool Within (OGRGeometryShadow* other) {
    return (OGR_G_Within(self, other) > 0);
  }

  bool Contains (OGRGeometryShadow* other) {
    return (OGR_G_Contains(self, other) > 0);
  }
  
  bool Overlaps (OGRGeometryShadow* other) {
    return (OGR_G_Overlaps(self, other) > 0);
  }
%clear OGRGeometryShadow* other;

%apply Pointer NONNULL {OSRSpatialReferenceShadow* reference};
  OGRErr TransformTo(OSRSpatialReferenceShadow* reference) {
    return OGR_G_TransformTo(self, reference);
  }
%clear OSRSpatialReferenceShadow* reference;
  
%apply Pointer NONNULL {OSRCoordinateTransformationShadow* trans};
  OGRErr Transform(OSRCoordinateTransformationShadow* trans) {
    return OGR_G_Transform(self, trans);
  }
%clear OSRCoordinateTransformationShadow* trans;
  
  %newobject GetSpatialReference;
  OSRSpatialReferenceShadow* GetSpatialReference() {
    OGRSpatialReferenceH ref =  OGR_G_GetSpatialReference(self);
    if( ref )
        OSRReference(ref);
    return (OSRSpatialReferenceShadow*) ref;
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

  void Segmentize(double dfMaxLength) {
    OGR_G_Segmentize(self, dfMaxLength);
  }


#if defined(SWIGCSHARP)  
  void GetEnvelope(OGREnvelope *env) {
    OGR_G_GetEnvelope(self, env);
  }

  void GetEnvelope3D(OGREnvelope3D *env) {
    OGR_G_GetEnvelope3D(self, env);
  }
#else
  void GetEnvelope(double argout[4]) {
    OGR_G_GetEnvelope(self, (OGREnvelope*)argout);
  }

  void GetEnvelope3D(double argout[6]) {
    OGR_G_GetEnvelope3D(self, (OGREnvelope3D*)argout);
  }
#endif  

#ifndef SWIGJAVA
  %newobject Centroid;
  OGRGeometryShadow* Centroid() {
    OGRGeometryShadow *pt = (OGRGeometryShadow*) OGR_G_CreateGeometry( wkbPoint );
    OGR_G_Centroid( self, pt );
    return pt;
  }
#endif
  
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

#ifndef GDAL_BINDINGS
int OGRGetDriverCount();
#endif

int OGRGetOpenDSCount();

OGRErr OGRSetGenerate_DB2_V72_BYTE_ORDER(int bGenerate_DB2_V72_BYTE_ORDER);

void OGRRegisterAll();

%rename (GeometryTypeToName) OGRGeometryTypeToName;
const char *OGRGeometryTypeToName( OGRwkbGeometryType eType );

%rename (GetFieldTypeName) OGR_GetFieldTypeName;
const char * OGR_GetFieldTypeName(OGRFieldType type);

%inline %{
  OGRDataSourceShadow* GetOpenDS(int ds_number) {
    OGRDataSourceShadow* layer = (OGRDataSourceShadow*) OGRGetOpenDS(ds_number);
    return layer;
  }
%}

%newobject Open;
#ifndef SWIGJAVA
%feature( "kwargs" ) Open;
#endif
%inline %{
  OGRDataSourceShadow* Open( const char *utf8_path, int update =0 ) {
    CPLErrorReset();
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpen(utf8_path,update,NULL);
    if( CPLGetLastErrorType() == CE_Failure && ds != NULL )
    {
        CPLDebug( "SWIG", 
		  "OGROpen() succeeded, but an error is posted, so we destroy"
		  " the datasource and fail at swig level." );
        OGRReleaseDataSource(ds);
        ds = NULL;
    }
	
    return ds;
  }
%}

%newobject OpenShared;
#ifndef SWIGJAVA
%feature( "kwargs" ) OpenShared;
#endif
%inline %{
  OGRDataSourceShadow* OpenShared( const char *utf8_path, int update =0 ) {
    CPLErrorReset();
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpenShared(utf8_path,update,NULL);
    if( CPLGetLastErrorType() == CE_Failure && ds != NULL )
    {
        OGRReleaseDataSource(ds);
        ds = NULL;
    }
	
    return ds;
  }
%}

#ifndef GDAL_BINDINGS
%inline %{
OGRDriverShadow* GetDriverByName( char const *name ) {
  return (OGRDriverShadow*) OGRGetDriverByName( name );
}

OGRDriverShadow* GetDriver(int driver_number) {
  return (OGRDriverShadow*) OGRGetDriver(driver_number);
}
%}
#endif

#if defined(SWIGPYTHON) || defined(SWIGJAVA)
/* FIXME: other bindings should also use those typemaps to avoid memory leaks */
%apply (char **options) {char ** papszArgv};
%apply (char **CSL) {(char **)};
#else
%apply (char **options) {char **};
#endif

/* Interface method added for GDAL 1.7.0 */
#ifdef SWIGJAVA
%inline %{
  char **GeneralCmdLineProcessor( char **papszArgv, int nOptions = 0 ) {
    int nResArgCount;
    
    /* We must add a 'dummy' element in front of the real argument list */
    /* as Java doesn't include the binary name as the first */
    /* argument, as C does... */
    char** papszArgvModBefore = CSLInsertString(CSLDuplicate(papszArgv), 0, "dummy");
    char** papszArgvModAfter = papszArgvModBefore;

    nResArgCount = 
      OGRGeneralCmdLineProcessor( CSLCount(papszArgvModBefore), &papszArgvModAfter, nOptions ); 

    CSLDestroy(papszArgvModBefore);

    if( nResArgCount <= 0 )
    {
        return NULL;
    }
    else
    {
        /* Now, remove the first dummy element */
        char** papszRet = CSLDuplicate(papszArgvModAfter + 1);
        CSLDestroy(papszArgvModAfter);
        return papszRet;
    }
  }
%}
#else
%inline %{
  char **GeneralCmdLineProcessor( char **papszArgv, int nOptions = 0 ) {
    int nResArgCount;

    nResArgCount = 
      OGRGeneralCmdLineProcessor( CSLCount(papszArgv), &papszArgv, nOptions ); 

    if( nResArgCount <= 0 )
        return NULL;
    else
        return papszArgv;
  }
%}
#endif
%clear char **;


#ifdef SWIGJAVA
class FeatureNative {
  FeatureNative();
  ~FeatureNative();
};

class GeometryNative {
  GeometryNative();
  ~GeometryNative();
};
#endif

//************************************************************************
//
// Language specific extensions
//
//************************************************************************

#ifdef SWIGCSHARP
%include "ogr_csharp_extend.i"
#endif


#ifdef SWIGJAVA
%include "ogr_java_extend.i"
#endif
