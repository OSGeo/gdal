/******************************************************************************
 * $Id$
 *
 * Name:     gdal.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
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

%include constraints.i

#ifdef PERL_CPAN_NAMESPACE
%module "Geo::GDAL"
#elif defined(SWIGCSHARP)
%module Gdal
#else
%module gdal
#endif

#ifdef SWIGCSHARP
%include swig_csharp_extensions.i
#endif

#ifndef SWIGJAVA
%feature ("compactdefaultargs");
#endif

//
// We register all the drivers upon module initialization
//

%{
#include <iostream>
using namespace std;

#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "cpl_http.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdalwarper.h"

typedef void GDALMajorObjectShadow;
typedef void GDALDriverShadow;
typedef void GDALDatasetShadow;
typedef void GDALRasterBandShadow;
typedef void GDALColorTableShadow;
typedef void GDALRasterAttributeTableShadow;
typedef void GDALTransformerInfoShadow;
typedef void GDALAsyncReaderShadow;
%}

#if defined(SWIGPYTHON) || defined(SWIGJAVA)
%{
#ifdef DEBUG 
typedef struct OGRSpatialReferenceHS OSRSpatialReferenceShadow;
typedef struct OGRLayerHS OGRLayerShadow;
typedef struct OGRGeometryHS OGRGeometryShadow;
#else
typedef void OSRSpatialReferenceShadow;
typedef void OGRLayerShadow;
typedef void OGRGeometryShadow;
#endif
typedef struct OGRStyleTableHS OGRStyleTableShadow;
%}
#endif /* #if defined(SWIGPYTHON) || defined(SWIGJAVA) */

#if defined(SWIGCSHARP)
typedef int OGRErr;
#endif

%{
/* use this to not return the int returned by GDAL */
typedef int RETURN_NONE;
/* return value that is used for VSI methods that return -1 on error (and set errno) */
typedef int VSI_RETVAL;
%}
typedef int RETURN_NONE;

//************************************************************************
//
// Enums.
//
//************************************************************************

typedef int GDALTileOrganization;
#ifndef SWIGCSHARP
typedef int GDALPaletteInterp;
typedef int GDALColorInterp;
typedef int GDALAccess;
typedef int GDALDataType;
typedef int CPLErr;
typedef int GDALResampleAlg;
typedef int GDALAsyncStatusType;
typedef int GDALRWFlag;
typedef int GDALRIOResampleAlg;
#else
/*! Pixel data types */
%rename (DataType) GDALDataType;
typedef enum {
    GDT_Unknown = 0,
    /*! Eight bit unsigned integer */           GDT_Byte = 1,
    /*! Sixteen bit unsigned integer */         GDT_UInt16 = 2,
    /*! Sixteen bit signed integer */           GDT_Int16 = 3,
    /*! Thirty two bit unsigned integer */      GDT_UInt32 = 4,
    /*! Thirty two bit signed integer */        GDT_Int32 = 5,
    /*! Thirty two bit floating point */        GDT_Float32 = 6,
    /*! Sixty four bit floating point */        GDT_Float64 = 7,
    /*! Complex Int16 */                        GDT_CInt16 = 8,
    /*! Complex Int32 */                        GDT_CInt32 = 9,
    /*! Complex Float32 */                      GDT_CFloat32 = 10,
    /*! Complex Float64 */                      GDT_CFloat64 = 11,
    GDT_TypeCount = 12          /* maximum type # + 1 */
} GDALDataType;

/*! Types of color interpretation for raster bands. */
%rename (ColorInterp) GDALColorInterp;
typedef enum
{
    GCI_Undefined=0,
    /*! Greyscale */                                      GCI_GrayIndex=1,
    /*! Paletted (see associated color table) */          GCI_PaletteIndex=2,
    /*! Red band of RGBA image */                         GCI_RedBand=3,
    /*! Green band of RGBA image */                       GCI_GreenBand=4,
    /*! Blue band of RGBA image */                        GCI_BlueBand=5,
    /*! Alpha (0=transparent, 255=opaque) */              GCI_AlphaBand=6,
    /*! Hue band of HLS image */                          GCI_HueBand=7,
    /*! Saturation band of HLS image */                   GCI_SaturationBand=8,
    /*! Lightness band of HLS image */                    GCI_LightnessBand=9,
    /*! Cyan band of CMYK image */                        GCI_CyanBand=10,
    /*! Magenta band of CMYK image */                     GCI_MagentaBand=11,
    /*! Yellow band of CMYK image */                      GCI_YellowBand=12,
    /*! Black band of CMLY image */                       GCI_BlackBand=13,
    /*! Y Luminance */                                    GCI_YCbCr_YBand=14,
    /*! Cb Chroma */                                      GCI_YCbCr_CbBand=15,
    /*! Cr Chroma */                                      GCI_YCbCr_CrBand=16,
    /*! Max current value */                              GCI_Max=16
} GDALColorInterp;

/*! Types of color interpretations for a GDALColorTable. */
%rename (PaletteInterp) GDALPaletteInterp;
typedef enum 
{
  /*! Grayscale (in GDALColorEntry.c1) */                      GPI_Gray=0,
  /*! Red, Green, Blue and Alpha in (in c1, c2, c3 and c4) */  GPI_RGB=1,
  /*! Cyan, Magenta, Yellow and Black (in c1, c2, c3 and c4)*/ GPI_CMYK=2,
  /*! Hue, Lightness and Saturation (in c1, c2, and c3) */     GPI_HLS=3
} GDALPaletteInterp;

/*! Flag indicating read/write, or read-only access to data. */
%rename (Access) GDALAccess;
typedef enum {
    /*! Read only (no update) access */ GA_ReadOnly = 0,
    /*! Read/write access. */           GA_Update = 1
} GDALAccess;

/*! Read/Write flag for RasterIO() method */
%rename (RWFlag) GDALRWFlag;
typedef enum {
    /*! Read data */   GF_Read = 0,
    /*! Write data */  GF_Write = 1
} GDALRWFlag;

%rename (RIOResampleAlg) GDALRIOResampleAlg;
typedef enum
{
    /*! Nearest neighbour */                GRIORA_NearestNeighbour = 0,
    /*! Bilinear (2x2 kernel) */            GRIORA_Bilinear = 1,
    /*! Cubic Convolution Approximation  */ GRIORA_Cubic = 2,
    /*! Cubic B-Spline Approximation */     GRIORA_CubicSpline = 3,
    /*! Lanczos windowed sinc interpolation (6x6 kernel) */ GRIORA_Lanczos = 4,
    /*! Average */                          GRIORA_Average = 5,
    /*! Mode (selects the value which appears most often of all the sampled points) */
                                            GRIORA_Mode = 6,
    /*! Gauss bluring */                    GRIORA_Gauss = 7
} GDALRIOResampleAlg;

/*! Warp Resampling Algorithm */
%rename (ResampleAlg) GDALResampleAlg;
typedef enum {
  /*! Nearest neighbour (select on one input pixel) */ GRA_NearestNeighbour=0,
  /*! Bilinear (2x2 kernel) */                         GRA_Bilinear=1,
  /*! Cubic Convolution Approximation (4x4 kernel) */  GRA_Cubic=2,
  /*! Cubic B-Spline Approximation (4x4 kernel) */     GRA_CubicSpline=3,
  /*! Lanczos windowed sinc interpolation (6x6 kernel) */ GRA_Lanczos=4,
  /*! Average (computes the average of all non-NODATA contributing pixels) */ GRA_Average=5, 
  /*! Mode (selects the value which appears most often of all the sampled points) */ GRA_Mode=6
} GDALResampleAlg;

%rename (AsyncStatusType) GDALAsyncStatusType;
typedef enum {
	GARIO_PENDING = 0,
	GARIO_UPDATE = 1,
	GARIO_ERROR = 2,
	GARIO_COMPLETE = 3
} GDALAsyncStatusType;
#endif

#if defined(SWIGPYTHON)
%include "gdal_python.i"
#elif defined(SWIGRUBY)
%include "gdal_ruby.i"
#elif defined(SWIGPHP4)
%include "gdal_php.i"
#elif defined(SWIGCSHARP)
%include "gdal_csharp.i"
#elif defined(SWIGPERL)
%include "gdal_perl.i"
#elif defined(SWIGJAVA)
%include "gdal_java.i"
#else
%include "gdal_typemaps.i"
#endif


/* Default memberin typemaps required to support SWIG 1.3.39 and above */
%typemap(memberin) char *Info %{
/* char* Info memberin typemap */
$1;
%}

%typemap(memberin) char *Id %{
/* char* Info memberin typemap */
$1;
%}

//************************************************************************
// Apply NONNULL to all utf8_path's. 
%apply Pointer NONNULL { const char* utf8_path };

//************************************************************************
//
// Define the exposed CPL functions.
//
//************************************************************************
%include "cpl.i"

//************************************************************************
//
// Define the XMLNode object
//
//************************************************************************
#if defined(SWIGCSHARP) || defined(SWIGJAVA)
%include "XMLNode.i"
#endif

//************************************************************************
//
// Define the MajorObject object
//
//************************************************************************
%include "MajorObject.i"

//************************************************************************
//
// Define the Driver object.
//
//************************************************************************
%include "Driver.i"



#if defined(SWIGPYTHON) || defined(SWIGJAVA) || defined(SWIGPERL)
/*
 * We need to import ogr.i and osr.i for OGRLayer and OSRSpatialRefrerence
 */
#define FROM_GDAL_I
%import ogr.i
#endif /* #if defined(SWIGPYTHON) || defined(SWIGJAVA) */


//************************************************************************
//
// Define renames.
//
//************************************************************************
%rename (GCP) GDAL_GCP;

#ifdef SWIGRUBY
%rename (all_register) GDALAllRegister;
%rename (get_cache_max) wrapper_GDALGetCacheMax;
%rename (set_cache_max) wrapper_GDALSetCacheMax;
%rename (get_cache_used) wrapper_GDALGetCacheUsed;
%rename (get_data_type_size) GDALGetDataTypeSize;
%rename (data_type_is_complex) GDALDataTypeIsComplex;
%rename (gcps_to_geo_transform) GDALGCPsToGeoTransform;
%rename (get_data_type_name) GDALGetDataTypeName;
%rename (get_data_type_by_name) GDALGetDataTypeByName;
%rename (get_color_interpretation_name) GDALGetColorInterpretationName;
%rename (get_palette_interpretation_name) GDALGetPaletteInterpretationName;
%rename (dec_to_dms) GDALDecToDMS;
%rename (packed_dms_to_dec) GDALPackedDMSToDec;
%rename (dec_to_packed_dms) GDALDecToPackedDMS;
%rename (parse_xml_string) CPLParseXMLString;
%rename (serialize_xml_tree) CPLSerializeXMLTree;
#else
%rename (GCP) GDAL_GCP;
%rename (GCPsToGeoTransform) GDALGCPsToGeoTransform;
%rename (ApplyGeoTransform) GDALApplyGeoTransform;
%rename (InvGeoTransform) GDALInvGeoTransform;
%rename (VersionInfo) GDALVersionInfo;
%rename (AllRegister) GDALAllRegister;
%rename (GetCacheMax) wrapper_GDALGetCacheMax;
%rename (SetCacheMax) wrapper_GDALSetCacheMax;
%rename (GetCacheUsed) wrapper_GDALGetCacheUsed;
%rename (GetDataTypeSize) GDALGetDataTypeSize;
%rename (DataTypeIsComplex) GDALDataTypeIsComplex;
%rename (GetDataTypeName) GDALGetDataTypeName;
%rename (GetDataTypeByName) GDALGetDataTypeByName;
%rename (GetColorInterpretationName) GDALGetColorInterpretationName;
%rename (GetPaletteInterpretationName) GDALGetPaletteInterpretationName;
%rename (DecToDMS) GDALDecToDMS;
%rename (PackedDMSToDec) GDALPackedDMSToDec;
%rename (DecToPackedDMS) GDALDecToPackedDMS;
%rename (ParseXMLString) CPLParseXMLString;
%rename (SerializeXMLTree) CPLSerializeXMLTree;
%rename (GetJPEG2000Structure) GDALGetJPEG2000Structure;
#endif
#ifdef SWIGPERL
%include "gdal_perl_rename.i"
#endif


//************************************************************************
//
// GDALColorEntry
//
//************************************************************************
#if !defined(SWIGPERL) && !defined(SWIGJAVA)
%rename (ColorEntry) GDALColorEntry;
typedef struct
{
    /*! gray, red, cyan or hue */
    short      c1;      
    /*! green, magenta, or lightness */    
    short      c2;      
    /*! blue, yellow, or saturation */
    short      c3;      
    /*! alpha or blackband */
    short      c4;      
} GDALColorEntry;
#endif

//************************************************************************
//
// Define the Ground Control Point structure.
//
//************************************************************************
// GCP - class?  serialize() method missing.
struct GDAL_GCP {
%extend {
%mutable;
  double GCPX;
  double GCPY;
  double GCPZ;
  double GCPPixel;
  double GCPLine;
  char *Info;
  char *Id;
%immutable;

#ifdef SWIGJAVA
  GDAL_GCP( double x, double y, double z,
            double pixel, double line,
            const char *info, const char *id) {
#else
  GDAL_GCP( double x = 0.0, double y = 0.0, double z = 0.0,
            double pixel = 0.0, double line = 0.0,
            const char *info = "", const char *id = "" ) {
#endif
    GDAL_GCP *self = (GDAL_GCP*) CPLMalloc( sizeof( GDAL_GCP ) );
    self->dfGCPX = x;
    self->dfGCPY = y;
    self->dfGCPZ = z;
    self->dfGCPPixel = pixel;
    self->dfGCPLine = line;
    self->pszInfo =  CPLStrdup( (info == 0) ? "" : info );
    self->pszId = CPLStrdup( (id==0)? "" : id );
    return self;
  }

  ~GDAL_GCP() {
    if ( self->pszInfo )
      CPLFree( self->pszInfo );
    if ( self->pszId )
      CPLFree( self->pszId );
    CPLFree( self );
  }


} /* extend */
}; /* GDAL_GCP */

%apply Pointer NONNULL {GDAL_GCP *gcp};
%inline %{

double GDAL_GCP_GCPX_get( GDAL_GCP *gcp ) {
  return gcp->dfGCPX;
}
void GDAL_GCP_GCPX_set( GDAL_GCP *gcp, double dfGCPX ) {
  gcp->dfGCPX = dfGCPX;
}
double GDAL_GCP_GCPY_get( GDAL_GCP *gcp ) {
  return gcp->dfGCPY;
}
void GDAL_GCP_GCPY_set( GDAL_GCP *gcp, double dfGCPY ) {
  gcp->dfGCPY = dfGCPY;
}
double GDAL_GCP_GCPZ_get( GDAL_GCP *gcp ) {
  return gcp->dfGCPZ;
}
void GDAL_GCP_GCPZ_set( GDAL_GCP *gcp, double dfGCPZ ) {
  gcp->dfGCPZ = dfGCPZ;
}
double GDAL_GCP_GCPPixel_get( GDAL_GCP *gcp ) {
  return gcp->dfGCPPixel;
}
void GDAL_GCP_GCPPixel_set( GDAL_GCP *gcp, double dfGCPPixel ) {
  gcp->dfGCPPixel = dfGCPPixel;
}
double GDAL_GCP_GCPLine_get( GDAL_GCP *gcp ) {
  return gcp->dfGCPLine;
}
void GDAL_GCP_GCPLine_set( GDAL_GCP *gcp, double dfGCPLine ) {
  gcp->dfGCPLine = dfGCPLine;
}
const char * GDAL_GCP_Info_get( GDAL_GCP *gcp ) {
  return gcp->pszInfo;
}
void GDAL_GCP_Info_set( GDAL_GCP *gcp, const char * pszInfo ) {
  if ( gcp->pszInfo ) 
    CPLFree( gcp->pszInfo );
  gcp->pszInfo = CPLStrdup(pszInfo);
}
const char * GDAL_GCP_Id_get( GDAL_GCP *gcp ) {
  return gcp->pszId;
}
void GDAL_GCP_Id_set( GDAL_GCP *gcp, const char * pszId ) {
  if ( gcp->pszId ) 
    CPLFree( gcp->pszId );
  gcp->pszId = CPLStrdup(pszId);
}
%} //%inline 

#if defined(SWIGCSHARP)
%inline %{
/* Duplicate, but transposed names for C# because 
*  the C# module outputs backwards names
*/
double GDAL_GCP_get_GCPX( GDAL_GCP *gcp ) {
  return gcp->dfGCPX;
}
void GDAL_GCP_set_GCPX( GDAL_GCP *gcp, double dfGCPX ) {
  gcp->dfGCPX = dfGCPX;
}
double GDAL_GCP_get_GCPY( GDAL_GCP *gcp ) {
  return gcp->dfGCPY;
}
void GDAL_GCP_set_GCPY( GDAL_GCP *gcp, double dfGCPY ) {
  gcp->dfGCPY = dfGCPY;
}
double GDAL_GCP_get_GCPZ( GDAL_GCP *gcp ) {
  return gcp->dfGCPZ;
}
void GDAL_GCP_set_GCPZ( GDAL_GCP *gcp, double dfGCPZ ) {
  gcp->dfGCPZ = dfGCPZ;
}
double GDAL_GCP_get_GCPPixel( GDAL_GCP *gcp ) {
  return gcp->dfGCPPixel;
}
void GDAL_GCP_set_GCPPixel( GDAL_GCP *gcp, double dfGCPPixel ) {
  gcp->dfGCPPixel = dfGCPPixel;
}
double GDAL_GCP_get_GCPLine( GDAL_GCP *gcp ) {
  return gcp->dfGCPLine;
}
void GDAL_GCP_set_GCPLine( GDAL_GCP *gcp, double dfGCPLine ) {
  gcp->dfGCPLine = dfGCPLine;
}
const char * GDAL_GCP_get_Info( GDAL_GCP *gcp ) {
  return gcp->pszInfo;
}
void GDAL_GCP_set_Info( GDAL_GCP *gcp, const char * pszInfo ) {
  if ( gcp->pszInfo ) 
    CPLFree( gcp->pszInfo );
  gcp->pszInfo = CPLStrdup(pszInfo);
}
const char * GDAL_GCP_get_Id( GDAL_GCP *gcp ) {
  return gcp->pszId;
}
void GDAL_GCP_set_Id( GDAL_GCP *gcp, const char * pszId ) {
  if ( gcp->pszId ) 
    CPLFree( gcp->pszId );
  gcp->pszId = CPLStrdup(pszId);
}
%} //%inline 
#endif //if defined(SWIGCSHARP)

%clear GDAL_GCP *gcp;

#ifdef SWIGJAVA
%rename (GCPsToGeoTransform) wrapper_GDALGCPsToGeoTransform;
%inline
{
int wrapper_GDALGCPsToGeoTransform( int nGCPs, GDAL_GCP const * pGCPs, 
    	                             double argout[6], int bApproxOK = 1 )
{
    return GDALGCPsToGeoTransform(nGCPs, pGCPs, argout, bApproxOK);
}
}
#else
%apply (IF_FALSE_RETURN_NONE) { (RETURN_NONE) };
RETURN_NONE GDALGCPsToGeoTransform( int nGCPs, GDAL_GCP const * pGCPs, 
    	                             double argout[6], int bApproxOK = 1 ); 
%clear (RETURN_NONE);
#endif

%include "cplvirtualmem.i"

//************************************************************************
//
// Define the Dataset object.
//
//************************************************************************
%include "Dataset.i"

//************************************************************************
//
// Define the Band object.
//
//************************************************************************
%include "Band.i"

//************************************************************************
//
// Define the ColorTable object.
//
//************************************************************************
%include "ColorTable.i"

//************************************************************************
//
// Define the RasterAttributeTable object.
//
//************************************************************************
%include "RasterAttributeTable.i"

//************************************************************************
//
// Raster Operations
//
//************************************************************************
%include "Operations.i"

%apply (double argin[ANY]) {(double padfGeoTransform[6])};
%apply (double *OUTPUT) {(double *pdfGeoX)};
%apply (double *OUTPUT) {(double *pdfGeoY)};
void GDALApplyGeoTransform( double padfGeoTransform[6], 
                            double dfPixel, double dfLine, 
                            double *pdfGeoX, double *pdfGeoY );
%clear (double *padfGeoTransform);
%clear (double *pdfGeoX);
%clear (double *pdfGeoY);

%apply (double argin[ANY]) {double gt_in[6]};
%apply (double argout[ANY]) {double gt_out[6]};
#ifdef SWIGJAVA
// FIXME: we should implement correctly the IF_FALSE_RETURN_NONE typemap
int GDALInvGeoTransform( double gt_in[6], double gt_out[6] );
#else
%apply (IF_FALSE_RETURN_NONE) { (RETURN_NONE) };
RETURN_NONE GDALInvGeoTransform( double gt_in[6], double gt_out[6] );
%clear (RETURN_NONE);
#endif
%clear (double *gt_in);
%clear (double *gt_out);

#ifdef SWIGJAVA
%apply (const char* stringWithDefaultValue) {const char *request};
%rename (VersionInfo) wrapper_GDALVersionInfo;
%inline {
const char *wrapper_GDALVersionInfo( const char *request = "VERSION_NUM" )
{
    return GDALVersionInfo(request ? request : "VERSION_NUM");
}
}
%clear (const char* request);
#else
const char *GDALVersionInfo( const char *request = "VERSION_NUM" );
#endif

void GDALAllRegister();

void GDALDestroyDriverManager();

#ifdef SWIGPYTHON
%inline {
GIntBig wrapper_GDALGetCacheMax()
{
    return GDALGetCacheMax64();
}
}

%inline {
GIntBig wrapper_GDALGetCacheUsed()
{
    return GDALGetCacheUsed64();
}
}

%inline {
void wrapper_GDALSetCacheMax(GIntBig nBytes)
{
    return GDALSetCacheMax64(nBytes);
}
}

#else
%inline {
int wrapper_GDALGetCacheMax()
{
    return GDALGetCacheMax();
}
}

%inline {
int wrapper_GDALGetCacheUsed()
{
    return GDALGetCacheUsed();
}
}

%inline {
void wrapper_GDALSetCacheMax(int nBytes)
{
    return GDALSetCacheMax(nBytes);
}
}
#endif

int GDALGetDataTypeSize( GDALDataType eDataType );

int GDALDataTypeIsComplex( GDALDataType eDataType );

const char *GDALGetDataTypeName( GDALDataType eDataType );

GDALDataType GDALGetDataTypeByName( const char * pszDataTypeName );

const char *GDALGetColorInterpretationName( GDALColorInterp eColorInterp );

const char *GDALGetPaletteInterpretationName( GDALPaletteInterp ePaletteInterp );

#ifdef SWIGJAVA
%apply (const char* stringWithDefaultValue) {const char *request};
%rename (DecToDMS) wrapper_GDALDecToDMS;
%inline {
const char *wrapper_GDALDecToDMS( double dfAngle, const char * pszAxis,
                                  int nPrecision = 2 )
{
    return GDALDecToDMS(dfAngle, pszAxis, nPrecision);
}
}
%clear (const char* request);
#else
const char *GDALDecToDMS( double, const char *, int = 2 );
#endif

double GDALPackedDMSToDec( double dfPacked );

double GDALDecToPackedDMS( double dfDec );


#if defined(SWIGCSHARP) || defined(SWIGJAVA)
%newobject CPLParseXMLString;
#endif
CPLXMLNode *CPLParseXMLString( char * pszXMLString );

#if defined(SWIGJAVA) || defined(SWIGCSHARP) || defined(SWIGPYTHON) || defined(SWIGPERL)
retStringAndCPLFree *CPLSerializeXMLTree( CPLXMLNode *xmlnode );
#else
char *CPLSerializeXMLTree( CPLXMLNode *xmlnode );
#endif

#if defined(SWIGPYTHON)
%newobject GDALGetJPEG2000Structure;
CPLXMLNode *GDALGetJPEG2000Structure( const char* pszFilename, char** options = NULL );
#endif

%inline {
retStringAndCPLFree *GetJPEG2000StructureAsString( const char* pszFilename, char** options = NULL )
{
    CPLXMLNode* psNode = GDALGetJPEG2000Structure(pszFilename, options);
    if( psNode == NULL )
        return NULL;
    char* pszXML = CPLSerializeXMLTree(psNode);
    CPLDestroyXMLNode(psNode);
    return pszXML;
}
}


//************************************************************************
//
// Define the factory functions for Drivers and Datasets
//
//************************************************************************

// Missing
// GetDriverList

%inline %{
int GetDriverCount() {
  return GDALGetDriverCount();
}
%}

%apply Pointer NONNULL { char const *name };
%inline %{
GDALDriverShadow* GetDriverByName( char const *name ) {
  return (GDALDriverShadow*) GDALGetDriverByName( name );
}
%}

%inline %{
GDALDriverShadow* GetDriver( int i ) {
  return (GDALDriverShadow*) GDALGetDriver( i );
}
%}

#ifdef SWIGJAVA
%newobject Open;
%inline %{
GDALDatasetShadow* Open( char const* utf8_path, GDALAccess eAccess) {
  CPLErrorReset();
  GDALDatasetShadow *ds = GDALOpen( utf8_path, eAccess );
  if( ds != NULL && CPLGetLastErrorType() == CE_Failure )
  {
      if ( GDALDereferenceDataset( ds ) <= 0 )
          GDALClose(ds);
      ds = NULL;
  }
  return (GDALDatasetShadow*) ds;
}
%}

%newobject Open;
%inline %{
GDALDatasetShadow* Open( char const* name ) {
  return Open( name, GA_ReadOnly );
}
%}

#else
%newobject Open;
%inline %{
GDALDatasetShadow* Open( char const* utf8_path, GDALAccess eAccess = GA_ReadOnly ) {
  CPLErrorReset();
  GDALDatasetShadow *ds = GDALOpen( utf8_path, eAccess );
  if( ds != NULL && CPLGetLastErrorType() == CE_Failure )
  {
      if ( GDALDereferenceDataset( ds ) <= 0 )
          GDALClose(ds);
      ds = NULL;
  }
  return (GDALDatasetShadow*) ds;
}
%}

#endif

%newobject OpenEx;
#ifndef SWIGJAVA
%feature( "kwargs" ) OpenEx;
#endif
%apply (char **options) {char** allowed_drivers};
%apply (char **options) {char** open_options};
%apply (char **options) {char** sibling_files};
%inline %{
GDALDatasetShadow* OpenEx( char const* utf8_path, unsigned int nOpenFlags = 0,
                           char** allowed_drivers = NULL, char** open_options = NULL,
                           char** sibling_files = NULL ) {
  CPLErrorReset();
#ifdef SWIGPYTHON
  if( GetUseExceptions() )
      nOpenFlags |= GDAL_OF_VERBOSE_ERROR;
#endif
  GDALDatasetShadow *ds = GDALOpenEx( utf8_path, nOpenFlags, allowed_drivers,
                                      open_options, sibling_files );
  if( ds != NULL && CPLGetLastErrorType() == CE_Failure )
  {
      if ( GDALDereferenceDataset( ds ) <= 0 )
          GDALClose(ds);
      ds = NULL;
  }
  return (GDALDatasetShadow*) ds;
}
%}
%clear char** allowed_drivers;
%clear char** open_options;
%clear char** sibling_files;

%newobject OpenShared;
%inline %{
GDALDatasetShadow* OpenShared( char const* utf8_path, GDALAccess eAccess = GA_ReadOnly ) {
  CPLErrorReset();
  GDALDatasetShadow *ds = GDALOpenShared( utf8_path, eAccess );
  if( ds != NULL && CPLGetLastErrorType() == CE_Failure )
  {
      if ( GDALDereferenceDataset( ds ) <= 0 )
          GDALClose(ds);
      ds = NULL;
  }
  return (GDALDatasetShadow*) ds;
}
%}

%apply (char **options) {char **papszSiblings};
%inline %{
GDALDriverShadow *IdentifyDriver( const char *utf8_path, 
                                  char **papszSiblings = NULL ) {
    return (GDALDriverShadow *) GDALIdentifyDriver( utf8_path, 
	                                            papszSiblings );
}
%}
%clear char **papszSiblings;

//************************************************************************
//
// Define Algorithms
//
//************************************************************************

// Missing
// CreateAndReprojectImage
// GCPsToGeoTransform


#if defined(SWIGPYTHON) || defined(SWIGJAVA)
/* FIXME: other bindings should also use those typemaps to avoid memory leaks */
%apply (char **options) {char ** papszArgv};
%apply (char **CSL) {(char **)};
#else
%apply (char **options) {char **};
#endif

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
      GDALGeneralCmdLineProcessor( CSLCount(papszArgvModBefore), &papszArgvModAfter, nOptions ); 

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
      GDALGeneralCmdLineProcessor( CSLCount(papszArgv), &papszArgv, nOptions ); 

    if( nResArgCount <= 0 )
        return NULL;
    else
        return papszArgv;
  }
%}
#endif
%clear char **;


//************************************************************************
//
// Language specific extensions
//
//************************************************************************

#ifdef SWIGCSHARP
%include "gdal_csharp_extend.i"
#endif

#ifdef SWIGPYTHON
/* Add a __version__ attribute to match the convention */
%pythoncode %{
__version__ = _gdal.VersionInfo("RELEASE_NAME") 
%}
#endif
