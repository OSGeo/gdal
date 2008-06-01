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
#include "ogr_srs_api.h"

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
%feature( "kwargs" ) CreateDataSource;
  OGRDataSourceShadow *CreateDataSource( const char *name, 
                                    char **options = 0 ) {
    OGRDataSourceShadow *ds = (OGRDataSourceShadow*) OGR_Dr_CreateDataSource( (OGRSFDriverH) self, name, options);
    return ds;
  }
  
%newobject CopyDataSource;
%feature( "kwargs" ) CopyDataSource;
  OGRDataSourceShadow *CopyDataSource( OGRDataSourceShadow* copy_ds, 
                                  const char* name, 
                                  char **options = 0 ) {
    OGRDataSourceShadow *ds = (OGRDataSourceShadow*) OGR_Dr_CopyDataSource((OGRSFDriverH)self, (OGRDataSourceH) copy_ds, name, options);
    return ds;
  }
  
%newobject Open;
%feature( "kwargs" ) Open;
  OGRDataSourceShadow *Open( const char* name, 
                        int update=0 ) {
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*) OGR_Dr_Open((OGRSFDriverH)self, name, update);
    return ds;
  }

  int DeleteDataSource( const char *name ) {
    return OGR_Dr_DeleteDataSource( (OGRSFDriverH)self, name );
  }

  bool TestCapability (const char *cap) {
    return OGR_Dr_TestCapability((OGRSFDriverH)self, cap);
  }
  
  const char * GetName() {
    return OGR_Dr_GetName( (OGRSFDriverH) self );
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
    OGRReleaseDataSource((OGRDataSourceH)self);
  }

  int GetRefCount() {
    return OGR_DS_GetRefCount((OGRDataSourceH)self);
  }
  
  int GetSummaryRefCount() {
    return OGR_DS_GetSummaryRefCount((OGRDataSourceH)self);
  }
  
  int GetLayerCount() {
    return OGR_DS_GetLayerCount((OGRDataSourceH)self);
  }
  
  OGRDriverShadow * GetDriver() {
    return (OGRDriverShadow *) OGR_DS_GetDriver( (OGRDataSourceH)self );
  }

  const char * GetName() {
    return OGR_DS_GetName((OGRDataSourceH)self);
  }
  
  OGRErr DeleteLayer(int index){
    return OGR_DS_DeleteLayer((OGRDataSourceH)self, index);
  }

  /* Note that datasources own their layers */
  %feature( "kwargs" ) CreateLayer;
  OGRLayerShadow *CreateLayer(const char* name, 
              OSRSpatialReferenceShadow* srs=NULL,
              OGRwkbGeometryType geom_type=wkbUnknown,
              char** options=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) 
        OGR_DS_CreateLayer( (OGRDataSourceH)self, name,
                            (OGRSpatialReferenceH) srs,
		            geom_type, options);
    return layer;
  }

  %feature( "kwargs" ) CopyLayer;
  OGRLayerShadow *CopyLayer(OGRLayerShadow *src_layer,
            const char* new_name,
            char** options=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_CopyLayer( (OGRDataSourceH)self,
                                                      (OGRLayerH) src_layer,
                                                      new_name,
                                                      options);
    return layer;
  }
  
  %feature( "kwargs" ) GetLayerByIndex;
  OGRLayerShadow *GetLayerByIndex( int index=0) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_GetLayer((OGRDataSourceH)self, index);
    return layer;
  }

  OGRLayerShadow *GetLayerByName( const char* layer_name) {
    OGRLayerShadow* layer = (OGRLayerShadow*) OGR_DS_GetLayerByName((OGRDataSourceH)self, layer_name);
    return layer;
  }

  bool TestCapability(const char * cap) {
    return OGR_DS_TestCapability((OGRDataSourceH)self, cap);
  }


  %feature( "kwargs" ) ExecuteSQL;
  OGRLayerShadow *ExecuteSQL(const char* statement,
                        OGRGeometryShadow* spatialFilter=NULL,
                        const char* dialect="") {
    OGRLayerShadow* layer = (OGRLayerShadow*) 
        OGR_DS_ExecuteSQL((OGRDataSourceH)self, statement, 
                          (OGRGeometryH)spatialFilter, dialect);
    return layer;
  }
  
%apply SWIGTYPE *DISOWN {OGRLayerShadow *layer};
  void ReleaseResultSet(OGRLayerShadow *layer){
    OGR_DS_ReleaseResultSet((OGRDataSourceH)self, (OGRLayerH) layer);
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
    return OGR_L_GetRefCount((OGRLayerH)self);
  }
  
  void SetSpatialFilter(OGRGeometryShadow* filter) {
    OGR_L_SetSpatialFilter ((OGRLayerH)self, (OGRGeometryH)filter);
  }
  
  void SetSpatialFilterRect( double minx, double miny,
                             double maxx, double maxy) {
    OGR_L_SetSpatialFilterRect((OGRLayerH)self, minx, miny, maxx, maxy);                          
  }
  
  OGRGeometryShadow *GetSpatialFilter() {
    return (OGRGeometryShadow *) OGR_L_GetSpatialFilter((OGRLayerH)self);
  }

  OGRErr SetAttributeFilter(char* filter_string) {
    return OGR_L_SetAttributeFilter((OGRLayerH)self, filter_string);
  }
  
  void ResetReading() {
    OGR_L_ResetReading((OGRLayerH)self);
  }
  
  const char * GetName() {
    return OGR_FD_GetName(OGR_L_GetLayerDefn((OGRLayerH)self));
  }
 
  const char * GetGeometryColumn() {
    return OGR_L_GetGeometryColumn((OGRLayerH)self);
  }
  
  const char * GetFIDColumn() {
    return OGR_L_GetFIDColumn((OGRLayerH)self);
  }

%newobject GetFeature;
  OGRFeatureShadow *GetFeature(long fid) {
    return (OGRFeatureShadow*) OGR_L_GetFeature((OGRLayerH)self, fid);
  }
  
%newobject GetNextFeature;
  OGRFeatureShadow *GetNextFeature() {
    return (OGRFeatureShadow*) OGR_L_GetNextFeature((OGRLayerH)self);
  }
  
  OGRErr SetNextByIndex(long new_index) {
    return OGR_L_SetNextByIndex((OGRLayerH)self, new_index);
  }
  
  OGRErr SetFeature(OGRFeatureShadow *feature) {
    return OGR_L_SetFeature((OGRLayerH)self, (OGRFeatureH)feature);
  }
  

  OGRErr CreateFeature(OGRFeatureShadow *feature) {
    return OGR_L_CreateFeature((OGRLayerH)self, (OGRFeatureH) feature);
  }
  
  OGRErr DeleteFeature(long fid) {
    return OGR_L_DeleteFeature((OGRLayerH)self, fid);
  }
  
  OGRErr SyncToDisk() {
    return OGR_L_SyncToDisk((OGRLayerH)self);
  }
  
  OGRFeatureDefnShadow *GetLayerDefn() {
    return (OGRFeatureDefnShadow*) OGR_L_GetLayerDefn((OGRLayerH)self);
  }

  %feature( "kwargs" ) GetFeatureCount;  
  int GetFeatureCount(int force=1) {
    return OGR_L_GetFeatureCount((OGRLayerH)self, force);
  }
  
#if defined(SWIGCSHARP)
  %feature( "kwargs" ) GetExtent;
  OGRErr GetExtent(OGREnvelope* extent, int force=1) {
    return OGR_L_GetExtent((OGRLayerH)self, extent, force);
  }
#else
  %feature( "kwargs" ) GetExtent;
  OGRErr GetExtent(double argout[4], int force=1) {
    return OGR_L_GetExtent((OGRLayerH)self, (OGREnvelope*)argout, force);
  }
#endif

  bool TestCapability(const char* cap) {
    return OGR_L_TestCapability((OGRLayerH)self, cap);
  }
  
  %feature( "kwargs" ) CreateField;
  OGRErr CreateField(OGRFieldDefnShadow* field_def, int approx_ok = 1) {
    return OGR_L_CreateField((OGRLayerH)self, (OGRFieldDefnH) field_def, approx_ok);
  }
  
  OGRErr StartTransaction() {
    return OGR_L_StartTransaction((OGRLayerH)self);
  }
  
  OGRErr CommitTransaction() {
    return OGR_L_CommitTransaction((OGRLayerH)self);
  }

  OGRErr RollbackTransaction() {
    return OGR_L_RollbackTransaction((OGRLayerH)self);
  }
  
  %newobject GetSpatialRef;
  OSRSpatialReferenceShadow *GetSpatialRef() {
    OGRSpatialReferenceH ref =  OGR_L_GetSpatialRef((OGRLayerH)self);
    if( ref )
        OSRReference(ref);
    return (OSRSpatialReferenceShadow*) ref;
  }
  
  GIntBig GetFeaturesRead() {
    return OGR_L_GetFeaturesRead((OGRLayerH)self);
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
    OGR_F_Destroy((OGRFeatureH) self);
  }

  %feature("kwargs") OGRFeatureShadow;
  OGRFeatureShadow( OGRFeatureDefnShadow *feature_def = 0 ) {
      return (OGRFeatureShadow*) OGR_F_Create( (OGRFeatureDefnH)feature_def );
  }

  OGRFeatureDefnShadow *GetDefnRef() {
    return (OGRFeatureDefnShadow*) OGR_F_GetDefnRef((OGRFeatureH)self);
  }
  
  OGRErr SetGeometry(OGRGeometryShadow* geom) {
    return OGR_F_SetGeometry((OGRFeatureH)self, (OGRGeometryH)geom);
  }

/* The feature takes over owernship of the geometry. */
%apply SWIGTYPE *DISOWN {OGRGeometryShadow *geom};
  OGRErr SetGeometryDirectly(OGRGeometryShadow* geom) {
    return OGR_F_SetGeometryDirectly((OGRFeatureH)self, (OGRGeometryH)geom);
  }
%clear OGRGeometryShadow *geom;
  
  /* Feature owns its geometry */
  OGRGeometryShadow *GetGeometryRef() {
    return (OGRGeometryShadow*) OGR_F_GetGeometryRef((OGRFeatureH)self);
  }
  
  %newobject Clone;
  OGRFeatureShadow *Clone() {
    return (OGRFeatureShadow*) OGR_F_Clone((OGRFeatureH)self);
  }
  
  bool Equal(OGRFeatureShadow *feature) {
    return OGR_F_Equal((OGRFeatureH)self, (OGRFeatureH) feature);
  }
  
  int GetFieldCount() {
    return OGR_F_GetFieldCount((OGRFeatureH)self);
  }

  /* ---- GetFieldDefnRef --------------------- */
  OGRFieldDefnShadow *GetFieldDefnRef(int id) {
    return (OGRFieldDefnShadow *) OGR_F_GetFieldDefnRef((OGRFeatureH)self, id);
  }

  OGRFieldDefnShadow *GetFieldDefnRef(const char* name) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return (OGRFieldDefnShadow *) OGR_F_GetFieldDefnRef((OGRFeatureH)self, i);
      return NULL;
  }
  /* ------------------------------------------- */

  /* ---- GetFieldAsString --------------------- */

  const char* GetFieldAsString(int id) {
    return (const char *) OGR_F_GetFieldAsString((OGRFeatureH)self, id);
  }

#ifndef SWIGPERL
  const char* GetFieldAsString(const char* name) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return (const char *) OGR_F_GetFieldAsString((OGRFeatureH)self, i);
      return NULL;
  }
#endif
  /* ------------------------------------------- */

  /* ---- GetFieldAsInteger -------------------- */

  int GetFieldAsInteger(int id) {
    return OGR_F_GetFieldAsInteger((OGRFeatureH)self, id);
  }

#ifndef SWIGPERL
  int GetFieldAsInteger(const char* name) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return OGR_F_GetFieldAsInteger((OGRFeatureH)self, i);
      return 0;
  }
#endif
  /* ------------------------------------------- */  

  /* ---- GetFieldAsDouble --------------------- */

  double GetFieldAsDouble(int id) {
    return OGR_F_GetFieldAsDouble((OGRFeatureH)self, id);
  }

#ifndef SWIGPERL
  double GetFieldAsDouble(const char* name) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return OGR_F_GetFieldAsDouble((OGRFeatureH)self, i);
      return 0;
  }
#endif
  /* ------------------------------------------- */  

  %apply (int *OUTPUT) {(int *)};
  void GetFieldAsDateTime(int id, int *pnYear, int *pnMonth, int *pnDay,
			  int *pnHour, int *pnMinute, int *pnSecond,
			  int *pnTZFlag) {
      OGR_F_GetFieldAsDateTime((OGRFeatureH)self, id, pnYear, pnMonth, pnDay,
			       pnHour, pnMinute, pnSecond,
			       pnTZFlag);
  }
  %clear (int *);

  void GetFieldAsIntegerList(int id, int *nLen, const int **pList) {
      *pList = OGR_F_GetFieldAsIntegerList((OGRFeatureH)self, id, nLen);
  }

  void GetFieldAsDoubleList(int id, int *nLen, const double **pList) {
      *pList = OGR_F_GetFieldAsDoubleList((OGRFeatureH)self, id, nLen);
  }

  void GetFieldAsStringList(int id, char ***pList) {
      *pList = OGR_F_GetFieldAsStringList((OGRFeatureH)self, id);
  }
  
  /* ---- IsFieldSet --------------------------- */
  bool IsFieldSet(int id) {
    return OGR_F_IsFieldSet((OGRFeatureH)self, id);
  }

  bool IsFieldSet(const char* name) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  return OGR_F_IsFieldSet((OGRFeatureH)self, i);
      return (bool)0;
  }
  /* ------------------------------------------- */  
      
  int GetFieldIndex(const char* name) {
      return OGR_F_GetFieldIndex((OGRFeatureH)self, name);
  }

  int GetFID() {
    return OGR_F_GetFID((OGRFeatureH)self);
  }
  
  OGRErr SetFID(int fid) {
    return OGR_F_SetFID((OGRFeatureH)self, fid);
  }
  
  void DumpReadable() {
    OGR_F_DumpReadable((OGRFeatureH)self, NULL);
  }

  void UnsetField(int id) {
    OGR_F_UnsetField((OGRFeatureH)self, id);
  }

#ifndef SWIGPERL
  void UnsetField(const char* name) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_UnsetField((OGRFeatureH)self, i);
  }
#endif

  /* ---- SetField ----------------------------- */
  
  %apply ( tostring argin ) { (const char* value) };
  void SetField(int id, const char* value) {
    OGR_F_SetFieldString((OGRFeatureH)self, id, value);
  }

#ifndef SWIGPERL
  void SetField(const char* name, const char* value) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_SetFieldString((OGRFeatureH)self, i, value);
  }
#endif
  %clear (const char* value );
  
  void SetField(int id, int value) {
    OGR_F_SetFieldInteger((OGRFeatureH)self, id, value);
  }
  
#ifndef SWIGPERL
  void SetField(const char* name, int value) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_SetFieldInteger((OGRFeatureH)self, i, value);
  }
#endif
  
  void SetField(int id, double value) {
    OGR_F_SetFieldDouble((OGRFeatureH)self, id, value);
  }
  
#ifndef SWIGPERL
  void SetField(const char* name, double value) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_SetFieldDouble((OGRFeatureH)self, i, value);
  }
#endif
  
  void SetField( int id, int year, int month, int day,
                             int hour, int minute, int second, 
                             int tzflag ) {
    OGR_F_SetFieldDateTime((OGRFeatureH)self, id, year, month, day,
                             hour, minute, second, 
                             tzflag);
  }

#ifndef SWIGPERL  
  void SetField(const char* name, int year, int month, int day,
                             int hour, int minute, int second, 
                             int tzflag ) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1)
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
      else
	  OGR_F_SetFieldDateTime((OGRFeatureH)self, i, year, month, day,
				 hour, minute, second, 
				 tzflag);
  }
#endif

  void SetFieldIntegerList(int id, int nList, int *pList) {
      OGR_F_SetFieldIntegerList((OGRFeatureH)self, id, nList, pList);
  }

  void SetFieldDoubleList(int id, int nList, double *pList) {
      OGR_F_SetFieldDoubleList((OGRFeatureH)self, id, nList, pList);
  }

  void SetFieldStringList(int id, char **pList) {
      OGR_F_SetFieldStringList((OGRFeatureH)self, id, pList);
  }

  /* ------------------------------------------- */  
  
  %feature("kwargs") SetFrom;
  OGRErr SetFrom(OGRFeatureShadow *other, int forgiving=1) {
    return OGR_F_SetFrom((OGRFeatureH)self, (OGRFeatureH)other, forgiving);
  }
  
  const char *GetStyleString() {
    return (const char*) OGR_F_GetStyleString((OGRFeatureH)self);
  }
  
  void SetStyleString(const char* the_string) {
    OGR_F_SetStyleString((OGRFeatureH)self, the_string);
  }

  /* ---- GetFieldType ------------------------- */  
  OGRFieldType GetFieldType(int id) {
    return (OGRFieldType) OGR_Fld_GetType( OGR_F_GetFieldDefnRef( (OGRFeatureH)self, id));
  }
  
  OGRFieldType GetFieldType(const char* name) {
      int i = OGR_F_GetFieldIndex((OGRFeatureH)self, name);
      if (i == -1) {
	  CPLError(CE_Failure, 1, "No such field: '%s'", name);
	  return (OGRFieldType)0;
      } else
	  return (OGRFieldType) OGR_Fld_GetType( 
	      OGR_F_GetFieldDefnRef( (OGRFeatureH)self,  i )
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
    OGR_FD_Release( (OGRFeatureDefnH)self );
  }

  %feature("kwargs") OGRFeatureDefnShadow;
  OGRFeatureDefnShadow(const char* name_null_ok=NULL) {
    OGRFeatureDefnH h = OGR_FD_Create(name_null_ok);
    OGR_FD_Reference(h);
    return (OGRFeatureDefnShadow* )h;
  }
  
  const char* GetName(){
    return OGR_FD_GetName((OGRFeatureDefnH)self);
  }
  
  int GetFieldCount(){
    return OGR_FD_GetFieldCount((OGRFeatureDefnH)self);
  }
  
  /* FeatureDefns own their FieldDefns */
  OGRFieldDefnShadow* GetFieldDefn(int i){
    return (OGRFieldDefnShadow*) OGR_FD_GetFieldDefn((OGRFeatureDefnH)self, i);
  }

  int GetFieldIndex(const char* name) {
      return OGR_FD_GetFieldIndex((OGRFeatureDefnH)self, name);
  }
  
  void AddFieldDefn(OGRFieldDefnShadow* defn) {
    OGR_FD_AddFieldDefn((OGRFeatureDefnH)self, (OGRFieldDefnH) defn);
  }
  
  OGRwkbGeometryType GetGeomType() {
    return (OGRwkbGeometryType) OGR_FD_GetGeomType((OGRFeatureDefnH)self);
  }
  
  void SetGeomType(OGRwkbGeometryType geom_type) {
    OGR_FD_SetGeomType((OGRFeatureDefnH)self, geom_type);
  }
  
  int GetReferenceCount(){
    return OGR_FD_GetReferenceCount((OGRFeatureDefnH)self);
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
    OGR_Fld_Destroy((OGRFieldDefnH)self);
  }

  %feature("kwargs") OGRFieldDefnShadow;
  OGRFieldDefnShadow( const char* name_null_ok="unnamed", 
                      OGRFieldType field_type=OFTString) {
    return (OGRFieldDefnShadow*) OGR_Fld_Create(name_null_ok, field_type);
  }

  const char * GetName() {
    return (const char *) OGR_Fld_GetNameRef((OGRFieldDefnH)self);
  }
  
  const char * GetNameRef() {
    return (const char *) OGR_Fld_GetNameRef((OGRFieldDefnH)self);
  }
  
  void SetName( const char* name) {
    OGR_Fld_SetName((OGRFieldDefnH)self, name);
  }
  
  OGRFieldType GetType() {
    return OGR_Fld_GetType((OGRFieldDefnH)self);
  }

  void SetType(OGRFieldType type) {
    OGR_Fld_SetType((OGRFieldDefnH)self, type);
  }
  
  OGRJustification GetJustify() {
    return OGR_Fld_GetJustify((OGRFieldDefnH)self);
  }
  
  void SetJustify(OGRJustification justify) {
    OGR_Fld_SetJustify((OGRFieldDefnH)self, justify);
  }
  
  int GetWidth () {
    return OGR_Fld_GetWidth((OGRFieldDefnH)self);
  }
  
  void SetWidth (int width) {
    OGR_Fld_SetWidth((OGRFieldDefnH)self, width);
  }
  
  int GetPrecision() {
    return OGR_Fld_GetPrecision((OGRFieldDefnH)self);
  }
  
  void SetPrecision(int precision) {
    OGR_Fld_SetPrecision((OGRFieldDefnH)self, precision);
  }

  const char * GetFieldTypeName(OGRFieldType type) {
    return OGR_GetFieldTypeName(type);
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
    OGRGeometryH geom;
    OGRErr err = OGR_G_CreateFromWkb( (unsigned char *) bin_string,
                                      (OGRSpatialReferenceH) reference,
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
%feature("kwargs") CreateGeometryFromWkb;
%inline {
OGRGeometryShadow* CreateGeometryFromWkb(int nLen, unsigned char *pBuf, 
                                            OSRSpatialReferenceShadow *reference=NULL ) {
    OGRGeometryH geom;
    OGRErr err = OGR_G_CreateFromWkb((unsigned char*) pBuf, (OGRSpatialReferenceH) reference, &geom, nLen);
    if (err != 0 ) {
       CPLError(CE_Failure, err, "%s", OGRErrMessages(err));
       return NULL;
    }
    return (OGRGeometryShadow*) geom;
  }
}
#endif

%feature( "kwargs" ) CreateGeometryFromWkt;
%apply (char **ignorechange) { (char **) };
%newobject CreateGeometryFromWkt;
%inline %{
  OGRGeometryShadow* CreateGeometryFromWkt( char **val, 
                                      OSRSpatialReferenceShadow *reference=NULL ) {
    OGRGeometryH geom;
    OGRErr err = OGR_G_CreateFromWkt(val,
                                     (OGRSpatialReferenceH) reference,
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
%feature( "kwargs" ) BuildPolygonFromEdges;
%inline %{
  OGRGeometryShadow* BuildPolygonFromEdges( OGRGeometryShadow*  hLineCollection,  
                                            int bBestEffort = 0, 
                                            int bAutoClose = 0, 
                                            double dfTolerance=0) {
  
  OGRGeometryH hPolygon = NULL;
  
  OGRErr eErr;

  hPolygon = OGRBuildPolygonFromEdges( (OGRGeometryH) hLineCollection, 
                                       bBestEffort, 
                                       bAutoClose, dfTolerance, &eErr );

  if (eErr != OGRERR_NONE ) {
    CPLError(CE_Failure, eErr, "%s", OGRErrMessages(eErr));
    return NULL;
  }

  return hPolygon;
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
    OGR_G_DestroyGeometry( (OGRGeometryH)self );
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
    else return 0;
  }  
#ifdef SWIGCSHARP
%clear (char *wkb_buf);
#else
%clear (int wkb, char *wkb_buf);
#endif
#endif

  OGRErr ExportToWkt( char** argout ) {
    return OGR_G_ExportToWkt((OGRGeometryH)self, argout);
  }

#ifndef SWIGCSHARP
#ifndef SWIGJAVA
  %feature("kwargs") ExportToWkb;
  OGRErr ExportToWkb( int *nLen, char **pBuf, OGRwkbByteOrder byte_order=wkbXDR ) {
    *nLen = OGR_G_WkbSize( (OGRGeometryH)self );
    *pBuf = (char *) malloc( *nLen * sizeof(unsigned char) );
    return OGR_G_ExportToWkb((OGRGeometryH)self, byte_order, 
                             (unsigned char*) *pBuf );
  }
#endif
#endif

  const char * ExportToGML() {
    return (const char *) OGR_G_ExportToGML((OGRGeometryH)self);
  }
  
  const char * ExportToKML(const char* altitude_mode=NULL) {
    return (const char *) OGR_G_ExportToKML((OGRGeometryH)self, altitude_mode);
  }

  const char * ExportToJson() {
    return (const char *) OGR_G_ExportToJson((OGRGeometryH)self);
  }

  %feature("kwargs") AddPoint;
  void AddPoint(double x, double y, double z = 0) {
    OGR_G_AddPoint( (OGRGeometryH)self, x, y, z );
  }
  
  void AddPoint_2D(double x, double y) {
    OGR_G_AddPoint_2D( (OGRGeometryH)self, x, y );
  }

/* The geometry now owns an inner geometry */
%apply SWIGTYPE *DISOWN {OGRGeometryShadow* other_disown};
  OGRErr AddGeometryDirectly( OGRGeometryShadow* other_disown ) {
    return OGR_G_AddGeometryDirectly( (OGRGeometryH)self, (OGRGeometryH)other_disown );
  }
%clear OGRGeometryShadow* other_disown;

  OGRErr AddGeometry( OGRGeometryShadow* other ) {
    return OGR_G_AddGeometry( (OGRGeometryH)self, (OGRGeometryH)other );
  }

  %newobject Clone;
  OGRGeometryShadow* Clone() {
    return (OGRGeometryShadow*) OGR_G_Clone((OGRGeometryH)self);
  } 
    
  OGRwkbGeometryType GetGeometryType() {
    return (OGRwkbGeometryType) OGR_G_GetGeometryType((OGRGeometryH)self);
  }

  const char * GetGeometryName() {
    return (const char *) OGR_G_GetGeometryName((OGRGeometryH)self);
  }
  
  double GetArea() {
    return OGR_G_GetArea((OGRGeometryH)self);
  }
  
  int GetPointCount() {
    return OGR_G_GetPointCount((OGRGeometryH)self);
  }

  %feature("kwargs") GetX;  
  double GetX(int point=0) {
    return OGR_G_GetX((OGRGeometryH)self, point);
  }

  %feature("kwargs") GetY;  
  double GetY(int point=0) {
    return OGR_G_GetY((OGRGeometryH)self, point);
  }

  %feature("kwargs") GetZ;  
  double GetZ(int point=0) {
    return OGR_G_GetZ((OGRGeometryH)self, point);
  } 

  void GetPoint(int iPoint = 0, double argout[3] = NULL) {
    OGR_G_GetPoint( (OGRGeometryH)self, iPoint, argout+0, argout+1, argout+2 );
  }

  void GetPoint_2D(int iPoint = 0, double argout[2] = NULL) {
    OGR_G_GetPoint( (OGRGeometryH)self, iPoint, argout+0, argout+1, NULL );
  }

  int GetGeometryCount() {
    return OGR_G_GetGeometryCount((OGRGeometryH)self);
  }

  %feature("kwargs") SetPoint;    
  void SetPoint(int point, double x, double y, double z=0) {
    OGR_G_SetPoint((OGRGeometryH)self, point, x, y, z);
  }

  %feature("kwargs") SetPoint_2D;
  void SetPoint_2D(int point, double x, double y) {
    OGR_G_SetPoint_2D((OGRGeometryH)self, point, x, y);
  }
  
  /* Geometries own their internal geometries */
  OGRGeometryShadow* GetGeometryRef(int geom) {
    return (OGRGeometryShadow*) OGR_G_GetGeometryRef((OGRGeometryH)self, geom);
  }

  %newobject GetBoundary;
  OGRGeometryShadow* GetBoundary() {
    return (OGRGeometryShadow*) OGR_G_GetBoundary((OGRGeometryH)self);
  }

  %newobject ConvexHull;
  OGRGeometryShadow* ConvexHull() {
    return (OGRGeometryShadow*) OGR_G_ConvexHull((OGRGeometryH)self);
  } 

  %newobject Buffer;
  %feature("kwargs") Buffer; 
  OGRGeometryShadow* Buffer( double distance, int quadsecs=30 ) {
    return (OGRGeometryShadow*) OGR_G_Buffer( (OGRGeometryH)self, distance, quadsecs );
  }

  %newobject Intersection;
  OGRGeometryShadow* Intersection( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_Intersection( (OGRGeometryH)self, (OGRGeometryH)other );
  }  
  
  %newobject Union;
  OGRGeometryShadow* Union( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) OGR_G_Union( (OGRGeometryH)self, (OGRGeometryH)other );
  }  
  
  %newobject Difference;
  OGRGeometryShadow* Difference( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) 
        OGR_G_Difference( (OGRGeometryH)self, (OGRGeometryH)other );
  }  

  %newobject SymmetricDifference;
  OGRGeometryShadow* SymmetricDifference( OGRGeometryShadow* other ) {
    return (OGRGeometryShadow*) 
        OGR_G_SymmetricDifference( (OGRGeometryH)self, (OGRGeometryH)other );
  } 
  
  double Distance( OGRGeometryShadow* other) {
    return OGR_G_Distance((OGRGeometryH)self, (OGRGeometryH)other);
  }
  
  void Empty () {
    OGR_G_Empty((OGRGeometryH)self);
  }

  bool IsEmpty () {
    return OGR_G_IsEmpty((OGRGeometryH)self);
  }  
  
  bool IsValid () {
    return OGR_G_IsValid((OGRGeometryH)self);
  }  
  
  bool IsSimple () {
    return OGR_G_IsSimple((OGRGeometryH)self);
  }  
  
  bool IsRing () {
    return OGR_G_IsRing((OGRGeometryH)self);
  }  
  
  bool Intersect (OGRGeometryShadow* other) {
    return OGR_G_Intersect((OGRGeometryH)self, (OGRGeometryH)other);
  }

  bool Equal (OGRGeometryShadow* other) {
    return OGR_G_Equal((OGRGeometryH)self, (OGRGeometryH)other);
  }
  
  bool Disjoint(OGRGeometryShadow* other) {
    return OGR_G_Disjoint((OGRGeometryH)self, (OGRGeometryH)other);
  }

  bool Touches (OGRGeometryShadow* other) {
    return OGR_G_Touches((OGRGeometryH)self, (OGRGeometryH)other);
  }

  bool Crosses (OGRGeometryShadow* other) {
    return OGR_G_Crosses((OGRGeometryH)self, (OGRGeometryH)other);
  }

  bool Within (OGRGeometryShadow* other) {
    return OGR_G_Within((OGRGeometryH)self, (OGRGeometryH)other);
  }

  bool Contains (OGRGeometryShadow* other) {
    return OGR_G_Contains((OGRGeometryH)self, (OGRGeometryH)other);
  }
  
  bool Overlaps (OGRGeometryShadow* other) {
    return OGR_G_Overlaps((OGRGeometryH)self, (OGRGeometryH)other);
  }

  OGRErr TransformTo(OSRSpatialReferenceShadow* reference) {
    return OGR_G_TransformTo((OGRGeometryH)self, 
                             (OGRSpatialReferenceH) reference);
  }
  
  OGRErr Transform(OSRCoordinateTransformationShadow* trans) {
    return OGR_G_Transform((OGRGeometryH)self, 
                           (OGRCoordinateTransformationH) trans);
  }
  
  %newobject GetSpatialReference;
  OSRSpatialReferenceShadow* GetSpatialReference() {
    OGRSpatialReferenceH ref =  OGR_G_GetSpatialReference((OGRGeometryH)self);
    if( ref )
        OSRReference(ref);
    return (OSRSpatialReferenceShadow*) ref;
  }
  
  void AssignSpatialReference(OSRSpatialReferenceShadow* reference) {
    OGR_G_AssignSpatialReference((OGRGeometryH)self, 
                                 (OGRSpatialReferenceH) reference);
  }
  
  void CloseRings() {
    OGR_G_CloseRings((OGRGeometryH)self);
  }
  
  void FlattenTo2D() {
    OGR_G_FlattenTo2D((OGRGeometryH)self);
  }

#if defined(SWIGCSHARP)  
  void GetEnvelope(OGREnvelope *env) {
    OGR_G_GetEnvelope((OGRGeometryH)self, env);
  }
#else
  void GetEnvelope(double argout[4]) {
    OGR_G_GetEnvelope((OGRGeometryH)self, (OGREnvelope*)argout);
  }
#endif  

#if !defined(SWIGJAVA)
  %newobject Centroid;
  OGRGeometryShadow* Centroid() {
    OGRGeometryShadow *pt = new_OGRGeometryShadow( wkbPoint );
    OGR_G_Centroid( (OGRGeometryH) self, (OGRGeometryH) pt );
    return pt;
  }
#endif
  
  int WkbSize() {
    return OGR_G_WkbSize((OGRGeometryH)self);
  }
  
  int GetCoordinateDimension() {
    return OGR_G_GetCoordinateDimension((OGRGeometryH)self);
  }

  void SetCoordinateDimension(int dimension) {
    OGR_G_SetCoordinateDimension((OGRGeometryH)self, dimension);
  }
  
  int GetDimension() {
    return OGR_G_GetDimension((OGRGeometryH)self);
  }

} /* %extend */

}; /* class OGRGeometryShadow */


/************************************************************************/
/*                        Other misc functions.                         */
/************************************************************************/

%{
char const *OGRDriverShadow_get_name( OGRDriverShadow *h ) {
  return OGR_Dr_GetName( (OGRSFDriverH) h );
}

char const *OGRDataSourceShadow_get_name( OGRDataSourceShadow *h ) {
  return OGR_DS_GetName( (OGRDataSourceH) h );
}

char const *OGRDriverShadow_name_get( OGRDriverShadow *h ) {
  return OGR_Dr_GetName( (OGRSFDriverH) h );
}

char const *OGRDataSourceShadow_name_get( OGRDataSourceShadow *h ) {
  return OGR_DS_GetName( (OGRDataSourceH) h );
}
%}

#ifndef GDAL_BINDINGS
int OGRGetDriverCount();
#endif

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
    CPLErrorReset();
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpen(filename,update,NULL);
    if( CPLGetLastErrorType() == CE_Failure && ds != NULL )
    {
        CPLDebug( "SWIG", 
		  "OGROpen() succeeded, but an error is posted, so we destroy"
		  " the datasource and fail at swig level." );
        OGRReleaseDataSource((OGRDataSourceH) ds);
        ds = NULL;
    }
	
    return ds;
  }
%}

%newobject OpenShared;
%feature( "kwargs" ) OpenShared;
%inline %{
  OGRDataSourceShadow* OpenShared( const char *filename, int update =0 ) {
    CPLErrorReset();
    OGRDataSourceShadow* ds = (OGRDataSourceShadow*)OGROpenShared(filename,update,NULL);
    if( CPLGetLastErrorType() == CE_Failure && ds != NULL )
    {
        OGRReleaseDataSource((OGRDataSourceH)ds);
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
