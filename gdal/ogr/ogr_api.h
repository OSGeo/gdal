/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API for OGR Geometry, Feature, Layers, DataSource and drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 ****************************************************************************/

#ifndef OGR_API_H_INCLUDED
#define OGR_API_H_INCLUDED

/**
 * \file ogr_api.h
 * 
 * C API and defines for OGRFeature, OGRGeometry, and OGRDataSource
 * related classes. 
 * 
 * See also: ogr_geometry.h, ogr_feature.h, ogrsf_frmts.h, ogr_featurestyle.h
 */

#include "gdal.h"
#include "ogr_core.h"

CPL_C_START

/* -------------------------------------------------------------------- */
/*      Geometry related functions (ogr_geometry.h)                     */
/* -------------------------------------------------------------------- */
#ifdef DEBUG
typedef struct OGRGeometryHS *OGRGeometryH;
#else
typedef void *OGRGeometryH;
#endif

#ifndef _DEFINED_OGRSpatialReferenceH
#define _DEFINED_OGRSpatialReferenceH

#ifdef DEBUG
typedef struct OGRSpatialReferenceHS *OGRSpatialReferenceH;
typedef struct OGRCoordinateTransformationHS *OGRCoordinateTransformationH;
#else
typedef void *OGRSpatialReferenceH;                               
typedef void *OGRCoordinateTransformationH;
#endif

#endif

struct _CPLXMLNode;

/* From base OGRGeometry class */

OGRErr CPL_DLL OGR_G_CreateFromWkb( unsigned char *, OGRSpatialReferenceH, 
                                    OGRGeometryH *, int );
OGRErr CPL_DLL OGR_G_CreateFromWkt( char **, OGRSpatialReferenceH, 
                                    OGRGeometryH * );
OGRErr CPL_DLL OGR_G_CreateFromFgf( unsigned char *, OGRSpatialReferenceH, 
                                    OGRGeometryH *, int, int * );
void   CPL_DLL OGR_G_DestroyGeometry( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_CreateGeometry( OGRwkbGeometryType );
OGRGeometryH CPL_DLL 
OGR_G_ApproximateArcAngles( 
    double dfCenterX, double dfCenterY, double dfZ,
    double dfPrimaryRadius, double dfSecondaryAxis, double dfRotation, 
    double dfStartAngle, double dfEndAngle,
    double dfMaxAngleStepSizeDegrees );

OGRGeometryH CPL_DLL OGR_G_ForceToPolygon( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_ForceToMultiPolygon( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_ForceToMultiPoint( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_ForceToMultiLineString( OGRGeometryH );

int    CPL_DLL OGR_G_GetDimension( OGRGeometryH );
int    CPL_DLL OGR_G_GetCoordinateDimension( OGRGeometryH );
void   CPL_DLL OGR_G_SetCoordinateDimension( OGRGeometryH, int );
OGRGeometryH CPL_DLL OGR_G_Clone( OGRGeometryH );
void   CPL_DLL OGR_G_GetEnvelope( OGRGeometryH, OGREnvelope * );
void   CPL_DLL OGR_G_GetEnvelope3D( OGRGeometryH, OGREnvelope3D * );
OGRErr CPL_DLL OGR_G_ImportFromWkb( OGRGeometryH, unsigned char *, int );
OGRErr CPL_DLL OGR_G_ExportToWkb( OGRGeometryH, OGRwkbByteOrder, unsigned char*);
int    CPL_DLL OGR_G_WkbSize( OGRGeometryH hGeom );
OGRErr CPL_DLL OGR_G_ImportFromWkt( OGRGeometryH, char ** );
OGRErr CPL_DLL OGR_G_ExportToWkt( OGRGeometryH, char ** );
OGRwkbGeometryType CPL_DLL OGR_G_GetGeometryType( OGRGeometryH );
const char CPL_DLL *OGR_G_GetGeometryName( OGRGeometryH );
void   CPL_DLL OGR_G_DumpReadable( OGRGeometryH, FILE *, const char * );
void   CPL_DLL OGR_G_FlattenTo2D( OGRGeometryH );
void   CPL_DLL OGR_G_CloseRings( OGRGeometryH );

OGRGeometryH CPL_DLL OGR_G_CreateFromGML( const char * );
char   CPL_DLL *OGR_G_ExportToGML( OGRGeometryH );
char   CPL_DLL *OGR_G_ExportToGMLEx( OGRGeometryH, char** papszOptions );

#if defined(_CPL_MINIXML_H_INCLUDED)
OGRGeometryH CPL_DLL OGR_G_CreateFromGMLTree( const CPLXMLNode * );
CPLXMLNode CPL_DLL *OGR_G_ExportToGMLTree( OGRGeometryH );
CPLXMLNode CPL_DLL *OGR_G_ExportEnvelopeToGMLTree( OGRGeometryH );
#endif

char   CPL_DLL *OGR_G_ExportToKML( OGRGeometryH, const char* pszAltitudeMode );

char   CPL_DLL *OGR_G_ExportToJson( OGRGeometryH );
char   CPL_DLL *OGR_G_ExportToJsonEx( OGRGeometryH, char** papszOptions );
OGRGeometryH CPL_DLL OGR_G_CreateGeometryFromJson( const char* );

void   CPL_DLL OGR_G_AssignSpatialReference( OGRGeometryH, 
                                             OGRSpatialReferenceH );
OGRSpatialReferenceH CPL_DLL OGR_G_GetSpatialReference( OGRGeometryH );
OGRErr CPL_DLL OGR_G_Transform( OGRGeometryH, OGRCoordinateTransformationH );
OGRErr CPL_DLL OGR_G_TransformTo( OGRGeometryH, OGRSpatialReferenceH );

OGRGeometryH CPL_DLL OGR_G_Simplify( OGRGeometryH hThis, double tolerance );
OGRGeometryH CPL_DLL OGR_G_SimplifyPreserveTopology( OGRGeometryH hThis, double tolerance );

void   CPL_DLL OGR_G_Segmentize(OGRGeometryH hGeom, double dfMaxLength );
int    CPL_DLL OGR_G_Intersects( OGRGeometryH, OGRGeometryH );
int    CPL_DLL OGR_G_Equals( OGRGeometryH, OGRGeometryH );
/*int    CPL_DLL OGR_G_EqualsExact( OGRGeometryH, OGRGeometryH, double );*/
int    CPL_DLL OGR_G_Disjoint( OGRGeometryH, OGRGeometryH );
int    CPL_DLL OGR_G_Touches( OGRGeometryH, OGRGeometryH );
int    CPL_DLL OGR_G_Crosses( OGRGeometryH, OGRGeometryH );
int    CPL_DLL OGR_G_Within( OGRGeometryH, OGRGeometryH );
int    CPL_DLL OGR_G_Contains( OGRGeometryH, OGRGeometryH );
int    CPL_DLL OGR_G_Overlaps( OGRGeometryH, OGRGeometryH );

OGRGeometryH CPL_DLL OGR_G_Boundary( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_ConvexHull( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_Buffer( OGRGeometryH, double, int );
OGRGeometryH CPL_DLL OGR_G_Intersection( OGRGeometryH, OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_Union( OGRGeometryH, OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_UnionCascaded( OGRGeometryH );
/*OGRGeometryH CPL_DLL OGR_G_PointOnSurface( OGRGeometryH );*/
/*OGRGeometryH CPL_DLL OGR_G_Polygonize( OGRGeometryH *, int);*/
/*OGRGeometryH CPL_DLL OGR_G_Polygonizer_getCutEdges( OGRGeometryH *, int);*/
/*OGRGeometryH CPL_DLL OGR_G_LineMerge( OGRGeometryH );*/

OGRGeometryH CPL_DLL OGR_G_Difference( OGRGeometryH, OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_SymDifference( OGRGeometryH, OGRGeometryH );
double CPL_DLL OGR_G_Distance( OGRGeometryH, OGRGeometryH );
double CPL_DLL OGR_G_Length( OGRGeometryH );
double CPL_DLL OGR_G_Area( OGRGeometryH );
int    CPL_DLL OGR_G_Centroid( OGRGeometryH, OGRGeometryH );

void   CPL_DLL OGR_G_Empty( OGRGeometryH );
int    CPL_DLL OGR_G_IsEmpty( OGRGeometryH );
int    CPL_DLL OGR_G_IsValid( OGRGeometryH );
/*char    CPL_DLL *OGR_G_IsValidReason( OGRGeometryH );*/
int    CPL_DLL OGR_G_IsSimple( OGRGeometryH );
int    CPL_DLL OGR_G_IsRing( OGRGeometryH );
 
OGRGeometryH CPL_DLL OGR_G_Polygonize( OGRGeometryH );

/* backward compatibility (non-standard methods) */
int    CPL_DLL OGR_G_Intersect( OGRGeometryH, OGRGeometryH );
int    CPL_DLL OGR_G_Equal( OGRGeometryH, OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_SymmetricDifference( OGRGeometryH, OGRGeometryH );
double CPL_DLL OGR_G_GetArea( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_GetBoundary( OGRGeometryH );

/* Methods for getting/setting vertices in points, line strings and rings */
int    CPL_DLL OGR_G_GetPointCount( OGRGeometryH );
int    CPL_DLL OGR_G_GetPoints( OGRGeometryH hGeom,
                                void* pabyX, int nXStride,
                                void* pabyY, int nYStride,
                                void* pabyZ, int nZStride);
double CPL_DLL OGR_G_GetX( OGRGeometryH, int );
double CPL_DLL OGR_G_GetY( OGRGeometryH, int );
double CPL_DLL OGR_G_GetZ( OGRGeometryH, int );
void   CPL_DLL OGR_G_GetPoint( OGRGeometryH, int iPoint, 
                               double *, double *, double * );
void   CPL_DLL OGR_G_SetPoint( OGRGeometryH, int iPoint, 
                               double, double, double );
void   CPL_DLL OGR_G_SetPoint_2D( OGRGeometryH, int iPoint, 
                                  double, double );
void   CPL_DLL OGR_G_AddPoint( OGRGeometryH, double, double, double );
void   CPL_DLL OGR_G_AddPoint_2D( OGRGeometryH, double, double );

/* Methods for getting/setting rings and members collections */

int    CPL_DLL OGR_G_GetGeometryCount( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_GetGeometryRef( OGRGeometryH, int );
OGRErr CPL_DLL OGR_G_AddGeometry( OGRGeometryH, OGRGeometryH );
OGRErr CPL_DLL OGR_G_AddGeometryDirectly( OGRGeometryH, OGRGeometryH );
OGRErr CPL_DLL OGR_G_RemoveGeometry( OGRGeometryH, int, int );

OGRGeometryH CPL_DLL OGRBuildPolygonFromEdges( OGRGeometryH hLinesAsCollection,
                                       int bBestEffort, 
                                       int bAutoClose, 
                                       double dfTolerance,
                                       OGRErr * peErr );

OGRErr CPL_DLL OGRSetGenerate_DB2_V72_BYTE_ORDER( 
    int bGenerate_DB2_V72_BYTE_ORDER );

int CPL_DLL OGRGetGenerate_DB2_V72_BYTE_ORDER(void);

/* -------------------------------------------------------------------- */
/*      Feature related (ogr_feature.h)                                 */
/* -------------------------------------------------------------------- */

#ifdef DEBUG
typedef struct OGRFieldDefnHS   *OGRFieldDefnH;
typedef struct OGRFeatureDefnHS *OGRFeatureDefnH;
typedef struct OGRFeatureHS     *OGRFeatureH;
typedef struct OGRStyleTableHS *OGRStyleTableH;
#else
typedef void *OGRFieldDefnH;
typedef void *OGRFeatureDefnH;
typedef void *OGRFeatureH;
typedef void *OGRStyleTableH;
#endif

/* OGRFieldDefn */

OGRFieldDefnH CPL_DLL OGR_Fld_Create( const char *, OGRFieldType ) CPL_WARN_UNUSED_RESULT;
void   CPL_DLL OGR_Fld_Destroy( OGRFieldDefnH );

void   CPL_DLL OGR_Fld_SetName( OGRFieldDefnH, const char * );
const char CPL_DLL *OGR_Fld_GetNameRef( OGRFieldDefnH );
OGRFieldType CPL_DLL OGR_Fld_GetType( OGRFieldDefnH );
void   CPL_DLL OGR_Fld_SetType( OGRFieldDefnH, OGRFieldType );
OGRJustification CPL_DLL OGR_Fld_GetJustify( OGRFieldDefnH );
void   CPL_DLL OGR_Fld_SetJustify( OGRFieldDefnH, OGRJustification );
int    CPL_DLL OGR_Fld_GetWidth( OGRFieldDefnH );
void   CPL_DLL OGR_Fld_SetWidth( OGRFieldDefnH, int );
int    CPL_DLL OGR_Fld_GetPrecision( OGRFieldDefnH );
void   CPL_DLL OGR_Fld_SetPrecision( OGRFieldDefnH, int );
void   CPL_DLL OGR_Fld_Set( OGRFieldDefnH, const char *, OGRFieldType, 
                            int, int, OGRJustification );
int    CPL_DLL OGR_Fld_IsIgnored( OGRFieldDefnH hDefn );
void   CPL_DLL OGR_Fld_SetIgnored( OGRFieldDefnH hDefn, int );

const char CPL_DLL *OGR_GetFieldTypeName( OGRFieldType );

/* OGRFeatureDefn */

OGRFeatureDefnH CPL_DLL OGR_FD_Create( const char * ) CPL_WARN_UNUSED_RESULT;
void   CPL_DLL OGR_FD_Destroy( OGRFeatureDefnH );
void   CPL_DLL OGR_FD_Release( OGRFeatureDefnH );
const char CPL_DLL *OGR_FD_GetName( OGRFeatureDefnH );
int    CPL_DLL OGR_FD_GetFieldCount( OGRFeatureDefnH );
OGRFieldDefnH CPL_DLL OGR_FD_GetFieldDefn( OGRFeatureDefnH, int );
int    CPL_DLL OGR_FD_GetFieldIndex( OGRFeatureDefnH, const char * );
void   CPL_DLL OGR_FD_AddFieldDefn( OGRFeatureDefnH, OGRFieldDefnH );
OGRErr CPL_DLL OGR_FD_DeleteFieldDefn( OGRFeatureDefnH hDefn, int iField );
OGRErr CPL_DLL OGR_FD_ReorderFieldDefns( OGRFeatureDefnH hDefn, int* panMap );
OGRwkbGeometryType CPL_DLL OGR_FD_GetGeomType( OGRFeatureDefnH );
void   CPL_DLL OGR_FD_SetGeomType( OGRFeatureDefnH, OGRwkbGeometryType );
int    CPL_DLL OGR_FD_IsGeometryIgnored( OGRFeatureDefnH );
void   CPL_DLL OGR_FD_SetGeometryIgnored( OGRFeatureDefnH, int );
int    CPL_DLL OGR_FD_IsStyleIgnored( OGRFeatureDefnH );
void   CPL_DLL OGR_FD_SetStyleIgnored( OGRFeatureDefnH, int );
int    CPL_DLL OGR_FD_Reference( OGRFeatureDefnH );
int    CPL_DLL OGR_FD_Dereference( OGRFeatureDefnH );
int    CPL_DLL OGR_FD_GetReferenceCount( OGRFeatureDefnH );

/* OGRFeature */

OGRFeatureH CPL_DLL OGR_F_Create( OGRFeatureDefnH ) CPL_WARN_UNUSED_RESULT;
void   CPL_DLL OGR_F_Destroy( OGRFeatureH );
OGRFeatureDefnH CPL_DLL OGR_F_GetDefnRef( OGRFeatureH );

OGRErr CPL_DLL OGR_F_SetGeometryDirectly( OGRFeatureH, OGRGeometryH );
OGRErr CPL_DLL OGR_F_SetGeometry( OGRFeatureH, OGRGeometryH );
OGRGeometryH CPL_DLL OGR_F_GetGeometryRef( OGRFeatureH );
OGRGeometryH CPL_DLL OGR_F_StealGeometry( OGRFeatureH );
OGRFeatureH CPL_DLL OGR_F_Clone( OGRFeatureH );
int    CPL_DLL OGR_F_Equal( OGRFeatureH, OGRFeatureH );

int    CPL_DLL OGR_F_GetFieldCount( OGRFeatureH );
OGRFieldDefnH CPL_DLL OGR_F_GetFieldDefnRef( OGRFeatureH, int );
int    CPL_DLL OGR_F_GetFieldIndex( OGRFeatureH, const char * );

int    CPL_DLL OGR_F_IsFieldSet( OGRFeatureH, int );
void   CPL_DLL OGR_F_UnsetField( OGRFeatureH, int );
OGRField CPL_DLL *OGR_F_GetRawFieldRef( OGRFeatureH, int );

int    CPL_DLL OGR_F_GetFieldAsInteger( OGRFeatureH, int );
double CPL_DLL OGR_F_GetFieldAsDouble( OGRFeatureH, int );
const char CPL_DLL *OGR_F_GetFieldAsString( OGRFeatureH, int );
const int CPL_DLL *OGR_F_GetFieldAsIntegerList( OGRFeatureH, int, int * );
const double CPL_DLL *OGR_F_GetFieldAsDoubleList( OGRFeatureH, int, int * );
char  CPL_DLL **OGR_F_GetFieldAsStringList( OGRFeatureH, int );
GByte CPL_DLL *OGR_F_GetFieldAsBinary( OGRFeatureH, int, int * );
int   CPL_DLL  OGR_F_GetFieldAsDateTime( OGRFeatureH, int, int *, int *, int *,
                                         int *, int *, int *, int * );

void   CPL_DLL OGR_F_SetFieldInteger( OGRFeatureH, int, int );
void   CPL_DLL OGR_F_SetFieldDouble( OGRFeatureH, int, double );
void   CPL_DLL OGR_F_SetFieldString( OGRFeatureH, int, const char * );
void   CPL_DLL OGR_F_SetFieldIntegerList( OGRFeatureH, int, int, int * );
void   CPL_DLL OGR_F_SetFieldDoubleList( OGRFeatureH, int, int, double * );
void   CPL_DLL OGR_F_SetFieldStringList( OGRFeatureH, int, char ** );
void   CPL_DLL OGR_F_SetFieldRaw( OGRFeatureH, int, OGRField * );
void   CPL_DLL OGR_F_SetFieldBinary( OGRFeatureH, int, int, GByte * );
void   CPL_DLL OGR_F_SetFieldDateTime( OGRFeatureH, int, 
                                       int, int, int, int, int, int, int );

long   CPL_DLL OGR_F_GetFID( OGRFeatureH );
OGRErr CPL_DLL OGR_F_SetFID( OGRFeatureH, long );
void   CPL_DLL OGR_F_DumpReadable( OGRFeatureH, FILE * );
OGRErr CPL_DLL OGR_F_SetFrom( OGRFeatureH, OGRFeatureH, int );
OGRErr CPL_DLL OGR_F_SetFromWithMap( OGRFeatureH, OGRFeatureH, int , int * );

const char CPL_DLL *OGR_F_GetStyleString( OGRFeatureH );
void   CPL_DLL OGR_F_SetStyleString( OGRFeatureH, const char * );
void   CPL_DLL OGR_F_SetStyleStringDirectly( OGRFeatureH, char * );
OGRStyleTableH CPL_DLL OGR_F_GetStyleTable( OGRFeatureH );
void   CPL_DLL OGR_F_SetStyleTableDirectly( OGRFeatureH, OGRStyleTableH );
void   CPL_DLL OGR_F_SetStyleTable( OGRFeatureH, OGRStyleTableH );

/* -------------------------------------------------------------------- */
/*      ogrsf_frmts.h                                                   */
/* -------------------------------------------------------------------- */

#ifdef DEBUG
typedef struct OGRLayerHS      *OGRLayerH;
typedef struct OGRDataSourceHS *OGRDataSourceH;
typedef struct OGRDriverHS     *OGRSFDriverH;
#else
typedef void *OGRLayerH;
typedef void *OGRDataSourceH;
typedef void *OGRSFDriverH;
#endif

/* OGRLayer */

const char CPL_DLL* OGR_L_GetName( OGRLayerH );
OGRwkbGeometryType CPL_DLL OGR_L_GetGeomType( OGRLayerH );
OGRGeometryH CPL_DLL OGR_L_GetSpatialFilter( OGRLayerH );
void   CPL_DLL OGR_L_SetSpatialFilter( OGRLayerH, OGRGeometryH );
void   CPL_DLL OGR_L_SetSpatialFilterRect( OGRLayerH, 
                                           double, double, double, double );
OGRErr CPL_DLL OGR_L_SetAttributeFilter( OGRLayerH, const char * );
void   CPL_DLL OGR_L_ResetReading( OGRLayerH );
OGRFeatureH CPL_DLL OGR_L_GetNextFeature( OGRLayerH );
OGRErr CPL_DLL OGR_L_SetNextByIndex( OGRLayerH, long );
OGRFeatureH CPL_DLL OGR_L_GetFeature( OGRLayerH, long );
OGRErr CPL_DLL OGR_L_SetFeature( OGRLayerH, OGRFeatureH );
OGRErr CPL_DLL OGR_L_CreateFeature( OGRLayerH, OGRFeatureH );
OGRErr CPL_DLL OGR_L_DeleteFeature( OGRLayerH, long );
OGRFeatureDefnH CPL_DLL OGR_L_GetLayerDefn( OGRLayerH );
OGRSpatialReferenceH CPL_DLL OGR_L_GetSpatialRef( OGRLayerH );
int    CPL_DLL OGR_L_GetFeatureCount( OGRLayerH, int );
OGRErr CPL_DLL OGR_L_GetExtent( OGRLayerH, OGREnvelope *, int );
int    CPL_DLL OGR_L_TestCapability( OGRLayerH, const char * );
OGRErr CPL_DLL OGR_L_CreateField( OGRLayerH, OGRFieldDefnH, int );
OGRErr CPL_DLL OGR_L_DeleteField( OGRLayerH, int iField );
OGRErr CPL_DLL OGR_L_ReorderFields( OGRLayerH, int* panMap );
OGRErr CPL_DLL OGR_L_ReorderField( OGRLayerH, int iOldFieldPos, int iNewFieldPos );
OGRErr CPL_DLL OGR_L_AlterFieldDefn( OGRLayerH, int iField, OGRFieldDefnH hNewFieldDefn, int nFlags );
OGRErr CPL_DLL OGR_L_StartTransaction( OGRLayerH );
OGRErr CPL_DLL OGR_L_CommitTransaction( OGRLayerH );
OGRErr CPL_DLL OGR_L_RollbackTransaction( OGRLayerH );
int    CPL_DLL OGR_L_Reference( OGRLayerH );
int    CPL_DLL OGR_L_Dereference( OGRLayerH );
int    CPL_DLL OGR_L_GetRefCount( OGRLayerH );
OGRErr CPL_DLL OGR_L_SyncToDisk( OGRLayerH );
GIntBig CPL_DLL OGR_L_GetFeaturesRead( OGRLayerH );
const char CPL_DLL *OGR_L_GetFIDColumn( OGRLayerH );
const char CPL_DLL *OGR_L_GetGeometryColumn( OGRLayerH );
OGRStyleTableH CPL_DLL OGR_L_GetStyleTable( OGRLayerH );
void   CPL_DLL OGR_L_SetStyleTableDirectly( OGRLayerH, OGRStyleTableH );
void   CPL_DLL OGR_L_SetStyleTable( OGRLayerH, OGRStyleTableH );
OGRErr CPL_DLL OGR_L_SetIgnoredFields( OGRLayerH, const char** );
OGRErr CPL_DLL OGR_L_Intersection( OGRLayerH, OGRLayerH, OGRLayerH, char**, GDALProgressFunc, void * );
OGRErr CPL_DLL OGR_L_Union( OGRLayerH, OGRLayerH, OGRLayerH, char**, GDALProgressFunc, void * );
OGRErr CPL_DLL OGR_L_SymDifference( OGRLayerH, OGRLayerH, OGRLayerH, char**, GDALProgressFunc, void * );
OGRErr CPL_DLL OGR_L_Identity( OGRLayerH, OGRLayerH, OGRLayerH, char**, GDALProgressFunc, void * );
OGRErr CPL_DLL OGR_L_Update( OGRLayerH, OGRLayerH, OGRLayerH, char**, GDALProgressFunc, void * );
OGRErr CPL_DLL OGR_L_Clip( OGRLayerH, OGRLayerH, OGRLayerH, char**, GDALProgressFunc, void * );
OGRErr CPL_DLL OGR_L_Erase( OGRLayerH, OGRLayerH, OGRLayerH, char**, GDALProgressFunc, void * );

/* OGRDataSource */

void   CPL_DLL OGR_DS_Destroy( OGRDataSourceH );
const char CPL_DLL *OGR_DS_GetName( OGRDataSourceH );
int    CPL_DLL OGR_DS_GetLayerCount( OGRDataSourceH );
OGRLayerH CPL_DLL OGR_DS_GetLayer( OGRDataSourceH, int );
OGRLayerH CPL_DLL OGR_DS_GetLayerByName( OGRDataSourceH, const char * );
OGRErr    CPL_DLL OGR_DS_DeleteLayer( OGRDataSourceH, int );
OGRSFDriverH CPL_DLL OGR_DS_GetDriver( OGRDataSourceH );
OGRLayerH CPL_DLL OGR_DS_CreateLayer( OGRDataSourceH, const char *, 
                                      OGRSpatialReferenceH, OGRwkbGeometryType,
                                      char ** );
OGRLayerH CPL_DLL OGR_DS_CopyLayer( OGRDataSourceH, OGRLayerH, const char *,
                                    char ** );
int    CPL_DLL OGR_DS_TestCapability( OGRDataSourceH, const char * );
OGRLayerH CPL_DLL OGR_DS_ExecuteSQL( OGRDataSourceH, const char *,
                                     OGRGeometryH, const char * );
void   CPL_DLL OGR_DS_ReleaseResultSet( OGRDataSourceH, OGRLayerH );
int    CPL_DLL OGR_DS_Reference( OGRDataSourceH );
int    CPL_DLL OGR_DS_Dereference( OGRDataSourceH );
int    CPL_DLL OGR_DS_GetRefCount( OGRDataSourceH );
int    CPL_DLL OGR_DS_GetSummaryRefCount( OGRDataSourceH );
OGRErr CPL_DLL OGR_DS_SyncToDisk( OGRDataSourceH );
OGRStyleTableH CPL_DLL OGR_DS_GetStyleTable( OGRDataSourceH );
void   CPL_DLL OGR_DS_SetStyleTableDirectly( OGRDataSourceH, OGRStyleTableH );
void   CPL_DLL OGR_DS_SetStyleTable( OGRDataSourceH, OGRStyleTableH );

/* OGRSFDriver */

const char CPL_DLL *OGR_Dr_GetName( OGRSFDriverH );
OGRDataSourceH CPL_DLL OGR_Dr_Open( OGRSFDriverH, const char *, int ) CPL_WARN_UNUSED_RESULT;
int CPL_DLL OGR_Dr_TestCapability( OGRSFDriverH, const char * );
OGRDataSourceH CPL_DLL OGR_Dr_CreateDataSource( OGRSFDriverH, const char *,
                                                char ** ) CPL_WARN_UNUSED_RESULT;
OGRDataSourceH CPL_DLL OGR_Dr_CopyDataSource( OGRSFDriverH,  OGRDataSourceH, 
                                              const char *, char ** ) CPL_WARN_UNUSED_RESULT;
OGRErr CPL_DLL OGR_Dr_DeleteDataSource( OGRSFDriverH, const char * );

/* OGRSFDriverRegistrar */

OGRDataSourceH CPL_DLL OGROpen( const char *, int, OGRSFDriverH * ) CPL_WARN_UNUSED_RESULT;
OGRDataSourceH CPL_DLL OGROpenShared( const char *, int, OGRSFDriverH * ) CPL_WARN_UNUSED_RESULT;
OGRErr  CPL_DLL OGRReleaseDataSource( OGRDataSourceH );
void    CPL_DLL OGRRegisterDriver( OGRSFDriverH );
void    CPL_DLL OGRDeregisterDriver( OGRSFDriverH );
int     CPL_DLL OGRGetDriverCount(void);
OGRSFDriverH CPL_DLL OGRGetDriver( int );
OGRSFDriverH CPL_DLL OGRGetDriverByName( const char * );
int     CPL_DLL OGRGetOpenDSCount(void);
OGRDataSourceH CPL_DLL OGRGetOpenDS( int iDS );


/* note: this is also declared in ogrsf_frmts.h */
void CPL_DLL OGRRegisterAll(void);
void CPL_DLL OGRCleanupAll(void);

/* -------------------------------------------------------------------- */
/*      ogrsf_featurestyle.h                                            */
/* -------------------------------------------------------------------- */

#ifdef DEBUG
typedef struct OGRStyleMgrHS *OGRStyleMgrH;
typedef struct OGRStyleToolHS *OGRStyleToolH;
#else
typedef void *OGRStyleMgrH;
typedef void *OGRStyleToolH;
#endif

/* OGRStyleMgr */

OGRStyleMgrH CPL_DLL OGR_SM_Create(OGRStyleTableH hStyleTable) CPL_WARN_UNUSED_RESULT;
void    CPL_DLL OGR_SM_Destroy(OGRStyleMgrH hSM);

const char CPL_DLL *OGR_SM_InitFromFeature(OGRStyleMgrH hSM, 
                                           OGRFeatureH hFeat);
int     CPL_DLL OGR_SM_InitStyleString(OGRStyleMgrH hSM, 
                                       const char *pszStyleString);
int     CPL_DLL OGR_SM_GetPartCount(OGRStyleMgrH hSM, 
                                    const char *pszStyleString);
OGRStyleToolH CPL_DLL OGR_SM_GetPart(OGRStyleMgrH hSM, int nPartId, 
                                     const char *pszStyleString);
int     CPL_DLL OGR_SM_AddPart(OGRStyleMgrH hSM, OGRStyleToolH hST);
int     CPL_DLL OGR_SM_AddStyle(OGRStyleMgrH hSM, const char *pszStyleName, 
                               const char *pszStyleString);

/* OGRStyleTool */

OGRStyleToolH CPL_DLL OGR_ST_Create(OGRSTClassId eClassId) CPL_WARN_UNUSED_RESULT;
void    CPL_DLL OGR_ST_Destroy(OGRStyleToolH hST);

OGRSTClassId CPL_DLL OGR_ST_GetType(OGRStyleToolH hST);

OGRSTUnitId CPL_DLL OGR_ST_GetUnit(OGRStyleToolH hST);
void    CPL_DLL OGR_ST_SetUnit(OGRStyleToolH hST, OGRSTUnitId eUnit, 
                               double dfGroundPaperScale);

const char CPL_DLL *OGR_ST_GetParamStr(OGRStyleToolH hST, int eParam, int *bValueIsNull);
int     CPL_DLL OGR_ST_GetParamNum(OGRStyleToolH hST, int eParam, int *bValueIsNull);
double  CPL_DLL OGR_ST_GetParamDbl(OGRStyleToolH hST, int eParam, int *bValueIsNull);
void    CPL_DLL OGR_ST_SetParamStr(OGRStyleToolH hST, int eParam, const char *pszValue);
void    CPL_DLL OGR_ST_SetParamNum(OGRStyleToolH hST, int eParam, int nValue);
void    CPL_DLL OGR_ST_SetParamDbl(OGRStyleToolH hST, int eParam, double dfValue);
const char CPL_DLL *OGR_ST_GetStyleString(OGRStyleToolH hST);

int CPL_DLL OGR_ST_GetRGBFromString(OGRStyleToolH hST, const char *pszColor, 
                                    int *pnRed, int *pnGreen, int *pnBlue, 
                                    int *pnAlpha);

/* OGRStyleTable */

OGRStyleTableH  CPL_DLL OGR_STBL_Create( void ) CPL_WARN_UNUSED_RESULT;
void    CPL_DLL OGR_STBL_Destroy( OGRStyleTableH hSTBL ); 
int     CPL_DLL OGR_STBL_SaveStyleTable( OGRStyleTableH hStyleTable,
                                         const char *pszFilename );
int     CPL_DLL OGR_STBL_LoadStyleTable( OGRStyleTableH hStyleTable,
                                         const char *pszFilename );
const char CPL_DLL *OGR_STBL_Find( OGRStyleTableH hStyleTable, const char *pszName );
void    CPL_DLL OGR_STBL_ResetStyleStringReading( OGRStyleTableH hStyleTable );
const char CPL_DLL *OGR_STBL_GetNextStyle( OGRStyleTableH hStyleTable);
const char CPL_DLL *OGR_STBL_GetLastStyleName( OGRStyleTableH hStyleTable);

CPL_C_END

#endif /* ndef OGR_API_H_INCLUDED */
