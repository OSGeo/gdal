/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  GDAL Core C/Public declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002 Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef GDAL_H_INCLUDED
#define GDAL_H_INCLUDED

/**
 * \file gdal.h
 *
 * Public (C callable) GDAL entry points.
 */

#ifndef DOXYGEN_SKIP
#include "gdal_version.h"
#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_virtualmem.h"
#include "ogr_api.h"
#endif

/* -------------------------------------------------------------------- */
/*      Significant constants.                                          */
/* -------------------------------------------------------------------- */

CPL_C_START

/*! Pixel data types */
typedef enum {
    /*! Unknown or unspecified type */ 		    GDT_Unknown = 0,
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

int CPL_DLL CPL_STDCALL GDALGetDataTypeSize( GDALDataType );
int CPL_DLL CPL_STDCALL GDALDataTypeIsComplex( GDALDataType );
const char CPL_DLL * CPL_STDCALL GDALGetDataTypeName( GDALDataType );
GDALDataType CPL_DLL CPL_STDCALL GDALGetDataTypeByName( const char * );
GDALDataType CPL_DLL CPL_STDCALL GDALDataTypeUnion( GDALDataType, GDALDataType );

/**
* status of the asynchronous stream
*/
typedef enum 
{	
	GARIO_PENDING = 0,
	GARIO_UPDATE = 1,
	GARIO_ERROR = 2,
	GARIO_COMPLETE = 3,
	GARIO_TypeCount = 4
} GDALAsyncStatusType;

const char CPL_DLL * CPL_STDCALL GDALGetAsyncStatusTypeName( GDALAsyncStatusType );
GDALAsyncStatusType CPL_DLL CPL_STDCALL GDALGetAsyncStatusTypeByName( const char * );

/*! Flag indicating read/write, or read-only access to data. */
typedef enum {
    /*! Read only (no update) access */ GA_ReadOnly = 0,
    /*! Read/write access. */           GA_Update = 1
} GDALAccess;

/*! Read/Write flag for RasterIO() method */
typedef enum {
    /*! Read data */   GF_Read = 0,
    /*! Write data */  GF_Write = 1
} GDALRWFlag;

/*! Types of color interpretation for raster bands. */
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

const char CPL_DLL *GDALGetColorInterpretationName( GDALColorInterp );
GDALColorInterp CPL_DLL GDALGetColorInterpretationByName( const char *pszName );

/*! Types of color interpretations for a GDALColorTable. */
typedef enum 
{
  /*! Grayscale (in GDALColorEntry.c1) */                      GPI_Gray=0,
  /*! Red, Green, Blue and Alpha in (in c1, c2, c3 and c4) */  GPI_RGB=1,
  /*! Cyan, Magenta, Yellow and Black (in c1, c2, c3 and c4)*/ GPI_CMYK=2,
  /*! Hue, Lightness and Saturation (in c1, c2, and c3) */     GPI_HLS=3
} GDALPaletteInterp;

const char CPL_DLL *GDALGetPaletteInterpretationName( GDALPaletteInterp );

/* "well known" metadata items. */

#define GDALMD_AREA_OR_POINT   "AREA_OR_POINT" 
#  define GDALMD_AOP_AREA      "Area"
#  define GDALMD_AOP_POINT     "Point"

/* -------------------------------------------------------------------- */
/*      GDAL Specific error codes.                                      */
/*                                                                      */
/*      error codes 100 to 299 reserved for GDAL.                       */
/* -------------------------------------------------------------------- */
#define CPLE_WrongFormat        200

/* -------------------------------------------------------------------- */
/*      Define handle types related to various internal classes.        */
/* -------------------------------------------------------------------- */

/** Opaque type used for the C bindings of the C++ GDALMajorObject class */
typedef void *GDALMajorObjectH;

/** Opaque type used for the C bindings of the C++ GDALDataset class */
typedef void *GDALDatasetH;

/** Opaque type used for the C bindings of the C++ GDALRasterBand class */
typedef void *GDALRasterBandH;

/** Opaque type used for the C bindings of the C++ GDALDriver class */
typedef void *GDALDriverH;

/** Opaque type used for the C bindings of the C++ GDALColorTable class */
typedef void *GDALColorTableH;

/** Opaque type used for the C bindings of the C++ GDALRasterAttributeTable class */
typedef void *GDALRasterAttributeTableH;

/** Opaque type used for the C bindings of the C++ GDALAsyncReader class */
typedef void *GDALAsyncReaderH;

/* ==================================================================== */
/*      Registration/driver related.                                    */
/* ==================================================================== */

#define GDAL_DMD_LONGNAME "DMD_LONGNAME"
#define GDAL_DMD_HELPTOPIC "DMD_HELPTOPIC"
#define GDAL_DMD_MIMETYPE "DMD_MIMETYPE"
#define GDAL_DMD_EXTENSION "DMD_EXTENSION"
#define GDAL_DMD_EXTENSIONS "DMD_EXTENSIONS"
#define GDAL_DMD_CREATIONOPTIONLIST "DMD_CREATIONOPTIONLIST" 
#define GDAL_DMD_OPENOPTIONLIST "DMD_OPENOPTIONLIST" 
#define GDAL_DMD_CREATIONDATATYPES "DMD_CREATIONDATATYPES" 
#define GDAL_DMD_SUBDATASETS "DMD_SUBDATASETS" 

#define GDAL_DCAP_OPEN       "DCAP_OPEN"
#define GDAL_DCAP_CREATE     "DCAP_CREATE"
#define GDAL_DCAP_CREATECOPY "DCAP_CREATECOPY"
#define GDAL_DCAP_VIRTUALIO  "DCAP_VIRTUALIO"

/** Capability set by a driver having raster capability. */
#define GDAL_DCAP_RASTER     "DCAP_RASTER"
/** Capability set by a driver having vector capability. */
#define GDAL_DCAP_VECTOR     "DCAP_VECTOR"

void CPL_DLL CPL_STDCALL GDALAllRegister( void );

GDALDatasetH CPL_DLL CPL_STDCALL GDALCreate( GDALDriverH hDriver,
                                 const char *, int, int, int, GDALDataType,
                                 char ** ) CPL_WARN_UNUSED_RESULT;
GDALDatasetH CPL_DLL CPL_STDCALL
GDALCreateCopy( GDALDriverH, const char *, GDALDatasetH,
                int, char **, GDALProgressFunc, void * ) CPL_WARN_UNUSED_RESULT;

GDALDriverH CPL_DLL CPL_STDCALL GDALIdentifyDriver( const char * pszFilename,
                                            char ** papszFileList );
GDALDatasetH CPL_DLL CPL_STDCALL
GDALOpen( const char *pszFilename, GDALAccess eAccess ) CPL_WARN_UNUSED_RESULT;
GDALDatasetH CPL_DLL CPL_STDCALL GDALOpenShared( const char *, GDALAccess ) CPL_WARN_UNUSED_RESULT;


/* Note: we define GDAL_OF_READONLY and GDAL_OF_UPDATE to be on purpose */
/* equals to GA_ReadOnly and GA_Update */

/** Open in read-only mode. */
#define     GDAL_OF_READONLY        0x00
/** Open in update mode. */
#define     GDAL_OF_UPDATE          0x01

/** Allow raster and vector drivers. */
#define     GDAL_OF_ALL             0x00

/** Allow raster drivers. */
#define     GDAL_OF_RASTER          0x02
/** Allow vector drivers. */
#define     GDAL_OF_VECTOR          0x04
/* Some space for GDAL 3.0 new types ;-) */
/*#define     GDAL_OF_OTHER_KIND1   0x08 */
/*#define     GDAL_OF_OTHER_KIND2   0x10 */
#ifndef DOXYGEN_SKIP
#define     GDAL_OF_KIND_MASK       0x1E
#endif

/** Open in shared mode. */
#define     GDAL_OF_SHARED          0x20

/** Emit error message in case of failed open. */
#define     GDAL_OF_VERBOSE_ERROR   0x40

/** Open as internal dataset. Such dataset isn't registered in the global list
 *  of opened dataset. Cannot be used with GDAL_OF_SHARED */
#define     GDAL_OF_INTERNAL        0x80

GDALDatasetH CPL_DLL CPL_STDCALL GDALOpenEx( const char* pszFilename,
                                             unsigned int nOpenFlags,
                                             const char* const* papszAllowedDrivers,
                                             const char* const* papszOpenOptions,
                                             const char* const* papszSiblingFiles ) CPL_WARN_UNUSED_RESULT;

int          CPL_DLL CPL_STDCALL GDALDumpOpenDatasets( FILE * );

GDALDriverH CPL_DLL CPL_STDCALL GDALGetDriverByName( const char * );
int CPL_DLL         CPL_STDCALL GDALGetDriverCount( void );
GDALDriverH CPL_DLL CPL_STDCALL GDALGetDriver( int );
void        CPL_DLL CPL_STDCALL GDALDestroyDriver( GDALDriverH );
int         CPL_DLL CPL_STDCALL GDALRegisterDriver( GDALDriverH );
void        CPL_DLL CPL_STDCALL GDALDeregisterDriver( GDALDriverH );
void        CPL_DLL CPL_STDCALL GDALDestroyDriverManager( void );
void        CPL_DLL             GDALDestroy( void );
CPLErr      CPL_DLL CPL_STDCALL GDALDeleteDataset( GDALDriverH, const char * );
CPLErr      CPL_DLL CPL_STDCALL GDALRenameDataset( GDALDriverH, 
                                                   const char * pszNewName,
                                                   const char * pszOldName );
CPLErr      CPL_DLL CPL_STDCALL GDALCopyDatasetFiles( GDALDriverH, 
                                                      const char * pszNewName,
                                                      const char * pszOldName);
int         CPL_DLL CPL_STDCALL GDALValidateCreationOptions( GDALDriverH,
                                                             char** papszCreationOptions);

/* The following are deprecated */
const char CPL_DLL * CPL_STDCALL GDALGetDriverShortName( GDALDriverH );
const char CPL_DLL * CPL_STDCALL GDALGetDriverLongName( GDALDriverH );
const char CPL_DLL * CPL_STDCALL GDALGetDriverHelpTopic( GDALDriverH );
const char CPL_DLL * CPL_STDCALL GDALGetDriverCreationOptionList( GDALDriverH );

/* ==================================================================== */
/*      GDAL_GCP                                                        */
/* ==================================================================== */

/** Ground Control Point */
typedef struct
{
    /** Unique identifier, often numeric */
    char        *pszId; 

    /** Informational message or "" */
    char        *pszInfo;

    /** Pixel (x) location of GCP on raster */
    double      dfGCPPixel;
    /** Line (y) location of GCP on raster */
    double      dfGCPLine;

    /** X position of GCP in georeferenced space */
    double      dfGCPX;

    /** Y position of GCP in georeferenced space */
    double      dfGCPY;

    /** Elevation of GCP, or zero if not known */
    double      dfGCPZ;
} GDAL_GCP;

void CPL_DLL CPL_STDCALL GDALInitGCPs( int, GDAL_GCP * );
void CPL_DLL CPL_STDCALL GDALDeinitGCPs( int, GDAL_GCP * );
GDAL_GCP CPL_DLL * CPL_STDCALL GDALDuplicateGCPs( int, const GDAL_GCP * );

int CPL_DLL CPL_STDCALL
GDALGCPsToGeoTransform( int nGCPCount, const GDAL_GCP *pasGCPs, 
                        double *padfGeoTransform, int bApproxOK )  CPL_WARN_UNUSED_RESULT; 
int CPL_DLL CPL_STDCALL
GDALInvGeoTransform( double *padfGeoTransformIn, 
                     double *padfInvGeoTransformOut ) CPL_WARN_UNUSED_RESULT;
void CPL_DLL CPL_STDCALL GDALApplyGeoTransform( double *, double, double, 
                                                double *, double * );
void CPL_DLL GDALComposeGeoTransforms(const double *padfGeoTransform1,
                                      const double *padfGeoTransform2,
                                      double *padfGeoTransformOut);

/* ==================================================================== */
/*      major objects (dataset, and, driver, drivermanager).            */
/* ==================================================================== */

char CPL_DLL  ** CPL_STDCALL GDALGetMetadataDomainList( GDALMajorObjectH hObject );
char CPL_DLL  ** CPL_STDCALL GDALGetMetadata( GDALMajorObjectH, const char * );
CPLErr CPL_DLL CPL_STDCALL GDALSetMetadata( GDALMajorObjectH, char **,
                                            const char * );
const char CPL_DLL * CPL_STDCALL 
GDALGetMetadataItem( GDALMajorObjectH, const char *, const char * );
CPLErr CPL_DLL CPL_STDCALL
GDALSetMetadataItem( GDALMajorObjectH, const char *, const char *,
                     const char * );
const char CPL_DLL * CPL_STDCALL GDALGetDescription( GDALMajorObjectH );
void CPL_DLL CPL_STDCALL GDALSetDescription( GDALMajorObjectH, const char * );

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

#define GDAL_DS_LAYER_CREATIONOPTIONLIST "DS_LAYER_CREATIONOPTIONLIST" 

GDALDriverH CPL_DLL CPL_STDCALL GDALGetDatasetDriver( GDALDatasetH );
char CPL_DLL ** CPL_STDCALL GDALGetFileList( GDALDatasetH );
void CPL_DLL CPL_STDCALL   GDALClose( GDALDatasetH );
int CPL_DLL CPL_STDCALL     GDALGetRasterXSize( GDALDatasetH );
int CPL_DLL CPL_STDCALL     GDALGetRasterYSize( GDALDatasetH );
int CPL_DLL CPL_STDCALL     GDALGetRasterCount( GDALDatasetH );
GDALRasterBandH CPL_DLL CPL_STDCALL GDALGetRasterBand( GDALDatasetH, int );

CPLErr CPL_DLL  CPL_STDCALL GDALAddBand( GDALDatasetH hDS, GDALDataType eType, 
                             char **papszOptions );

GDALAsyncReaderH CPL_DLL CPL_STDCALL 
GDALBeginAsyncReader(GDALDatasetH hDS, int nXOff, int nYOff,
                     int nXSize, int nYSize,
                     void *pBuf, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount, int* panBandMap,
                     int nPixelSpace, int nLineSpace, int nBandSpace,
                     char **papszOptions);

void  CPL_DLL CPL_STDCALL 
GDALEndAsyncReader(GDALDatasetH hDS, GDALAsyncReaderH hAsynchReaderH);

CPLErr CPL_DLL CPL_STDCALL GDALDatasetRasterIO( 
    GDALDatasetH hDS, GDALRWFlag eRWFlag,
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    void * pBuffer, int nBXSize, int nBYSize, GDALDataType eBDataType,
    int nBandCount, int *panBandCount, 
    int nPixelSpace, int nLineSpace, int nBandSpace);

CPLErr CPL_DLL CPL_STDCALL GDALDatasetAdviseRead( GDALDatasetH hDS, 
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    int nBXSize, int nBYSize, GDALDataType eBDataType,
    int nBandCount, int *panBandCount, char **papszOptions );

const char CPL_DLL * CPL_STDCALL GDALGetProjectionRef( GDALDatasetH );
CPLErr CPL_DLL CPL_STDCALL GDALSetProjection( GDALDatasetH, const char * );
CPLErr CPL_DLL CPL_STDCALL GDALGetGeoTransform( GDALDatasetH, double * );
CPLErr CPL_DLL CPL_STDCALL GDALSetGeoTransform( GDALDatasetH, double * );

int CPL_DLL CPL_STDCALL  GDALGetGCPCount( GDALDatasetH );
const char CPL_DLL * CPL_STDCALL GDALGetGCPProjection( GDALDatasetH );
const GDAL_GCP CPL_DLL * CPL_STDCALL GDALGetGCPs( GDALDatasetH );
CPLErr CPL_DLL CPL_STDCALL GDALSetGCPs( GDALDatasetH, int, const GDAL_GCP *,
                                        const char * );

void CPL_DLL * CPL_STDCALL GDALGetInternalHandle( GDALDatasetH, const char * );
int CPL_DLL CPL_STDCALL GDALReferenceDataset( GDALDatasetH );
int CPL_DLL CPL_STDCALL GDALDereferenceDataset( GDALDatasetH );

CPLErr CPL_DLL CPL_STDCALL
GDALBuildOverviews( GDALDatasetH, const char *, int, int *,
                    int, int *, GDALProgressFunc, void * );
void CPL_DLL CPL_STDCALL GDALGetOpenDatasets( GDALDatasetH **hDS, int *pnCount );
int CPL_DLL CPL_STDCALL GDALGetAccess( GDALDatasetH hDS );
void CPL_DLL CPL_STDCALL GDALFlushCache( GDALDatasetH hDS );

CPLErr CPL_DLL CPL_STDCALL 
              GDALCreateDatasetMaskBand( GDALDatasetH hDS, int nFlags );

CPLErr CPL_DLL CPL_STDCALL GDALDatasetCopyWholeRaster(
    GDALDatasetH hSrcDS, GDALDatasetH hDstDS, char **papszOptions, 
    GDALProgressFunc pfnProgress, void *pProgressData );

CPLErr CPL_DLL CPL_STDCALL GDALRasterBandCopyWholeRaster(
    GDALRasterBandH hSrcBand, GDALRasterBandH hDstBand, char **papszOptions,
    GDALProgressFunc pfnProgress, void *pProgressData );

CPLErr CPL_DLL 
GDALRegenerateOverviews( GDALRasterBandH hSrcBand, 
                         int nOverviewCount, GDALRasterBandH *pahOverviewBands,
                         const char *pszResampling, 
                         GDALProgressFunc pfnProgress, void *pProgressData );

int    CPL_DLL GDALDatasetGetLayerCount( GDALDatasetH );
OGRLayerH CPL_DLL GDALDatasetGetLayer( GDALDatasetH, int );
OGRLayerH CPL_DLL GDALDatasetGetLayerByName( GDALDatasetH, const char * );
OGRErr    CPL_DLL GDALDatasetDeleteLayer( GDALDatasetH, int );
OGRLayerH CPL_DLL GDALDatasetCreateLayer( GDALDatasetH, const char *, 
                                      OGRSpatialReferenceH, OGRwkbGeometryType,
                                      char ** );
OGRLayerH CPL_DLL GDALDatasetCopyLayer( GDALDatasetH, OGRLayerH, const char *,
                                        char ** );
int    CPL_DLL GDALDatasetTestCapability( GDALDatasetH, const char * );
OGRLayerH CPL_DLL GDALDatasetExecuteSQL( GDALDatasetH, const char *,
                                     OGRGeometryH, const char * );
void   CPL_DLL GDALDatasetReleaseResultSet( GDALDatasetH, OGRLayerH );
OGRStyleTableH CPL_DLL GDALDatasetGetStyleTable( GDALDatasetH );
void   CPL_DLL GDALDatasetSetStyleTableDirectly( GDALDatasetH, OGRStyleTableH );
void   CPL_DLL GDALDatasetSetStyleTable( GDALDatasetH, OGRStyleTableH );


/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

/**
 * SRCVAL - Macro which may be used by pixel functions to obtain
 *          a pixel from a source buffer.
 */
#define SRCVAL(papoSource, eSrcType, ii) \
      (eSrcType == GDT_Byte ? \
          ((GByte *)papoSource)[ii] : \
      (eSrcType == GDT_Float32 ? \
          ((float *)papoSource)[ii] : \
      (eSrcType == GDT_Float64 ? \
          ((double *)papoSource)[ii] : \
      (eSrcType == GDT_Int32 ? \
          ((GInt32 *)papoSource)[ii] : \
      (eSrcType == GDT_UInt16 ? \
          ((GUInt16 *)papoSource)[ii] : \
      (eSrcType == GDT_Int16 ? \
          ((GInt16 *)papoSource)[ii] : \
      (eSrcType == GDT_UInt32 ? \
          ((GUInt32 *)papoSource)[ii] : \
      (eSrcType == GDT_CInt16 ? \
          ((GInt16 *)papoSource)[ii * 2] : \
      (eSrcType == GDT_CInt32 ? \
          ((GInt32 *)papoSource)[ii * 2] : \
      (eSrcType == GDT_CFloat32 ? \
          ((float *)papoSource)[ii * 2] : \
      (eSrcType == GDT_CFloat64 ? \
          ((double *)papoSource)[ii * 2] : 0)))))))))))

typedef CPLErr
(*GDALDerivedPixelFunc)(void **papoSources, int nSources, void *pData,
			int nBufXSize, int nBufYSize,
			GDALDataType eSrcType, GDALDataType eBufType,
                        int nPixelSpace, int nLineSpace);

GDALDataType CPL_DLL CPL_STDCALL GDALGetRasterDataType( GDALRasterBandH );
void CPL_DLL CPL_STDCALL 
GDALGetBlockSize( GDALRasterBandH, int * pnXSize, int * pnYSize );

CPLErr CPL_DLL CPL_STDCALL GDALRasterAdviseRead( GDALRasterBandH hRB, 
    int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
    int nBXSize, int nBYSize, GDALDataType eBDataType, char **papszOptions );

CPLErr CPL_DLL CPL_STDCALL 
GDALRasterIO( GDALRasterBandH hRBand, GDALRWFlag eRWFlag,
              int nDSXOff, int nDSYOff, int nDSXSize, int nDSYSize,
              void * pBuffer, int nBXSize, int nBYSize,GDALDataType eBDataType,
              int nPixelSpace, int nLineSpace );
CPLErr CPL_DLL CPL_STDCALL GDALReadBlock( GDALRasterBandH, int, int, void * );
CPLErr CPL_DLL CPL_STDCALL GDALWriteBlock( GDALRasterBandH, int, int, void * );
int CPL_DLL CPL_STDCALL GDALGetRasterBandXSize( GDALRasterBandH );
int CPL_DLL CPL_STDCALL GDALGetRasterBandYSize( GDALRasterBandH );
GDALAccess CPL_DLL CPL_STDCALL GDALGetRasterAccess( GDALRasterBandH );
int CPL_DLL CPL_STDCALL GDALGetBandNumber( GDALRasterBandH );
GDALDatasetH CPL_DLL CPL_STDCALL GDALGetBandDataset( GDALRasterBandH );

GDALColorInterp CPL_DLL CPL_STDCALL
GDALGetRasterColorInterpretation( GDALRasterBandH );
CPLErr CPL_DLL CPL_STDCALL 
GDALSetRasterColorInterpretation( GDALRasterBandH, GDALColorInterp );
GDALColorTableH CPL_DLL CPL_STDCALL GDALGetRasterColorTable( GDALRasterBandH );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterColorTable( GDALRasterBandH, GDALColorTableH );
int CPL_DLL CPL_STDCALL GDALHasArbitraryOverviews( GDALRasterBandH );
int CPL_DLL CPL_STDCALL GDALGetOverviewCount( GDALRasterBandH );
GDALRasterBandH CPL_DLL CPL_STDCALL GDALGetOverview( GDALRasterBandH, int );
double CPL_DLL CPL_STDCALL GDALGetRasterNoDataValue( GDALRasterBandH, int * );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterNoDataValue( GDALRasterBandH, double );
char CPL_DLL ** CPL_STDCALL GDALGetRasterCategoryNames( GDALRasterBandH );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterCategoryNames( GDALRasterBandH, char ** );
double CPL_DLL CPL_STDCALL GDALGetRasterMinimum( GDALRasterBandH, int *pbSuccess );
double CPL_DLL CPL_STDCALL GDALGetRasterMaximum( GDALRasterBandH, int *pbSuccess );
CPLErr CPL_DLL CPL_STDCALL GDALGetRasterStatistics( 
    GDALRasterBandH, int bApproxOK, int bForce, 
    double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev );
CPLErr CPL_DLL CPL_STDCALL GDALComputeRasterStatistics( 
    GDALRasterBandH, int bApproxOK, 
    double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev,
    GDALProgressFunc pfnProgress, void *pProgressData );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterStatistics( 
    GDALRasterBandH hBand, 
    double dfMin, double dfMax, double dfMean, double dfStdDev );

const char CPL_DLL * CPL_STDCALL GDALGetRasterUnitType( GDALRasterBandH );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterUnitType( GDALRasterBandH hBand, const char *pszNewValue );
double CPL_DLL CPL_STDCALL GDALGetRasterOffset( GDALRasterBandH, int *pbSuccess );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterOffset( GDALRasterBandH hBand, double dfNewOffset);
double CPL_DLL CPL_STDCALL GDALGetRasterScale( GDALRasterBandH, int *pbSuccess );
CPLErr CPL_DLL CPL_STDCALL GDALSetRasterScale( GDALRasterBandH hBand, double dfNewOffset );
void CPL_DLL CPL_STDCALL 
GDALComputeRasterMinMax( GDALRasterBandH hBand, int bApproxOK,
                         double adfMinMax[2] );
CPLErr CPL_DLL CPL_STDCALL GDALFlushRasterCache( GDALRasterBandH hBand );
CPLErr CPL_DLL CPL_STDCALL GDALGetRasterHistogram( GDALRasterBandH hBand,
                                       double dfMin, double dfMax,
                                       int nBuckets, int *panHistogram,
                                       int bIncludeOutOfRange, int bApproxOK,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );
CPLErr CPL_DLL CPL_STDCALL GDALGetDefaultHistogram( GDALRasterBandH hBand,
                                       double *pdfMin, double *pdfMax,
                                       int *pnBuckets, int **ppanHistogram,
                                       int bForce,
                                       GDALProgressFunc pfnProgress,
                                       void * pProgressData );
CPLErr CPL_DLL CPL_STDCALL GDALSetDefaultHistogram( GDALRasterBandH hBand,
                                       double dfMin, double dfMax,
                                       int nBuckets, int *panHistogram );
int CPL_DLL CPL_STDCALL
GDALGetRandomRasterSample( GDALRasterBandH, int, float * );
GDALRasterBandH CPL_DLL CPL_STDCALL
GDALGetRasterSampleOverview( GDALRasterBandH, int );
CPLErr CPL_DLL CPL_STDCALL GDALFillRaster( GDALRasterBandH hBand,
                          double dfRealValue, double dfImaginaryValue );
CPLErr CPL_DLL CPL_STDCALL
GDALComputeBandStats( GDALRasterBandH hBand, int nSampleStep, 
                             double *pdfMean, double *pdfStdDev, 
                             GDALProgressFunc pfnProgress,
                             void *pProgressData );
CPLErr CPL_DLL  GDALOverviewMagnitudeCorrection( GDALRasterBandH hBaseBand, 
                                        int nOverviewCount, 
                                        GDALRasterBandH *pahOverviews, 
                                        GDALProgressFunc pfnProgress, 
                                        void *pProgressData );

GDALRasterAttributeTableH CPL_DLL CPL_STDCALL GDALGetDefaultRAT( 
    GDALRasterBandH hBand );
CPLErr CPL_DLL CPL_STDCALL GDALSetDefaultRAT( GDALRasterBandH, 
                                              GDALRasterAttributeTableH );
CPLErr CPL_DLL CPL_STDCALL GDALAddDerivedBandPixelFunc( const char *pszName,
                                    GDALDerivedPixelFunc pfnPixelFunc );

GDALRasterBandH CPL_DLL CPL_STDCALL GDALGetMaskBand( GDALRasterBandH hBand );
int CPL_DLL CPL_STDCALL GDALGetMaskFlags( GDALRasterBandH hBand );
CPLErr CPL_DLL CPL_STDCALL 
                       GDALCreateMaskBand( GDALRasterBandH hBand, int nFlags );

#define GMF_ALL_VALID     0x01
#define GMF_PER_DATASET   0x02
#define GMF_ALPHA         0x04
#define GMF_NODATA        0x08

/* ==================================================================== */
/*     GDALAsyncReader                                                  */
/* ==================================================================== */

GDALAsyncStatusType CPL_DLL CPL_STDCALL 
GDALARGetNextUpdatedRegion(GDALAsyncReaderH hARIO, double dfTimeout,
                         int* pnXBufOff, int* pnYBufOff, 
                         int* pnXBufSize, int* pnYBufSize );
int CPL_DLL CPL_STDCALL GDALARLockBuffer(GDALAsyncReaderH hARIO,
                                        double dfTimeout);
void CPL_DLL CPL_STDCALL GDALARUnlockBuffer(GDALAsyncReaderH hARIO); 

/* -------------------------------------------------------------------- */
/*      Helper functions.                                               */
/* -------------------------------------------------------------------- */
int CPL_DLL CPL_STDCALL GDALGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv, 
                                         int nOptions );
void CPL_DLL CPL_STDCALL GDALSwapWords( void *pData, int nWordSize, int nWordCount,
                            int nWordSkip );
void CPL_DLL CPL_STDCALL 
    GDALCopyWords( void * pSrcData, GDALDataType eSrcType, int nSrcPixelOffset,
                   void * pDstData, GDALDataType eDstType, int nDstPixelOffset,
                   int nWordCount );

void CPL_DLL 
GDALCopyBits( const GByte *pabySrcData, int nSrcOffset, int nSrcStep, 
              GByte *pabyDstData, int nDstOffset, int nDstStep,
              int nBitCount, int nStepCount );

int CPL_DLL CPL_STDCALL GDALLoadWorldFile( const char *, double * );
int CPL_DLL CPL_STDCALL GDALReadWorldFile( const char *, const char *,
                                           double * );
int CPL_DLL CPL_STDCALL GDALWriteWorldFile( const char *, const char *,
                                            double * );
int CPL_DLL CPL_STDCALL GDALLoadTabFile( const char *, double *, char **,
                                         int *, GDAL_GCP ** );
int CPL_DLL CPL_STDCALL GDALReadTabFile( const char *, double *, char **,
                                         int *, GDAL_GCP ** );
int CPL_DLL CPL_STDCALL GDALLoadOziMapFile( const char *, double *, char **,
                                            int *, GDAL_GCP ** );
int CPL_DLL CPL_STDCALL GDALReadOziMapFile( const char * ,  double *,
                                            char **, int *, GDAL_GCP ** );
char CPL_DLL ** CPL_STDCALL GDALLoadRPBFile( const char *pszFilename, 
                                             char **papszSiblingFiles );
char CPL_DLL ** CPL_STDCALL GDALLoadRPCFile( const char *pszFilename, 
                                             char **papszSiblingFiles );
CPLErr CPL_DLL CPL_STDCALL GDALWriteRPBFile( const char *pszFilename, 
                                             char **papszMD );
char CPL_DLL ** CPL_STDCALL GDALLoadIMDFile( const char *pszFilename, 
                                             char **papszSiblingFiles );
CPLErr CPL_DLL CPL_STDCALL GDALWriteIMDFile( const char *pszFilename, 
                                             char **papszMD );

const char CPL_DLL * CPL_STDCALL GDALDecToDMS( double, const char *, int );
double CPL_DLL CPL_STDCALL GDALPackedDMSToDec( double );
double CPL_DLL CPL_STDCALL GDALDecToPackedDMS( double );

/* Note to developers : please keep this section in sync with ogr_core.h */

#ifndef GDAL_VERSION_INFO_DEFINED
#define GDAL_VERSION_INFO_DEFINED
const char CPL_DLL * CPL_STDCALL GDALVersionInfo( const char * );
#endif

#ifndef GDAL_CHECK_VERSION

int CPL_DLL CPL_STDCALL GDALCheckVersion( int nVersionMajor, int nVersionMinor,
                                          const char* pszCallingComponentName);

/** Helper macro for GDALCheckVersion()
  @see GDALCheckVersion()
  */
#define GDAL_CHECK_VERSION(pszCallingComponentName) \
 GDALCheckVersion(GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR, pszCallingComponentName)

#endif

typedef struct { 
    double      dfLINE_OFF;
    double      dfSAMP_OFF;
    double      dfLAT_OFF;
    double      dfLONG_OFF;
    double      dfHEIGHT_OFF;

    double      dfLINE_SCALE;
    double      dfSAMP_SCALE;
    double      dfLAT_SCALE;
    double      dfLONG_SCALE;
    double      dfHEIGHT_SCALE;

    double      adfLINE_NUM_COEFF[20];
    double      adfLINE_DEN_COEFF[20];
    double      adfSAMP_NUM_COEFF[20];
    double      adfSAMP_DEN_COEFF[20];
    
    double	dfMIN_LONG;
    double      dfMIN_LAT;
    double      dfMAX_LONG;
    double	dfMAX_LAT;

} GDALRPCInfo;

int CPL_DLL CPL_STDCALL GDALExtractRPCInfo( char **, GDALRPCInfo * );

/* ==================================================================== */
/*      Color tables.                                                   */
/* ==================================================================== */

/** Color tuple */
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

GDALColorTableH CPL_DLL CPL_STDCALL GDALCreateColorTable( GDALPaletteInterp );
void CPL_DLL CPL_STDCALL GDALDestroyColorTable( GDALColorTableH );
GDALColorTableH CPL_DLL CPL_STDCALL GDALCloneColorTable( GDALColorTableH );
GDALPaletteInterp CPL_DLL CPL_STDCALL GDALGetPaletteInterpretation( GDALColorTableH );
int CPL_DLL CPL_STDCALL GDALGetColorEntryCount( GDALColorTableH );
const GDALColorEntry CPL_DLL * CPL_STDCALL GDALGetColorEntry( GDALColorTableH, int );
int CPL_DLL CPL_STDCALL GDALGetColorEntryAsRGB( GDALColorTableH, int, GDALColorEntry *);
void CPL_DLL CPL_STDCALL GDALSetColorEntry( GDALColorTableH, int, const GDALColorEntry * );
void CPL_DLL CPL_STDCALL GDALCreateColorRamp( GDALColorTableH hTable, 
            int nStartIndex, const GDALColorEntry *psStartColor,
            int nEndIndex, const GDALColorEntry *psEndColor );

/* ==================================================================== */
/*      Raster Attribute Table						*/
/* ==================================================================== */

/** Field type of raster attribute table */
typedef enum {
    /*! Integer field */	   	   GFT_Integer , 
    /*! Floating point (double) field */   GFT_Real,
    /*! String field */                    GFT_String
} GDALRATFieldType;

/** Field usage of raster attribute table */
typedef enum {
    /*! General purpose field. */          GFU_Generic = 0,  
    /*! Histogram pixel count */           GFU_PixelCount = 1,
    /*! Class name */                      GFU_Name = 2,
    /*! Class range minimum */             GFU_Min = 3,
    /*! Class range maximum */             GFU_Max = 4,
    /*! Class value (min=max) */           GFU_MinMax = 5,
    /*! Red class color (0-255) */         GFU_Red = 6,
    /*! Green class color (0-255) */       GFU_Green = 7,
    /*! Blue class color (0-255) */        GFU_Blue = 8,
    /*! Alpha (0=transparent,255=opaque)*/ GFU_Alpha = 9,
    /*! Color Range Red Minimum */         GFU_RedMin = 10,
    /*! Color Range Green Minimum */       GFU_GreenMin = 11,
    /*! Color Range Blue Minimum */        GFU_BlueMin = 12,
    /*! Color Range Alpha Minimum */       GFU_AlphaMin = 13,
    /*! Color Range Red Maximum */         GFU_RedMax = 14,
    /*! Color Range Green Maximum */       GFU_GreenMax = 15,
    /*! Color Range Blue Maximum */        GFU_BlueMax = 16,
    /*! Color Range Alpha Maximum */       GFU_AlphaMax = 17,
    /*! Maximum GFU value */               GFU_MaxCount
} GDALRATFieldUsage;

GDALRasterAttributeTableH CPL_DLL CPL_STDCALL 
                                           GDALCreateRasterAttributeTable(void);
void CPL_DLL CPL_STDCALL GDALDestroyRasterAttributeTable(
    GDALRasterAttributeTableH );

int CPL_DLL CPL_STDCALL GDALRATGetColumnCount( GDALRasterAttributeTableH );

const char CPL_DLL * CPL_STDCALL GDALRATGetNameOfCol( 
    GDALRasterAttributeTableH, int );
GDALRATFieldUsage CPL_DLL CPL_STDCALL GDALRATGetUsageOfCol( 
    GDALRasterAttributeTableH, int );
GDALRATFieldType CPL_DLL CPL_STDCALL GDALRATGetTypeOfCol( 
    GDALRasterAttributeTableH, int );

int CPL_DLL CPL_STDCALL GDALRATGetColOfUsage( GDALRasterAttributeTableH, 
                                              GDALRATFieldUsage );
int CPL_DLL CPL_STDCALL GDALRATGetRowCount( GDALRasterAttributeTableH );

const char CPL_DLL * CPL_STDCALL GDALRATGetValueAsString( 
    GDALRasterAttributeTableH, int ,int);
int CPL_DLL CPL_STDCALL GDALRATGetValueAsInt( 
    GDALRasterAttributeTableH, int ,int);
double CPL_DLL CPL_STDCALL GDALRATGetValueAsDouble( 
    GDALRasterAttributeTableH, int ,int);

void CPL_DLL CPL_STDCALL GDALRATSetValueAsString( GDALRasterAttributeTableH, int, int,
                                                  const char * );
void CPL_DLL CPL_STDCALL GDALRATSetValueAsInt( GDALRasterAttributeTableH, int, int,
                                               int );
void CPL_DLL CPL_STDCALL GDALRATSetValueAsDouble( GDALRasterAttributeTableH, int, int,
                                                  double );

int CPL_DLL CPL_STDCALL GDALRATChangesAreWrittenToFile( GDALRasterAttributeTableH hRAT );

CPLErr CPL_DLL CPL_STDCALL GDALRATValuesIOAsDouble( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag, 
                                        int iField, int iStartRow, int iLength, double *pdfData );
CPLErr CPL_DLL CPL_STDCALL GDALRATValuesIOAsInteger( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag, 
                                        int iField, int iStartRow, int iLength, int *pnData);
CPLErr CPL_DLL CPL_STDCALL GDALRATValuesIOAsString( GDALRasterAttributeTableH hRAT, GDALRWFlag eRWFlag, 
                                        int iField, int iStartRow, int iLength, char **papszStrList);

void CPL_DLL CPL_STDCALL GDALRATSetRowCount( GDALRasterAttributeTableH, 
                                             int );
CPLErr CPL_DLL CPL_STDCALL GDALRATCreateColumn( GDALRasterAttributeTableH, 
                                                const char *, 
                                                GDALRATFieldType, 
                                                GDALRATFieldUsage );
CPLErr CPL_DLL CPL_STDCALL GDALRATSetLinearBinning( GDALRasterAttributeTableH, 
                                                    double, double );
int CPL_DLL CPL_STDCALL GDALRATGetLinearBinning( GDALRasterAttributeTableH, 
                                                 double *, double * );
CPLErr CPL_DLL CPL_STDCALL GDALRATInitializeFromColorTable(
    GDALRasterAttributeTableH, GDALColorTableH );
GDALColorTableH CPL_DLL CPL_STDCALL GDALRATTranslateToColorTable(
    GDALRasterAttributeTableH, int nEntryCount );
void CPL_DLL CPL_STDCALL GDALRATDumpReadable( GDALRasterAttributeTableH, 
                                              FILE * );
GDALRasterAttributeTableH CPL_DLL CPL_STDCALL 
    GDALRATClone( GDALRasterAttributeTableH );

int CPL_DLL CPL_STDCALL GDALRATGetRowOfValue( GDALRasterAttributeTableH , double );


/* ==================================================================== */
/*      GDAL Cache Management                                           */
/* ==================================================================== */

void CPL_DLL CPL_STDCALL GDALSetCacheMax( int nBytes );
int CPL_DLL CPL_STDCALL GDALGetCacheMax(void);
int CPL_DLL CPL_STDCALL GDALGetCacheUsed(void);
void CPL_DLL CPL_STDCALL GDALSetCacheMax64( GIntBig nBytes );
GIntBig CPL_DLL CPL_STDCALL GDALGetCacheMax64(void);
GIntBig CPL_DLL CPL_STDCALL GDALGetCacheUsed64(void);

int CPL_DLL CPL_STDCALL GDALFlushCacheBlock(void);

/* ==================================================================== */
/*      GDAL virtual memory                                             */
/* ==================================================================== */

CPLVirtualMem CPL_DLL* GDALDatasetGetVirtualMem( GDALDatasetH hDS,
                                                 GDALRWFlag eRWFlag,
                                                 int nXOff, int nYOff,
                                                 int nXSize, int nYSize,
                                                 int nBufXSize, int nBufYSize,
                                                 GDALDataType eBufType,
                                                 int nBandCount, int* panBandMap,
                                                 int nPixelSpace,
                                                 GIntBig nLineSpace,
                                                 GIntBig nBandSpace,
                                                 size_t nCacheSize,
                                                 size_t nPageSizeHint,
                                                 int bSingleThreadUsage,
                                                 char **papszOptions );

CPLVirtualMem CPL_DLL* GDALRasterBandGetVirtualMem( GDALRasterBandH hBand,
                                         GDALRWFlag eRWFlag,
                                         int nXOff, int nYOff,
                                         int nXSize, int nYSize,
                                         int nBufXSize, int nBufYSize,
                                         GDALDataType eBufType,
                                         int nPixelSpace,
                                         GIntBig nLineSpace,
                                         size_t nCacheSize,
                                         size_t nPageSizeHint,
                                         int bSingleThreadUsage,
                                         char **papszOptions );

CPLVirtualMem CPL_DLL* GDALGetVirtualMemAuto( GDALRasterBandH hBand,
                                              GDALRWFlag eRWFlag,
                                              int *pnPixelSpace,
                                              GIntBig *pnLineSpace,
                                              char **papszOptions );

typedef enum
{
    /*! Tile Interleaved by Pixel: tile (0,0) with internal band interleaved by pixel organization, tile (1, 0), ...  */
    GTO_TIP,
    /*! Band Interleaved by Tile : tile (0,0) of first band, tile (0,0) of second band, ... tile (1,0) of fisrt band, tile (1,0) of second band, ... */
    GTO_BIT,
    /*! Band SeQuential : all the tiles of first band, all the tiles of following band... */
    GTO_BSQ
} GDALTileOrganization;

CPLVirtualMem CPL_DLL* GDALDatasetGetTiledVirtualMem( GDALDatasetH hDS,
                                                      GDALRWFlag eRWFlag,
                                                      int nXOff, int nYOff,
                                                      int nXSize, int nYSize,
                                                      int nTileXSize, int nTileYSize,
                                                      GDALDataType eBufType,
                                                      int nBandCount, int* panBandMap,
                                                      GDALTileOrganization eTileOrganization,
                                                      size_t nCacheSize,
                                                      int bSingleThreadUsage,
                                                      char **papszOptions );

CPLVirtualMem CPL_DLL* GDALRasterBandGetTiledVirtualMem( GDALRasterBandH hBand,
                                                         GDALRWFlag eRWFlag,
                                                         int nXOff, int nYOff,
                                                         int nXSize, int nYSize,
                                                         int nTileXSize, int nTileYSize,
                                                         GDALDataType eBufType,
                                                         size_t nCacheSize,
                                                         int bSingleThreadUsage,
                                                         char **papszOptions );


CPL_C_END

#endif /* ndef GDAL_H_INCLUDED */
