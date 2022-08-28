/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API for OGR Geometry, Feature, Layers, DataSource and drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_progress.h"
#include "cpl_minixml.h"
#include "ogr_core.h"

#include <stdbool.h>
#include <stddef.h>

CPL_C_START

bool CPL_DLL OGRGetGEOSVersion(int *pnMajor, int *pnMinor, int *pnPatch);

/* -------------------------------------------------------------------- */
/*      Geometry related functions (ogr_geometry.h)                     */
/* -------------------------------------------------------------------- */
#ifndef DEFINEH_OGRGeometryH
/*! @cond Doxygen_Suppress */
#define DEFINEH_OGRGeometryH
/*! @endcond */
#ifdef DEBUG
typedef struct OGRGeometryHS *OGRGeometryH;
#else
/** Opaque type for a geometry */
typedef void *OGRGeometryH;
#endif
#endif /* DEFINEH_OGRGeometryH */

#ifndef DEFINED_OGRSpatialReferenceH
/*! @cond Doxygen_Suppress */
#define DEFINED_OGRSpatialReferenceH
/*! @endcond */

#ifndef DOXYGEN_XML
#ifdef DEBUG
typedef struct OGRSpatialReferenceHS *OGRSpatialReferenceH;
typedef struct OGRCoordinateTransformationHS *OGRCoordinateTransformationH;
#else
/** Opaque type for a spatial reference system */
typedef void *OGRSpatialReferenceH;
/** Opaque type for a coordinate transformation object */
typedef void *OGRCoordinateTransformationH;
#endif
#endif

#endif /* DEFINED_OGRSpatialReferenceH */

struct _CPLXMLNode;

/* From base OGRGeometry class */

OGRErr CPL_DLL OGR_G_CreateFromWkb( const void*, OGRSpatialReferenceH,
                                    OGRGeometryH *, int );
OGRErr CPL_DLL OGR_G_CreateFromWkbEx( const void*, OGRSpatialReferenceH,
                                      OGRGeometryH *, size_t );
OGRErr CPL_DLL OGR_G_CreateFromWkt( char **, OGRSpatialReferenceH,
                                    OGRGeometryH * );
OGRErr CPL_DLL OGR_G_CreateFromFgf( const void*, OGRSpatialReferenceH,
                                    OGRGeometryH *, int, int * );
void   CPL_DLL OGR_G_DestroyGeometry( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_CreateGeometry( OGRwkbGeometryType ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL
OGR_G_ApproximateArcAngles(
    double dfCenterX, double dfCenterY, double dfZ,
    double dfPrimaryRadius, double dfSecondaryAxis, double dfRotation,
    double dfStartAngle, double dfEndAngle,
    double dfMaxAngleStepSizeDegrees ) CPL_WARN_UNUSED_RESULT;

OGRGeometryH CPL_DLL OGR_G_ForceToPolygon( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_ForceToLineString( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_ForceToMultiPolygon( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_ForceToMultiPoint( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_ForceToMultiLineString( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_ForceTo( OGRGeometryH hGeom,
                                    OGRwkbGeometryType eTargetType,
                                    char** papszOptions ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_RemoveLowerDimensionSubGeoms( const OGRGeometryH hGeom ) CPL_WARN_UNUSED_RESULT;

int    CPL_DLL OGR_G_GetDimension( OGRGeometryH );
int    CPL_DLL OGR_G_GetCoordinateDimension( OGRGeometryH );
int    CPL_DLL OGR_G_CoordinateDimension( OGRGeometryH );
void   CPL_DLL OGR_G_SetCoordinateDimension( OGRGeometryH, int );
int    CPL_DLL OGR_G_Is3D( OGRGeometryH );
int    CPL_DLL OGR_G_IsMeasured( OGRGeometryH );
void   CPL_DLL OGR_G_Set3D( OGRGeometryH, int );
void   CPL_DLL OGR_G_SetMeasured( OGRGeometryH, int );
OGRGeometryH CPL_DLL OGR_G_Clone( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
void   CPL_DLL OGR_G_GetEnvelope( OGRGeometryH, OGREnvelope * );
void   CPL_DLL OGR_G_GetEnvelope3D( OGRGeometryH, OGREnvelope3D * );
OGRErr CPL_DLL OGR_G_ImportFromWkb( OGRGeometryH, const void*, int );
OGRErr CPL_DLL OGR_G_ExportToWkb( OGRGeometryH, OGRwkbByteOrder, unsigned char*);
OGRErr CPL_DLL OGR_G_ExportToIsoWkb( OGRGeometryH, OGRwkbByteOrder, unsigned char*);
int    CPL_DLL OGR_G_WkbSize( OGRGeometryH hGeom );
size_t CPL_DLL OGR_G_WkbSizeEx( OGRGeometryH hGeom );
OGRErr CPL_DLL OGR_G_ImportFromWkt( OGRGeometryH, char ** );
OGRErr CPL_DLL OGR_G_ExportToWkt( OGRGeometryH, char ** );
OGRErr CPL_DLL OGR_G_ExportToIsoWkt( OGRGeometryH, char ** );
OGRwkbGeometryType CPL_DLL OGR_G_GetGeometryType( OGRGeometryH );
const char CPL_DLL *OGR_G_GetGeometryName( OGRGeometryH );
void   CPL_DLL OGR_G_DumpReadable( OGRGeometryH, FILE *, const char * );
void   CPL_DLL OGR_G_FlattenTo2D( OGRGeometryH );
void   CPL_DLL OGR_G_CloseRings( OGRGeometryH );

OGRGeometryH CPL_DLL OGR_G_CreateFromGML( const char * ) CPL_WARN_UNUSED_RESULT;
char   CPL_DLL *OGR_G_ExportToGML( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
char   CPL_DLL *OGR_G_ExportToGMLEx( OGRGeometryH, char** papszOptions ) CPL_WARN_UNUSED_RESULT;

OGRGeometryH CPL_DLL OGR_G_CreateFromGMLTree( const CPLXMLNode * ) CPL_WARN_UNUSED_RESULT;
CPLXMLNode CPL_DLL *OGR_G_ExportToGMLTree( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
CPLXMLNode CPL_DLL *OGR_G_ExportEnvelopeToGMLTree( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;

char   CPL_DLL *OGR_G_ExportToKML( OGRGeometryH, const char* pszAltitudeMode ) CPL_WARN_UNUSED_RESULT;

char   CPL_DLL *OGR_G_ExportToJson( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
char   CPL_DLL *OGR_G_ExportToJsonEx( OGRGeometryH, char** papszOptions ) CPL_WARN_UNUSED_RESULT;
/** Create a OGR geometry from a GeoJSON geometry object */
OGRGeometryH CPL_DLL OGR_G_CreateGeometryFromJson( const char* ) CPL_WARN_UNUSED_RESULT;
/** Create a OGR geometry from a ESRI JSON geometry object */
OGRGeometryH CPL_DLL OGR_G_CreateGeometryFromEsriJson( const char* ) CPL_WARN_UNUSED_RESULT;

void   CPL_DLL OGR_G_AssignSpatialReference( OGRGeometryH,
                                             OGRSpatialReferenceH );
OGRSpatialReferenceH CPL_DLL OGR_G_GetSpatialReference( OGRGeometryH );
OGRErr CPL_DLL OGR_G_Transform( OGRGeometryH, OGRCoordinateTransformationH );
OGRErr CPL_DLL OGR_G_TransformTo( OGRGeometryH, OGRSpatialReferenceH );

/** Opaque type for a geometry transformer. */
typedef struct OGRGeomTransformer* OGRGeomTransformerH;
OGRGeomTransformerH CPL_DLL OGR_GeomTransformer_Create( OGRCoordinateTransformationH,
                                                        CSLConstList papszOptions ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_GeomTransformer_Transform(OGRGeomTransformerH hTransformer, OGRGeometryH hGeom ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL OGR_GeomTransformer_Destroy(OGRGeomTransformerH hTransformer);

OGRGeometryH CPL_DLL OGR_G_Simplify( OGRGeometryH hThis, double tolerance ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_SimplifyPreserveTopology( OGRGeometryH hThis, double tolerance ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_DelaunayTriangulation( OGRGeometryH hThis, double dfTolerance, int bOnlyEdges ) CPL_WARN_UNUSED_RESULT;

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

OGRGeometryH CPL_DLL OGR_G_Boundary( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_ConvexHull( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_ConcaveHull( OGRGeometryH, double dfRatio, bool bAllowHoles ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_Buffer( OGRGeometryH, double, int ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_Intersection( OGRGeometryH, OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_Union( OGRGeometryH, OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_UnionCascaded( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_PointOnSurface( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
/*OGRGeometryH CPL_DLL OGR_G_Polygonize( OGRGeometryH *, int);*/
/*OGRGeometryH CPL_DLL OGR_G_Polygonizer_getCutEdges( OGRGeometryH *, int);*/
/*OGRGeometryH CPL_DLL OGR_G_LineMerge( OGRGeometryH );*/

OGRGeometryH CPL_DLL OGR_G_Difference( OGRGeometryH, OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_SymDifference( OGRGeometryH, OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
double CPL_DLL OGR_G_Distance( OGRGeometryH, OGRGeometryH );
double CPL_DLL OGR_G_Distance3D( OGRGeometryH, OGRGeometryH );
double CPL_DLL OGR_G_Length( OGRGeometryH );
double CPL_DLL OGR_G_Area( OGRGeometryH );
int    CPL_DLL OGR_G_Centroid( OGRGeometryH, OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_Value( OGRGeometryH, double dfDistance ) CPL_WARN_UNUSED_RESULT;

void   CPL_DLL OGR_G_Empty( OGRGeometryH );
int    CPL_DLL OGR_G_IsEmpty( OGRGeometryH );
int    CPL_DLL OGR_G_IsValid( OGRGeometryH );
/*char    CPL_DLL *OGR_G_IsValidReason( OGRGeometryH );*/
OGRGeometryH CPL_DLL OGR_G_MakeValid( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_MakeValidEx( OGRGeometryH, CSLConstList ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_Normalize( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;
int    CPL_DLL OGR_G_IsSimple( OGRGeometryH );
int    CPL_DLL OGR_G_IsRing( OGRGeometryH );

OGRGeometryH CPL_DLL OGR_G_Polygonize( OGRGeometryH ) CPL_WARN_UNUSED_RESULT;

/*! @cond Doxygen_Suppress */
/* backward compatibility (non-standard methods) */
int    CPL_DLL OGR_G_Intersect( OGRGeometryH, OGRGeometryH ) CPL_WARN_DEPRECATED("Non standard method. Use OGR_G_Intersects() instead");
int    CPL_DLL OGR_G_Equal( OGRGeometryH, OGRGeometryH ) CPL_WARN_DEPRECATED("Non standard method. Use OGR_G_Equals() instead");
OGRGeometryH CPL_DLL OGR_G_SymmetricDifference( OGRGeometryH, OGRGeometryH ) CPL_WARN_DEPRECATED("Non standard method. Use OGR_G_SymDifference() instead");
double CPL_DLL OGR_G_GetArea( OGRGeometryH ) CPL_WARN_DEPRECATED("Non standard method. Use OGR_G_Area() instead");
OGRGeometryH CPL_DLL OGR_G_GetBoundary( OGRGeometryH ) CPL_WARN_DEPRECATED("Non standard method. Use OGR_G_Boundary() instead");
/*! @endcond */

/* Methods for getting/setting vertices in points, line strings and rings */
int    CPL_DLL OGR_G_GetPointCount( OGRGeometryH );
int    CPL_DLL OGR_G_GetPoints( OGRGeometryH hGeom,
                                void* pabyX, int nXStride,
                                void* pabyY, int nYStride,
                                void* pabyZ, int nZStride);
int    CPL_DLL OGR_G_GetPointsZM( OGRGeometryH hGeom,
                                  void* pabyX, int nXStride,
                                  void* pabyY, int nYStride,
                                  void* pabyZ, int nZStride,
                                  void* pabyM, int nMStride);
double CPL_DLL OGR_G_GetX( OGRGeometryH, int );
double CPL_DLL OGR_G_GetY( OGRGeometryH, int );
double CPL_DLL OGR_G_GetZ( OGRGeometryH, int );
double CPL_DLL OGR_G_GetM( OGRGeometryH, int );
void   CPL_DLL OGR_G_GetPoint( OGRGeometryH, int iPoint,
                               double *, double *, double * );
void   CPL_DLL OGR_G_GetPointZM( OGRGeometryH, int iPoint,
                                 double *, double *, double *, double * );
void   CPL_DLL OGR_G_SetPointCount( OGRGeometryH hGeom, int nNewPointCount );
void   CPL_DLL OGR_G_SetPoint( OGRGeometryH, int iPoint,
                               double, double, double );
void   CPL_DLL OGR_G_SetPoint_2D( OGRGeometryH, int iPoint,
                                  double, double );
void   CPL_DLL OGR_G_SetPointM( OGRGeometryH, int iPoint,
                                double, double, double );
void   CPL_DLL OGR_G_SetPointZM( OGRGeometryH, int iPoint,
                                 double, double, double, double );
void   CPL_DLL OGR_G_AddPoint( OGRGeometryH, double, double, double );
void   CPL_DLL OGR_G_AddPoint_2D( OGRGeometryH, double, double );
void   CPL_DLL OGR_G_AddPointM( OGRGeometryH, double, double, double );
void   CPL_DLL OGR_G_AddPointZM( OGRGeometryH, double, double, double, double );
void   CPL_DLL OGR_G_SetPoints( OGRGeometryH hGeom, int nPointsIn,
                                const void* pabyX, int nXStride,
                                const void* pabyY, int nYStride,
                                const void* pabyZ, int nZStride );
void   CPL_DLL OGR_G_SetPointsZM( OGRGeometryH hGeom, int nPointsIn,
                                  const void* pabyX, int nXStride,
                                  const void* pabyY, int nYStride,
                                  const void* pabyZ, int nZStride,
                                  const void* pabyM, int nMStride );
void   CPL_DLL OGR_G_SwapXY( OGRGeometryH hGeom );

/* Methods for getting/setting rings and members collections */

int    CPL_DLL OGR_G_GetGeometryCount( OGRGeometryH );
OGRGeometryH CPL_DLL OGR_G_GetGeometryRef( OGRGeometryH, int );
OGRErr CPL_DLL OGR_G_AddGeometry( OGRGeometryH, OGRGeometryH );
OGRErr CPL_DLL OGR_G_AddGeometryDirectly( OGRGeometryH, OGRGeometryH );
OGRErr CPL_DLL OGR_G_RemoveGeometry( OGRGeometryH, int, int );

int CPL_DLL OGR_G_HasCurveGeometry( OGRGeometryH, int bLookForNonLinear );
OGRGeometryH CPL_DLL OGR_G_GetLinearGeometry( OGRGeometryH hGeom,
                                              double dfMaxAngleStepSizeDegrees,
                                              char** papszOptions) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_G_GetCurveGeometry( OGRGeometryH hGeom,
                                             char** papszOptions ) CPL_WARN_UNUSED_RESULT;

OGRGeometryH CPL_DLL
OGRBuildPolygonFromEdges( OGRGeometryH hLinesAsCollection,
                          int bBestEffort,
                          int bAutoClose,
                          double dfTolerance,
                          OGRErr * peErr ) CPL_WARN_UNUSED_RESULT;

/*! @cond Doxygen_Suppress */
OGRErr CPL_DLL OGRSetGenerate_DB2_V72_BYTE_ORDER(
    int bGenerate_DB2_V72_BYTE_ORDER );

int CPL_DLL OGRGetGenerate_DB2_V72_BYTE_ORDER(void);
/*! @endcond */

void CPL_DLL OGRSetNonLinearGeometriesEnabledFlag(int bFlag);
int CPL_DLL OGRGetNonLinearGeometriesEnabledFlag(void);

/** Opaque type for a prepared geometry */
typedef struct _OGRPreparedGeometry * OGRPreparedGeometryH;

int CPL_DLL OGRHasPreparedGeometrySupport(void);
OGRPreparedGeometryH CPL_DLL OGRCreatePreparedGeometry( OGRGeometryH hGeom );
void CPL_DLL OGRDestroyPreparedGeometry( OGRPreparedGeometryH hPreparedGeom );
int CPL_DLL OGRPreparedGeometryIntersects( OGRPreparedGeometryH hPreparedGeom,
                                           OGRGeometryH hOtherGeom );
int CPL_DLL OGRPreparedGeometryContains( OGRPreparedGeometryH hPreparedGeom,
                                         OGRGeometryH hOtherGeom );

/* -------------------------------------------------------------------- */
/*      Feature related (ogr_feature.h)                                 */
/* -------------------------------------------------------------------- */

#ifndef DEFINE_OGRFeatureH
/*! @cond Doxygen_Suppress */
#define DEFINE_OGRFeatureH
/*! @endcond */
#ifdef DEBUG
typedef struct OGRFieldDefnHS   *OGRFieldDefnH;
typedef struct OGRFeatureDefnHS *OGRFeatureDefnH;
typedef struct OGRFeatureHS     *OGRFeatureH;
typedef struct OGRStyleTableHS *OGRStyleTableH;
#else
/** Opaque type for a field definition (OGRFieldDefn) */
typedef void *OGRFieldDefnH;
/** Opaque type for a feature definition (OGRFeatureDefn) */
typedef void *OGRFeatureDefnH;
/** Opaque type for a feature (OGRFeature) */
typedef void *OGRFeatureH;
/** Opaque type for a style table (OGRStyleTable) */
typedef void *OGRStyleTableH;
#endif
/** Opaque type for a geometry field definition (OGRGeomFieldDefn) */
typedef struct OGRGeomFieldDefnHS *OGRGeomFieldDefnH;

/** Opaque type for a field domain definition (OGRFieldDomain) */
typedef struct OGRFieldDomainHS *OGRFieldDomainH;
#endif /* DEFINE_OGRFeatureH */

/* OGRFieldDefn */

OGRFieldDefnH CPL_DLL OGR_Fld_Create( const char *, OGRFieldType ) CPL_WARN_UNUSED_RESULT;
void   CPL_DLL OGR_Fld_Destroy( OGRFieldDefnH );

void   CPL_DLL OGR_Fld_SetName( OGRFieldDefnH, const char * );
const char CPL_DLL *OGR_Fld_GetNameRef( OGRFieldDefnH );
void   CPL_DLL OGR_Fld_SetAlternativeName( OGRFieldDefnH, const char * );
const char CPL_DLL *OGR_Fld_GetAlternativeNameRef( OGRFieldDefnH );
OGRFieldType CPL_DLL OGR_Fld_GetType( OGRFieldDefnH );
void   CPL_DLL OGR_Fld_SetType( OGRFieldDefnH, OGRFieldType );
OGRFieldSubType CPL_DLL OGR_Fld_GetSubType( OGRFieldDefnH );
void   CPL_DLL OGR_Fld_SetSubType( OGRFieldDefnH, OGRFieldSubType );
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
int    CPL_DLL OGR_Fld_IsNullable( OGRFieldDefnH hDefn );
void   CPL_DLL OGR_Fld_SetNullable( OGRFieldDefnH hDefn, int );
int    CPL_DLL OGR_Fld_IsUnique( OGRFieldDefnH hDefn );
void   CPL_DLL OGR_Fld_SetUnique( OGRFieldDefnH hDefn, int );
const char CPL_DLL *OGR_Fld_GetDefault( OGRFieldDefnH hDefn );
void   CPL_DLL OGR_Fld_SetDefault( OGRFieldDefnH hDefn, const char* );
int    CPL_DLL OGR_Fld_IsDefaultDriverSpecific( OGRFieldDefnH hDefn );
const char CPL_DLL* OGR_Fld_GetDomainName( OGRFieldDefnH hDefn );
void   CPL_DLL OGR_Fld_SetDomainName( OGRFieldDefnH hDefn, const char* );

const char CPL_DLL *OGR_GetFieldTypeName( OGRFieldType );
const char CPL_DLL *OGR_GetFieldSubTypeName( OGRFieldSubType );
int CPL_DLL OGR_AreTypeSubTypeCompatible( OGRFieldType eType,
                                          OGRFieldSubType eSubType );

/* OGRGeomFieldDefnH */

OGRGeomFieldDefnH    CPL_DLL OGR_GFld_Create( const char *, OGRwkbGeometryType ) CPL_WARN_UNUSED_RESULT;
void                 CPL_DLL OGR_GFld_Destroy( OGRGeomFieldDefnH );

void                 CPL_DLL OGR_GFld_SetName( OGRGeomFieldDefnH, const char * );
const char           CPL_DLL *OGR_GFld_GetNameRef( OGRGeomFieldDefnH );

OGRwkbGeometryType   CPL_DLL OGR_GFld_GetType( OGRGeomFieldDefnH );
void                 CPL_DLL OGR_GFld_SetType( OGRGeomFieldDefnH, OGRwkbGeometryType );

OGRSpatialReferenceH CPL_DLL OGR_GFld_GetSpatialRef( OGRGeomFieldDefnH );
void                 CPL_DLL OGR_GFld_SetSpatialRef( OGRGeomFieldDefnH,
                                                     OGRSpatialReferenceH hSRS );

int                  CPL_DLL OGR_GFld_IsNullable( OGRGeomFieldDefnH hDefn );
void                 CPL_DLL OGR_GFld_SetNullable( OGRGeomFieldDefnH hDefn, int );

int                  CPL_DLL OGR_GFld_IsIgnored( OGRGeomFieldDefnH hDefn );
void                 CPL_DLL OGR_GFld_SetIgnored( OGRGeomFieldDefnH hDefn, int );

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
OGRErr CPL_DLL OGR_FD_ReorderFieldDefns( OGRFeatureDefnH hDefn, const int* panMap );
OGRwkbGeometryType CPL_DLL OGR_FD_GetGeomType( OGRFeatureDefnH );
void   CPL_DLL OGR_FD_SetGeomType( OGRFeatureDefnH, OGRwkbGeometryType );
int    CPL_DLL OGR_FD_IsGeometryIgnored( OGRFeatureDefnH );
void   CPL_DLL OGR_FD_SetGeometryIgnored( OGRFeatureDefnH, int );
int    CPL_DLL OGR_FD_IsStyleIgnored( OGRFeatureDefnH );
void   CPL_DLL OGR_FD_SetStyleIgnored( OGRFeatureDefnH, int );
int    CPL_DLL OGR_FD_Reference( OGRFeatureDefnH );
int    CPL_DLL OGR_FD_Dereference( OGRFeatureDefnH );
int    CPL_DLL OGR_FD_GetReferenceCount( OGRFeatureDefnH );

int               CPL_DLL OGR_FD_GetGeomFieldCount( OGRFeatureDefnH hFDefn );
OGRGeomFieldDefnH CPL_DLL OGR_FD_GetGeomFieldDefn( OGRFeatureDefnH hFDefn,
                                                   int i );
int               CPL_DLL OGR_FD_GetGeomFieldIndex( OGRFeatureDefnH hFDefn,
                                                    const char *pszName);

void              CPL_DLL OGR_FD_AddGeomFieldDefn( OGRFeatureDefnH hFDefn,
                                                   OGRGeomFieldDefnH hGFldDefn);
OGRErr            CPL_DLL OGR_FD_DeleteGeomFieldDefn( OGRFeatureDefnH hFDefn,
                                                      int iGeomField );
int               CPL_DLL OGR_FD_IsSame( OGRFeatureDefnH hFDefn,
                                         OGRFeatureDefnH hOtherFDefn );
/* OGRFeature */

OGRFeatureH CPL_DLL OGR_F_Create( OGRFeatureDefnH ) CPL_WARN_UNUSED_RESULT;
void   CPL_DLL OGR_F_Destroy( OGRFeatureH );
OGRFeatureDefnH CPL_DLL OGR_F_GetDefnRef( OGRFeatureH );

OGRErr CPL_DLL OGR_F_SetGeometryDirectly( OGRFeatureH, OGRGeometryH );
OGRErr CPL_DLL OGR_F_SetGeometry( OGRFeatureH, OGRGeometryH );
OGRGeometryH CPL_DLL OGR_F_GetGeometryRef( OGRFeatureH );
OGRGeometryH CPL_DLL OGR_F_StealGeometry( OGRFeatureH ) CPL_WARN_UNUSED_RESULT;
OGRGeometryH CPL_DLL OGR_F_StealGeometryEx( OGRFeatureH, int iGeomField ) CPL_WARN_UNUSED_RESULT;
OGRFeatureH CPL_DLL OGR_F_Clone( OGRFeatureH ) CPL_WARN_UNUSED_RESULT;
int    CPL_DLL OGR_F_Equal( OGRFeatureH, OGRFeatureH );

int    CPL_DLL OGR_F_GetFieldCount( OGRFeatureH );
OGRFieldDefnH CPL_DLL OGR_F_GetFieldDefnRef( OGRFeatureH, int );
int    CPL_DLL OGR_F_GetFieldIndex( OGRFeatureH, const char * );

int    CPL_DLL OGR_F_IsFieldSet( OGRFeatureH, int );
void   CPL_DLL OGR_F_UnsetField( OGRFeatureH, int );

int    CPL_DLL OGR_F_IsFieldNull( OGRFeatureH, int );
int    CPL_DLL OGR_F_IsFieldSetAndNotNull( OGRFeatureH, int );
void   CPL_DLL OGR_F_SetFieldNull( OGRFeatureH, int );

OGRField CPL_DLL *OGR_F_GetRawFieldRef( OGRFeatureH, int );

int    CPL_DLL OGR_RawField_IsUnset( const OGRField* );
int    CPL_DLL OGR_RawField_IsNull( const OGRField* );
void   CPL_DLL OGR_RawField_SetUnset( OGRField* );
void   CPL_DLL OGR_RawField_SetNull( OGRField* );

int    CPL_DLL OGR_F_GetFieldAsInteger( OGRFeatureH, int );
GIntBig CPL_DLL OGR_F_GetFieldAsInteger64( OGRFeatureH, int );
double CPL_DLL OGR_F_GetFieldAsDouble( OGRFeatureH, int );
const char CPL_DLL *OGR_F_GetFieldAsString( OGRFeatureH, int );
const int CPL_DLL *OGR_F_GetFieldAsIntegerList( OGRFeatureH, int, int * );
const GIntBig CPL_DLL *OGR_F_GetFieldAsInteger64List( OGRFeatureH, int, int * );
const double CPL_DLL *OGR_F_GetFieldAsDoubleList( OGRFeatureH, int, int * );
char  CPL_DLL **OGR_F_GetFieldAsStringList( OGRFeatureH, int );
GByte CPL_DLL *OGR_F_GetFieldAsBinary( OGRFeatureH, int, int * );
int   CPL_DLL  OGR_F_GetFieldAsDateTime( OGRFeatureH, int, int *, int *, int *,
                                         int *, int *, int *, int * );
int   CPL_DLL OGR_F_GetFieldAsDateTimeEx( OGRFeatureH hFeat, int iField,
                                int *pnYear, int *pnMonth, int *pnDay,
                                int *pnHour, int *pnMinute, float *pfSecond,
                                int *pnTZFlag );

void   CPL_DLL OGR_F_SetFieldInteger( OGRFeatureH, int, int );
void   CPL_DLL OGR_F_SetFieldInteger64( OGRFeatureH, int, GIntBig );
void   CPL_DLL OGR_F_SetFieldDouble( OGRFeatureH, int, double );
void   CPL_DLL OGR_F_SetFieldString( OGRFeatureH, int, const char * );
void   CPL_DLL OGR_F_SetFieldIntegerList( OGRFeatureH, int, int, const int * );
void   CPL_DLL OGR_F_SetFieldInteger64List( OGRFeatureH, int, int, const GIntBig * );
void   CPL_DLL OGR_F_SetFieldDoubleList( OGRFeatureH, int, int, const double * );
void   CPL_DLL OGR_F_SetFieldStringList( OGRFeatureH, int, CSLConstList );
void   CPL_DLL OGR_F_SetFieldRaw( OGRFeatureH, int, const OGRField * );
void   CPL_DLL OGR_F_SetFieldBinary( OGRFeatureH, int, int, const void * );
void   CPL_DLL OGR_F_SetFieldDateTime( OGRFeatureH, int,
                                       int, int, int, int, int, int, int );
void   CPL_DLL OGR_F_SetFieldDateTimeEx( OGRFeatureH, int,
                                       int, int, int, int, int, float, int );

int               CPL_DLL OGR_F_GetGeomFieldCount( OGRFeatureH hFeat );
OGRGeomFieldDefnH CPL_DLL OGR_F_GetGeomFieldDefnRef( OGRFeatureH hFeat,
                                                     int iField );
int               CPL_DLL OGR_F_GetGeomFieldIndex( OGRFeatureH hFeat,
                                                   const char *pszName);

OGRGeometryH      CPL_DLL OGR_F_GetGeomFieldRef( OGRFeatureH hFeat,
                                                 int iField );
OGRErr            CPL_DLL OGR_F_SetGeomFieldDirectly( OGRFeatureH hFeat,
                                                      int iField,
                                                      OGRGeometryH hGeom );
OGRErr            CPL_DLL OGR_F_SetGeomField( OGRFeatureH hFeat,
                                              int iField, OGRGeometryH hGeom );

GIntBig CPL_DLL OGR_F_GetFID( OGRFeatureH );
OGRErr CPL_DLL OGR_F_SetFID( OGRFeatureH, GIntBig );
void   CPL_DLL OGR_F_DumpReadable( OGRFeatureH, FILE * );
OGRErr CPL_DLL OGR_F_SetFrom( OGRFeatureH, OGRFeatureH, int );
OGRErr CPL_DLL OGR_F_SetFromWithMap( OGRFeatureH, OGRFeatureH, int , const int * );

const char CPL_DLL *OGR_F_GetStyleString( OGRFeatureH );
void   CPL_DLL OGR_F_SetStyleString( OGRFeatureH, const char * );
void   CPL_DLL OGR_F_SetStyleStringDirectly( OGRFeatureH, char * );
/** Return style table */
OGRStyleTableH CPL_DLL OGR_F_GetStyleTable( OGRFeatureH );
/** Set style table and take ownership */
void   CPL_DLL OGR_F_SetStyleTableDirectly( OGRFeatureH, OGRStyleTableH );
/** Set style table */
void   CPL_DLL OGR_F_SetStyleTable( OGRFeatureH, OGRStyleTableH );

const char CPL_DLL *OGR_F_GetNativeData( OGRFeatureH );
void CPL_DLL OGR_F_SetNativeData( OGRFeatureH, const char* );
const char CPL_DLL *OGR_F_GetNativeMediaType( OGRFeatureH );
void CPL_DLL OGR_F_SetNativeMediaType( OGRFeatureH, const char* );

void   CPL_DLL OGR_F_FillUnsetWithDefault( OGRFeatureH hFeat,
                                           int bNotNullableOnly,
                                           char** papszOptions );
int    CPL_DLL OGR_F_Validate( OGRFeatureH, int nValidateFlags, int bEmitError );

/* OGRFieldDomain */

void CPL_DLL OGR_FldDomain_Destroy(OGRFieldDomainH);
const char CPL_DLL* OGR_FldDomain_GetName(OGRFieldDomainH);
const char CPL_DLL* OGR_FldDomain_GetDescription(OGRFieldDomainH);
OGRFieldDomainType CPL_DLL OGR_FldDomain_GetDomainType(OGRFieldDomainH);
OGRFieldType CPL_DLL OGR_FldDomain_GetFieldType(OGRFieldDomainH);
OGRFieldSubType CPL_DLL OGR_FldDomain_GetFieldSubType(OGRFieldDomainH);
OGRFieldDomainSplitPolicy CPL_DLL OGR_FldDomain_GetSplitPolicy(OGRFieldDomainH);
void CPL_DLL OGR_FldDomain_SetSplitPolicy(OGRFieldDomainH, OGRFieldDomainSplitPolicy);
OGRFieldDomainMergePolicy CPL_DLL OGR_FldDomain_GetMergePolicy(OGRFieldDomainH);
void CPL_DLL OGR_FldDomain_SetMergePolicy(OGRFieldDomainH, OGRFieldDomainMergePolicy);

OGRFieldDomainH CPL_DLL OGR_CodedFldDomain_Create(const char* pszName,
                                                  const char* pszDescription,
                                                  OGRFieldType eFieldType,
                                                  OGRFieldSubType eFieldSubType,
                                                  const OGRCodedValue* enumeration);
const OGRCodedValue CPL_DLL* OGR_CodedFldDomain_GetEnumeration(OGRFieldDomainH);

OGRFieldDomainH CPL_DLL OGR_RangeFldDomain_Create(const char* pszName,
                                                  const char* pszDescription,
                                                  OGRFieldType eFieldType,
                                                  OGRFieldSubType eFieldSubType,
                                                  const OGRField* psMin,
                                                  bool bMinIsInclusive,
                                                  const OGRField* psMax,
                                                  bool bMaxIsInclusive);
const OGRField CPL_DLL *OGR_RangeFldDomain_GetMin(OGRFieldDomainH, bool* pbIsInclusiveOut);
const OGRField CPL_DLL *OGR_RangeFldDomain_GetMax(OGRFieldDomainH, bool* pbIsInclusiveOut);

OGRFieldDomainH CPL_DLL OGR_GlobFldDomain_Create(const char* pszName,
                                                  const char* pszDescription,
                                                  OGRFieldType eFieldType,
                                                  OGRFieldSubType eFieldSubType,
                                                  const char* pszGlob);
const char CPL_DLL *OGR_GlobFldDomain_GetGlob(OGRFieldDomainH);

/* -------------------------------------------------------------------- */
/*      ogrsf_frmts.h                                                   */
/* -------------------------------------------------------------------- */

#ifdef DEBUG
typedef struct OGRLayerHS      *OGRLayerH;
typedef struct OGRDataSourceHS *OGRDataSourceH;
typedef struct OGRDriverHS     *OGRSFDriverH;
#else
/** Opaque type for a layer (OGRLayer) */
typedef void *OGRLayerH;
/** Opaque type for a OGR datasource (OGRDataSource) */
typedef void *OGRDataSourceH;
/** Opaque type for a OGR driver (OGRSFDriver) */
typedef void *OGRSFDriverH;
#endif

/* OGRLayer */

const char CPL_DLL* OGR_L_GetName( OGRLayerH );
OGRwkbGeometryType CPL_DLL OGR_L_GetGeomType( OGRLayerH );
OGRGeometryH CPL_DLL OGR_L_GetSpatialFilter( OGRLayerH );
void   CPL_DLL OGR_L_SetSpatialFilter( OGRLayerH, OGRGeometryH );
void   CPL_DLL OGR_L_SetSpatialFilterRect( OGRLayerH,
                                           double, double, double, double );
void     CPL_DLL OGR_L_SetSpatialFilterEx( OGRLayerH, int iGeomField,
                                           OGRGeometryH hGeom );
void     CPL_DLL OGR_L_SetSpatialFilterRectEx( OGRLayerH, int iGeomField,
                                               double dfMinX, double dfMinY,
                                               double dfMaxX, double dfMaxY );
OGRErr CPL_DLL OGR_L_SetAttributeFilter( OGRLayerH, const char * );
void   CPL_DLL OGR_L_ResetReading( OGRLayerH );
OGRFeatureH CPL_DLL OGR_L_GetNextFeature( OGRLayerH ) CPL_WARN_UNUSED_RESULT;

/** Conveniency macro to iterate over features of a layer.
 *
 * Typical usage is:
 * <pre>
 * OGR_FOR_EACH_FEATURE_BEGIN(hFeat, hLayer)
 * {
 *      // Do something, including continue, break;
 *      // Do not explicitly destroy the feature (unless you use return or goto
 *      // outside of the loop, in which case use OGR_F_Destroy(hFeat))
 * }
 * OGR_FOR_EACH_FEATURE_END(hFeat)
 * </pre>
 *
 * In C++, you might want to use instead range-based loop:
 * <pre>
 * for( auto&& poFeature: poLayer )
 * {
 * }
 * </pre>
 *
 * @param hFeat variable name for OGRFeatureH. The variable will be declared
 *              inside the macro body.
 * @param hLayer layer to iterate over.
 *
 * @since GDAL 2.3
 */
#define OGR_FOR_EACH_FEATURE_BEGIN(hFeat, hLayer) \
    { \
        OGRFeatureH hFeat = CPL_NULLPTR; \
        OGR_L_ResetReading(hLayer); \
        while( true) \
        { \
            if( hFeat ) \
                OGR_F_Destroy(hFeat); \
            hFeat = OGR_L_GetNextFeature(hLayer); \
            if( !hFeat ) \
                break;

/** End of iterator. */
#define OGR_FOR_EACH_FEATURE_END(hFeat) \
        } \
        OGR_F_Destroy(hFeat); \
    }

/** Data type for a Arrow C stream Include ogr_recordbatch.h to get the definition. */
struct ArrowArrayStream;

bool CPL_DLL OGR_L_GetArrowStream(OGRLayerH hLayer,
                                  struct ArrowArrayStream* out_stream,
                                  char** papszOptions);

OGRErr CPL_DLL OGR_L_SetNextByIndex( OGRLayerH, GIntBig );
OGRFeatureH CPL_DLL OGR_L_GetFeature( OGRLayerH, GIntBig )  CPL_WARN_UNUSED_RESULT;
OGRErr CPL_DLL OGR_L_SetFeature( OGRLayerH, OGRFeatureH ) CPL_WARN_UNUSED_RESULT;
OGRErr CPL_DLL OGR_L_CreateFeature( OGRLayerH, OGRFeatureH ) CPL_WARN_UNUSED_RESULT;
OGRErr CPL_DLL OGR_L_DeleteFeature( OGRLayerH, GIntBig ) CPL_WARN_UNUSED_RESULT;
OGRFeatureDefnH CPL_DLL OGR_L_GetLayerDefn( OGRLayerH );
OGRSpatialReferenceH CPL_DLL OGR_L_GetSpatialRef( OGRLayerH );
int    CPL_DLL OGR_L_FindFieldIndex( OGRLayerH, const char *, int bExactMatch );
GIntBig CPL_DLL OGR_L_GetFeatureCount( OGRLayerH, int );
OGRErr CPL_DLL OGR_L_GetExtent( OGRLayerH, OGREnvelope *, int );
OGRErr  CPL_DLL OGR_L_GetExtentEx( OGRLayerH, int iGeomField,
                                   OGREnvelope *psExtent, int bForce );
int    CPL_DLL OGR_L_TestCapability( OGRLayerH, const char * );
OGRErr CPL_DLL OGR_L_CreateField( OGRLayerH, OGRFieldDefnH, int );
OGRErr CPL_DLL OGR_L_CreateGeomField( OGRLayerH hLayer,
                                      OGRGeomFieldDefnH hFieldDefn, int bForce );
OGRErr CPL_DLL OGR_L_DeleteField( OGRLayerH, int iField );
OGRErr CPL_DLL OGR_L_ReorderFields( OGRLayerH, int* panMap );
OGRErr CPL_DLL OGR_L_ReorderField( OGRLayerH, int iOldFieldPos, int iNewFieldPos );
OGRErr CPL_DLL OGR_L_AlterFieldDefn( OGRLayerH, int iField, OGRFieldDefnH hNewFieldDefn, int nFlags );
OGRErr CPL_DLL OGR_L_AlterGeomFieldDefn( OGRLayerH, int iField, OGRGeomFieldDefnH hNewGeomFieldDefn, int nFlags );
OGRErr CPL_DLL OGR_L_StartTransaction( OGRLayerH )  CPL_WARN_UNUSED_RESULT;
OGRErr CPL_DLL OGR_L_CommitTransaction( OGRLayerH )  CPL_WARN_UNUSED_RESULT;
OGRErr CPL_DLL OGR_L_RollbackTransaction( OGRLayerH );
OGRErr CPL_DLL OGR_L_Rename( OGRLayerH hLayer, const char* pszNewName );

/*! @cond Doxygen_Suppress */
int    CPL_DLL OGR_L_Reference( OGRLayerH );
int    CPL_DLL OGR_L_Dereference( OGRLayerH );
int    CPL_DLL OGR_L_GetRefCount( OGRLayerH );
/*! @endcond */
OGRErr CPL_DLL OGR_L_SyncToDisk( OGRLayerH );
/*! @cond Doxygen_Suppress */
GIntBig CPL_DLL OGR_L_GetFeaturesRead( OGRLayerH );
/*! @endcond */
const char CPL_DLL *OGR_L_GetFIDColumn( OGRLayerH );
const char CPL_DLL *OGR_L_GetGeometryColumn( OGRLayerH );
/** Get style table */
OGRStyleTableH CPL_DLL OGR_L_GetStyleTable( OGRLayerH );
/** Set style table (and take ownership) */
void   CPL_DLL OGR_L_SetStyleTableDirectly( OGRLayerH, OGRStyleTableH );
/** Set style table */
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
/*! @cond Doxygen_Suppress */
int    CPL_DLL OGR_DS_Reference( OGRDataSourceH );
int    CPL_DLL OGR_DS_Dereference( OGRDataSourceH );
int    CPL_DLL OGR_DS_GetRefCount( OGRDataSourceH );
int    CPL_DLL OGR_DS_GetSummaryRefCount( OGRDataSourceH );
/*! @endcond */
/** Flush pending changes to disk. See GDALDataset::FlushCache() */
OGRErr CPL_DLL OGR_DS_SyncToDisk( OGRDataSourceH );
/** Get style table */
OGRStyleTableH CPL_DLL OGR_DS_GetStyleTable( OGRDataSourceH );
/** Set style table (and take ownership) */
void   CPL_DLL OGR_DS_SetStyleTableDirectly( OGRDataSourceH, OGRStyleTableH );
/** Set style table */
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
/*! @cond Doxygen_Suppress */
void    CPL_DLL OGRRegisterDriver( OGRSFDriverH );
void    CPL_DLL OGRDeregisterDriver( OGRSFDriverH );
/*! @endcond */
int     CPL_DLL OGRGetDriverCount(void);
OGRSFDriverH CPL_DLL OGRGetDriver( int );
OGRSFDriverH CPL_DLL OGRGetDriverByName( const char * );
/*! @cond Doxygen_Suppress */
int     CPL_DLL OGRGetOpenDSCount(void);
OGRDataSourceH CPL_DLL OGRGetOpenDS( int iDS );
/*! @endcond */

void CPL_DLL OGRRegisterAll(void);

/** Clean-up all drivers (including raster ones starting with GDAL 2.0.
 * See GDALDestroyDriverManager() */
void CPL_DLL OGRCleanupAll(void);

/* -------------------------------------------------------------------- */
/*      ogrsf_featurestyle.h                                            */
/* -------------------------------------------------------------------- */

#ifdef DEBUG
typedef struct OGRStyleMgrHS *OGRStyleMgrH;
typedef struct OGRStyleToolHS *OGRStyleToolH;
#else
/** Style manager opaque type */
typedef void *OGRStyleMgrH;
/** Style tool opaque type */
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
int     CPL_DLL OGR_STBL_AddStyle( OGRStyleTableH hStyleTable,
                                   const char *pszName,
                                   const char *pszStyleString);
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
