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

#ifdef PERL_CPAN_NAMESPACE
%module "Geo::GDAL"
#elif defined(SWIGCSHARP)
%module Gdal
#else
%module gdal
#endif

#ifdef SWIGCSHARP
%include swig_csharp_extensions.i
%implement_class(SWIGTYPE)
#endif

%feature ("compactdefaultargs");

//
// We register all the drivers upon module initialization
//

%{
#include <iostream>
using namespace std;

#include "cpl_port.h"
#include "cpl_string.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdalwarper.h"

typedef void GDALMajorObjectShadow;
typedef void GDALDriverShadow;
typedef void GDALDatasetShadow;
typedef void GDALRasterBandShadow;

typedef int FALSE_IS_ERR;

%}

//************************************************************************
//
// Enums.
//
//************************************************************************

#ifndef SWIGCSHARP
typedef int GDALColorInterp;
typedef int GDALAccess;
typedef int GDALDataType;
typedef int CPLErr;
typedef int GDALResampleAlg;
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

/*! Warp Resampling Algorithm */
%rename (ResampleAlg) GDALResampleAlg;
typedef enum {
  /*! Nearest neighbour (select on one input pixel) */ GRA_NearestNeighbour=0,
  /*! Bilinear (2x2 kernel) */                         GRA_Bilinear=1,
  /*! Cubic Convolution Approximation (4x4 kernel) */  GRA_Cubic=2,
  /*! Cubic B-Spline Approximation (4x4 kernel) */     GRA_CubicSpline=3,
} GDALResampleAlg;
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
#if defined(SWIGCSHARP)
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


//************************************************************************
//
// Define renames.
//
//************************************************************************
%rename (GCP) GDAL_GCP;

#ifdef SWIGRUBY
%rename (all_register) GDALAllRegister;
%rename (get_cache_max) GDALGetCacheMax;
%rename (set_cache_max) GDALSetCacheMax;
%rename (set_cache_used) GDALGetCacheUsed;
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
%rename (AllRegister) GDALAllRegister;
%rename (GetCacheMax) GDALGetCacheMax;
%rename (SetCacheMax) GDALSetCacheMax;
%rename (GetCacheUsed) GDALGetCacheUsed;
%rename (GetDataTypeSize) GDALGetDataTypeSize;
%rename (DataTypeIsComplex) GDALDataTypeIsComplex;
%rename (GCPsToGeoTransform) GDALGCPsToGeoTransform;
%rename (GetDataTypeName) GDALGetDataTypeName;
%rename (GetDataTypeByName) GDALGetDataTypeByName;
%rename (GetColorInterpretationName) GDALGetColorInterpretationName;
%rename (GetPaletteInterpretationName) GDALGetPaletteInterpretationName;
%rename (DecToDMS) GDALDecToDMS;
%rename (PackedDMSToDec) GDALPackedDMSToDec;
%rename (DecToPackedDMS) GDALDecToPackedDMS;
%rename (ParseXMLString) CPLParseXMLString;
%rename (SerializeXMLTree) CPLSerializeXMLTree;
#endif

//************************************************************************
//
// GDALColorEntry
//
//************************************************************************
#if defined(SWIGCSHARP)
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

  GDAL_GCP( double x = 0.0, double y = 0.0, double z = 0.0,
            double pixel = 0.0, double line = 0.0,
            const char *info = "", const char *id = "" ) {
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
%inline %{

double GDAL_GCP_GCPX_get( GDAL_GCP *h ) {
  return h->dfGCPX;
}
void GDAL_GCP_GCPX_set( GDAL_GCP *h, double val ) {
  h->dfGCPX = val;
}
double GDAL_GCP_GCPY_get( GDAL_GCP *h ) {
  return h->dfGCPY;
}
void GDAL_GCP_GCPY_set( GDAL_GCP *h, double val ) {
  h->dfGCPY = val;
}
double GDAL_GCP_GCPZ_get( GDAL_GCP *h ) {
  return h->dfGCPZ;
}
void GDAL_GCP_GCPZ_set( GDAL_GCP *h, double val ) {
  h->dfGCPZ = val;
}
double GDAL_GCP_GCPPixel_get( GDAL_GCP *h ) {
  return h->dfGCPPixel;
}
void GDAL_GCP_GCPPixel_set( GDAL_GCP *h, double val ) {
  h->dfGCPPixel = val;
}
double GDAL_GCP_GCPLine_get( GDAL_GCP *h ) {
  return h->dfGCPLine;
}
void GDAL_GCP_GCPLine_set( GDAL_GCP *h, double val ) {
  h->dfGCPLine = val;
}
const char * GDAL_GCP_Info_get( GDAL_GCP *h ) {
  return h->pszInfo;
}
void GDAL_GCP_Info_set( GDAL_GCP *h, const char * val ) {
  if ( h->pszInfo ) 
    CPLFree( h->pszInfo );
  h->pszInfo = CPLStrdup(val);
}
const char * GDAL_GCP_Id_get( GDAL_GCP *h ) {
  return h->pszId;
}
void GDAL_GCP_Id_set( GDAL_GCP *h, const char * val ) {
  if ( h->pszId ) 
    CPLFree( h->pszId );
  h->pszId = CPLStrdup(val);
}



/* Duplicate, but transposed names for C# because 
*  the C# module outputs backwards names
*/
double GDAL_GCP_get_GCPX( GDAL_GCP *h ) {
  return h->dfGCPX;
}
void GDAL_GCP_set_GCPX( GDAL_GCP *h, double val ) {
  h->dfGCPX = val;
}
double GDAL_GCP_get_GCPY( GDAL_GCP *h ) {
  return h->dfGCPY;
}
void GDAL_GCP_set_GCPY( GDAL_GCP *h, double val ) {
  h->dfGCPY = val;
}
double GDAL_GCP_get_GCPZ( GDAL_GCP *h ) {
  return h->dfGCPZ;
}
void GDAL_GCP_set_GCPZ( GDAL_GCP *h, double val ) {
  h->dfGCPZ = val;
}
double GDAL_GCP_get_GCPPixel( GDAL_GCP *h ) {
  return h->dfGCPPixel;
}
void GDAL_GCP_set_GCPPixel( GDAL_GCP *h, double val ) {
  h->dfGCPPixel = val;
}
double GDAL_GCP_get_GCPLine( GDAL_GCP *h ) {
  return h->dfGCPLine;
}
void GDAL_GCP_set_GCPLine( GDAL_GCP *h, double val ) {
  h->dfGCPLine = val;
}
const char * GDAL_GCP_get_Info( GDAL_GCP *h ) {
  return h->pszInfo;
}
void GDAL_GCP_set_Info( GDAL_GCP *h, const char * val ) {
  if ( h->pszInfo ) 
    CPLFree( h->pszInfo );
  h->pszInfo = CPLStrdup(val);
}
const char * GDAL_GCP_get_Id( GDAL_GCP *h ) {
  return h->pszId;
}
void GDAL_GCP_set_Id( GDAL_GCP *h, const char * val ) {
  if ( h->pszId ) 
    CPLFree( h->pszId );
  h->pszId = CPLStrdup(val);
}

%} //%inline 

%apply (IF_FALSE_RETURN_NONE) { (FALSE_IS_ERR) };
FALSE_IS_ERR GDALGCPsToGeoTransform( int nGCPs, GDAL_GCP const * pGCPs, 
    	                             double argout[6], int bApproxOK = 1 ); 
%clear (FALSE_IS_ERR);


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
// Define the global methods
//
//************************************************************************
//
// Missing
//
// GeneralCmdLineProcessor
// TermProgress
//

void GDALAllRegister();

int GDALGetCacheMax();

void GDALSetCacheMax( int nBytes );
    
int GDALGetCacheUsed();
    
int GDALGetDataTypeSize( GDALDataType );

int GDALDataTypeIsComplex( GDALDataType );

const char *GDALGetDataTypeName( GDALDataType );

GDALDataType GDALGetDataTypeByName( const char * );

const char *GDALGetColorInterpretationName( GDALColorInterp );

const char *GDALGetPaletteInterpretationName( GDALPaletteInterp );

const char *GDALDecToDMS( double, const char *, int = 2 );

double GDALPackedDMSToDec( double );

double GDALDecToPackedDMS( double );

#if defined(SWIGCSHARP)
%newobject CPLParseXMLString;
#endif
CPLXMLNode *CPLParseXMLString( char * );

char *CPLSerializeXMLTree( CPLXMLNode *xmlnode );

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

%newobject Open;
%inline %{
GDALDatasetShadow* Open( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  GDALDatasetShadow *ds = GDALOpen( name, eAccess );
  return (GDALDatasetShadow*) ds;
}
%}

%newobject OpenShared;
%inline %{
GDALDatasetShadow* OpenShared( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  GDALDatasetShadow *ds = GDALOpenShared( name, eAccess );
  return (GDALDatasetShadow*) ds;
}
%}

//************************************************************************
//
// Define Algorithms
//
//************************************************************************

// Missing
// ComputeMedianCutPCT
// DitherRGB2PCT
// RGBFile2PCTFile
// AutoCreateWarpedVRT
// ReprojectImage
// CreateAndReprojectImage
// GCPsToGeoTransform

%newobject AutoCreateWarpedVRT;
%inline %{
GDALDatasetShadow *AutoCreateWarpedVRT( GDALDatasetShadow *src_ds,
                                        const char *src_wkt = 0,
                                        const char *dst_wkt = 0,
                                        GDALResampleAlg eResampleAlg = GRA_NearestNeighbour,
                                        double maxerror = 0.0 ) {
  GDALDatasetShadow *ds = GDALAutoCreateWarpedVRT( src_ds, src_wkt,
                                                   dst_wkt,
                                                   eResampleAlg,
                                                   maxerror,
                                                   0 );
  if (ds == 0) {
    /*throw CPLGetLastErrorMsg(); causes a SWIG_exception later*/
  }
  return ds;
  
}
%}

%newobject AutoCreateWarpedVRT;
%apply (char **options) {char **};
%inline %{
  char **GeneralCmdLineProcessor( char **papszArgv, int nOptions = 0 ) {
    GDALGeneralCmdLineProcessor( CSLCount(papszArgv), &papszArgv, nOptions ); 
    return papszArgv;
  }
%}
%clear char **;


//************************************************************************
//
// Language specific extensions
//
//************************************************************************

#ifdef SWIGCSHARP
%include "gdal_csharp_extend.i"
#endif
