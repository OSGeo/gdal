/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Bridge 
 * Purpose:  Declarations for GDAL Bridge support.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 * This file needs to be kept up to date with the contents of gdal.h by hand.
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.7  2000/09/26 15:20:32  warmerda
 * added GDALGetRasterBand{X,Y}Size
 *
 * Revision 1.6  2000/08/28 20:16:14  warmerda
 * added lots of OGRSpatialReference stuff
 *
 * Revision 1.5  2000/08/25 20:03:40  warmerda
 * added more entry points
 *
 * Revision 1.4  1999/09/17 03:18:37  warmerda
 * added name indirection for function pointer names for libtool
 *
 * Revision 1.3  1999/04/22 13:36:43  warmerda
 * Added copyright header.
 *
 */

#ifndef GDALBRIDGE_H_INCLUDED
#define GDALBRIDGE_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      Start C context.                                                */
/* -------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif
    
/* ==================================================================== */
/*      Standard types and defines normally supplied by cpl_port.h.     */
/* ==================================================================== */
#if UINT_MAX == 65535
typedef long            GInt32;
typedef unsigned long   GUInt32;
#else
typedef int             GInt32;
typedef unsigned int    GUInt32;
#endif

typedef short           GInt16;
typedef unsigned short  GUInt16;
typedef unsigned char   GByte;
typedef int             GBool;

#ifndef FALSE
#  define FALSE		0
#  define TRUE		1
#endif

#ifndef NULL
#  define NULL		0
#endif

#ifndef GDAL_ENTRY
#  define GDAL_ENTRY extern
#  define GDAL_NULL
#endif

/* -------------------------------------------------------------------- */
/*      Significant constants.                                          */
/* -------------------------------------------------------------------- */

/*! Pixel data types */
typedef enum {
    GDT_Unknown = 0,
    /*! Eight bit unsigned integer */ 		GDT_Byte = 1,
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
    GDT_TypeCount = 12		/* maximum type # + 1 */
} GDALDataType;

GDAL_ENTRY int	(*pfnGDALGetDataTypeSize)( GDALDataType ) GDAL_NULL;
#define GDALGetDataTypeSize pfnGDALGetDataTypeSize

typedef enum {
    GA_ReadOnly = 0,
    GA_Update = 1
} GDALAccess;

typedef enum {
    GF_Read = 0,
    GF_Write = 1
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
    /*! Black band of CMLY image */                       GCI_BlackBand=13
} GDALColorInterp;

/*! Types of color interpretations for a GDALColorTable. */
typedef enum 
{
  /*! Grayscale (in GDALColorEntry.c1) */                      GPI_Gray=0,
  /*! Red, Green, Blue and Alpha in (in c1, c2, c3 and c4) */  GPI_RGB=1,
  /*! Cyan, Magenta, Yellow and Black (in c1, c2, c3 and c4)*/ GPI_CMYK=2,
  /*! Hue, Lightness and Saturation (in c1, c2, and c3) */     GPI_HLS=3
} GDALPaletteInterp;

/* -------------------------------------------------------------------- */
/*      GDAL Specific error codes.                                      */
/*                                                                      */
/*      error codes 100 to 299 reserved for GDAL.                       */
/* -------------------------------------------------------------------- */
typedef enum
{
    CE_None = 0,
    CE_Log = 1,
    CE_Warning = 2,
    CE_Failure = 3,
    CE_Fatal = 4
  
} CPLErr;

#define CPLE_AppDefined			1
#define CPLE_OutOfMemory		2
#define CPLE_FileIO			3
#define CPLE_OpenFailed			4
#define CPLE_IllegalArg			5
#define CPLE_NotSupported		6
#define CPLE_AssertionFailed		7
#define CPLE_NoWriteAccess		8

#define CPLE_WrongFormat	200

typedef int OGRErr;

#define OGRERR_NONE                0
#define OGRERR_NOT_ENOUGH_DATA     1    /* not enough data to deserialize */
#define OGRERR_NOT_ENOUGH_MEMORY   2
#define OGRERR_UNSUPPORTED_GEOMETRY_TYPE 3
#define OGRERR_UNSUPPORTED_OPERATION 4
#define OGRERR_CORRUPT_DATA        5
#define OGRERR_FAILURE             6
#define OGRERR_UNSUPPORTED_SRS     7

/* -------------------------------------------------------------------- */
/*      Define handle types related to various internal classes.        */
/* -------------------------------------------------------------------- */

typedef void *GDALMajorObjectH;
typedef void *GDALDatasetH;
typedef void *GDALRasterBandH;
typedef void *GDALDriverH;
typedef void *GDALProjDefH;
typedef void *GDALColorTableH;
typedef void *OGRSpatialReferenceH;

/* ==================================================================== */
/*      Registration/driver related.                                    */
/* ==================================================================== */

GDAL_ENTRY void (*pfnGDALAllRegister)( void ) GDAL_NULL;
#define GDALAllRegister pfnGDALAllRegister

GDAL_ENTRY GDALDatasetH (*pfnGDALCreate)( GDALDriverH hDriver, const char *,
                                          int, int, int, GDALDataType,
                                          char ** ) GDAL_NULL;
#define GDALCreate pfnGDALCreate


GDAL_ENTRY GDALDatasetH (*pfnGDALOpen)( const char *, GDALAccess ) GDAL_NULL;
#define GDALOpen pfnGDALOpen

GDAL_ENTRY GDALDriverH (*pfnGDALGetDriverByName)( const char * ) GDAL_NULL;
#define GDALGetDriverByName pfnGDALGetDriverByName

/* ==================================================================== */
/*      GDALDataset class ... normally this represents one file.        */
/* ==================================================================== */

GDAL_ENTRY void (*pfnGDALClose)( GDALDatasetH ) GDAL_NULL;
#define GDALClose pfnGDALClose

GDAL_ENTRY int (*pfnGDALGetRasterXSize)( GDALDatasetH ) GDAL_NULL;
#define GDALGetRasterXSize pfnGDALGetRasterXSize

GDAL_ENTRY int (*pfnGDALGetRasterYSize)( GDALDatasetH ) GDAL_NULL;
#define GDALGetRasterYSize pfnGDALGetRasterYSize

GDAL_ENTRY int (*pfnGDALGetRasterCount)( GDALDatasetH ) GDAL_NULL;
#define GDALGetRasterCount pfnGDALGetRasterCount

GDAL_ENTRY GDALRasterBandH
               (*pfnGDALGetRasterBand)( GDALDatasetH, int) GDAL_NULL;
#define GDALGetRasterBand pfnGDALGetRasterBand

GDAL_ENTRY const char *(*pfnGDALGetProjectionRef)( GDALDatasetH ) GDAL_NULL;
#define GDALGetProjectionRef pfnGDALGetProjectionRef

GDAL_ENTRY CPLErr (*pfnGDALSetProjection)( GDALDatasetH,
                                           const char * ) GDAL_NULL;
#define GDALSetProjection pfnGDALSetProjection

GDAL_ENTRY CPLErr (*pfnGDALGetGeoTransform)( GDALDatasetH, double* ) GDAL_NULL;
#define GDALGetGeoTransform pfnGDALGetGeoTransform

GDAL_ENTRY CPLErr (*pfnGDALSetGeoTransform)( GDALDatasetH, double* ) GDAL_NULL;
#define GDALSetGeoTransform pfnGDALSetGeoTransform

GDAL_ENTRY void *(*pfnGDALGetInternalHandle)( GDALDatasetH,
                                              const char * ) GDAL_NULL;
#define GDALGetInternalHandle pfnGDALGetInternalHandle

/* ==================================================================== */
/*      GDALRasterBand ... one band/channel in a dataset.               */
/* ==================================================================== */

GDAL_ENTRY GDALDataType (*pGDALGetRasterDataType)( GDALRasterBandH ) GDAL_NULL;
#define GDALGetRasterDataType pGDALGetRasterDataType

GDAL_ENTRY void (*pGDALGetBlockSize)( GDALRasterBandH,
                                      int * pnXSize, int * pnYSize ) GDAL_NULL;
#define GDALGetBlockSize pGDALGetBlockSize

GDAL_ENTRY CPLErr (*pGDALRasterIO)( GDALRasterBandH hRBand, GDALRWFlag eRWFlag,
                                    int nDSXOff, int nDSYOff,
                                    int nDSXSize, int nDSYSize,
                                    void * pBuffer, int nBXSize, int nBYSize,
                                    GDALDataType eBDataType,
                                   int nPixelSpace, int nLineSpace ) GDAL_NULL;
#define GDALRasterIO pGDALRasterIO

GDAL_ENTRY CPLErr (*pGDALReadBlock)( GDALRasterBandH,
                                     int, int, void * ) GDAL_NULL;
#define GDALReadBlock pGDALReadBlock

GDAL_ENTRY CPLErr (*pGDALWriteBlock)( GDALRasterBandH,
                                      int, int, void * ) GDAL_NULL;
#define GDALWriteBlock pGDALWriteBlock

GDAL_ENTRY int (*pGDALGetOverviewCount)( GDALRasterBandH ) GDAL_NULL;
#define GDALGetOverviewCount pGDALGetOverviewCount

GDAL_ENTRY GDALRasterBandH (*pGDALGetOverview)( GDALRasterBandH, int ) GDAL_NULL;
#define GDALGetOverview pGDALGetOverview

GDAL_ENTRY GDALColorInterp (*pGDALGetRasterColorInterpretation)
						( GDALRasterBandH ) GDAL_NULL;
#define GDALGetRasterColorInterpretation pGDALGetRasterColorInterpretation

GDAL_ENTRY const char *(*pGDALGetColorInterpretationName)( GDALColorInterp ) GDAL_NULL;
#define GDALGetColorInterpretationName pGDALGetColorInterpretationName

GDAL_ENTRY GDALColorTableH (*pGDALGetRasterColorTable)( GDALRasterBandH ) GDAL_NULL;
#define GDALGetRasterColorTable pGDALGetRasterColorTable

GDAL_ENTRY int (*pfnGDALGetRasterBandXSize)( GDALRasterBandH ) GDAL_NULL;
#define GDALGetRasterBandXSize pfnGDALGetRasterBandXSize

GDAL_ENTRY int (*pfnGDALGetRasterBandYSize)( GDALRasterBandH ) GDAL_NULL;
#define GDALGetRasterBandYSize pfnGDALGetRasterBandYSize

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

GDAL_ENTRY GDALPaletteInterp (*pGDALGetPaletteInterpretation)( GDALColorTableH ) GDAL_NULL;
#define GDALGetPaletteInterpretation pGDALGetPaletteInterpretation

GDAL_ENTRY const char *(*pGDALGetPaletteInterpretationName)(GDALPaletteInterp) GDAL_NULL;
#define GDALGetPaletteInterpretationName pGDALGetPaletteInterpretationName

GDAL_ENTRY int (*pGDALGetColorEntryCount)( GDALColorTableH ) GDAL_NULL;
#define GDALGetColorEntryCount pGDALGetColorEntryCount

GDAL_ENTRY const GDALColorEntry *(*pGDALGetColorEntry)( GDALColorTableH, int ) GDAL_NULL;
#define GDALGetColorEntry pGDALGetColorEntry

GDAL_ENTRY int (*pGDALGetColorEntryAsRGB)( GDALColorTableH, int, 
                                           GDALColorEntry *) GDAL_NULL;
#define GDALGetColorEntryAsRGB pGDALGetColorEntryAsRGB

GDAL_ENTRY void (*pGDALSetColorEntry)( GDALColorTableH, int, 
                                       const GDALColorEntry * ) GDAL_NULL;
#define GDALSetColorEntry pGDALSetColorEntry

/* ==================================================================== */
/*      Projections                                                     */
/* ==================================================================== */

GDAL_ENTRY GDALProjDefH (*pGDALCreateProjDef)( const char * ) GDAL_NULL;
#define GDALCreateProjDef pGDALCreateProjDef

GDAL_ENTRY CPLErr (*pGDALReprojectToLongLat)( GDALProjDefH,
                                              double *, double * ) GDAL_NULL;
#define GDALReprojectToLongLat pGDALReprojectToLongLat

GDAL_ENTRY CPLErr (*pGDALReprojectFromLongLat)( GDALProjDefH,
                                                double *, double * ) GDAL_NULL;
#define GDALReprojectFromLongLat pGDALReprojectFromLongLat

GDAL_ENTRY void (*pGDALDestroyProjDef)( GDALProjDefH ) GDAL_NULL;
#define GDALDestroyProjDef pGDALDestroyProjDef

GDAL_ENTRY const char *(*pGDALDecToDMS)( double, const char *, int ) GDAL_NULL;
#define GDALDecToDMS pGDALDecToDMS

/* -------------------------------------------------------------------- */
/*      ogr_srs_api.h services.                                         */
/* -------------------------------------------------------------------- */

GDAL_ENTRY OGRSpatialReferenceH 
	(*pOSRNewSpatialReference)( const char * ) GDAL_NULL;
#define OSRNewSpatialReference pOSRNewSpatialReference

GDAL_ENTRY OGRSpatialReferenceH 
	(*pOSRCloneGeogCS)( OGRSpatialReferenceH ) GDAL_NULL;
#define OSRCloneGeogCS pOSRCloneGeogCS

GDAL_ENTRY void 
	(*pOSRDestroySpatialReference)( OGRSpatialReferenceH ) GDAL_NULL;
#define OSRDestroySpatialReference pOSRDestroySpatialReference

GDAL_ENTRY int (*pOSRReference)( OGRSpatialReferenceH ) GDAL_NULL;
#define OSRReference pOSRReference

GDAL_ENTRY int (*pOSRDereference)( OGRSpatialReferenceH ) GDAL_NULL;
#define OSRDereference pOSRDereference

GDAL_ENTRY OGRErr (*pOSRImportFromEPSG)( OGRSpatialReferenceH, int ) GDAL_NULL;
#define OSRImportFromEPSG pOSRImportFromEPSG

GDAL_ENTRY OGRErr 
	(*pOSRImportFromWkt)( OGRSpatialReferenceH, char ** ) GDAL_NULL;
#define OSRImportFromWkt pOSRImportFromWkt

GDAL_ENTRY OGRErr 
	(*pOSRImportFromProj4)( OGRSpatialReferenceH, const char *) GDAL_NULL;
#define OSRImportFromProj4 pOSRImportFromProj4

GDAL_ENTRY OGRErr 
	(*pOSRExportToWkt)( OGRSpatialReferenceH, char ** ) GDAL_NULL;
#define OSRExportToWkt pOSRExportToWkt

GDAL_ENTRY OGRErr 
       (*pOSRExportToPrettyWkt)( OGRSpatialReferenceH, char **, int) GDAL_NULL;
#define OSRExportToPrettyWkt pOSRExportToPrettyWkt

GDAL_ENTRY OGRErr 
	(*pOSRExportToProj4)( OGRSpatialReferenceH, char **) GDAL_NULL;
#define OSRExportToProj4 pOSRExportToProj4

GDAL_ENTRY OGRErr 
	(*pOSRSetAttrValue)( OGRSpatialReferenceH hSRS,
                             const char * pszNodePath,
                             const char * pszNewNodeValue ) GDAL_NULL;
#define OSRSetAttrValue pOSRSetAttrValue

GDAL_ENTRY const char * (*pOSRGetAttrValue)( OGRSpatialReferenceH hSRS,
                           const char * pszName, int iChild ) GDAL_NULL;
#define OSRGetAttrValue pOSRGetAttrValue

GDAL_ENTRY OGRErr (*pOSRSetLinearUnits)( OGRSpatialReferenceH, const char *, 
                                         double ) GDAL_NULL;
#define OSRSetLinearUnits pOSRSetLinearUnits

GDAL_ENTRY double (*pOSRGetLinearUnits)( OGRSpatialReferenceH, 
                                         char ** ) GDAL_NULL;
#define OSRGetLinearUnits pOSRGetLinearUnits

GDAL_ENTRY int (*pOSRIsGeographic)( OGRSpatialReferenceH ) GDAL_NULL;
#define OSRIsGeographic pOSRIsGeographic

GDAL_ENTRY int (*pOSRIsProjected)( OGRSpatialReferenceH ) GDAL_NULL;
#define OSRIsProjected pOSRIsProjected

GDAL_ENTRY int (*pOSRIsSameGeogCS)( OGRSpatialReferenceH, 
                                    OGRSpatialReferenceH ) GDAL_NULL;
#define OSRIsSameGeogCS pOSRIsSameGeogCS

GDAL_ENTRY int (*pOSRIsSame)( OGRSpatialReferenceH, 
                              OGRSpatialReferenceH ) GDAL_NULL;
#define OSRIsSame pOSRIsSame

GDAL_ENTRY OGRErr (*pOSRSetProjCS)( OGRSpatialReferenceH hSRS, 
                                    const char * pszName ) GDAL_NULL;
#define OSRSetProjCS pOSRSetProjCS

GDAL_ENTRY OGRErr (*pOSRSetWellKnownGeogCS)( OGRSpatialReferenceH hSRS,
                                             const char * pszName ) GDAL_NULL;
#define OSRSetWellKnownGeogCS pOSRSetWellKnownGeogCS

GDAL_ENTRY OGRErr (*pOSRSetGeogCS)( OGRSpatialReferenceH hSRS,
                      const char * pszGeogName,
                      const char * pszDatumName,
                      const char * pszEllipsoidName,
                      double dfSemiMajor, double dfInvFlattening,
                      const char * pszPMName /* = NULL */,
                      double dfPMOffset /* = 0.0 */,
                      const char * pszUnits /* = NULL */,
                      double dfConvertToRadians /* = 0.0 */ ) GDAL_NULL;
#define OSRSetGeogCS pOSRSetGeogCS

GDAL_ENTRY double (*pOSRGetSemiMajor)( OGRSpatialReferenceH, 
                                       OGRErr * /* = NULL */ ) GDAL_NULL;
#define OSRGetSemiMajor pOSRGetSemiMajor

GDAL_ENTRY double (*pOSRGetSemiMinor)( OGRSpatialReferenceH, 
                                       OGRErr * /* = NULL */ ) GDAL_NULL;
#define OSRGetSemiMinor pOSRGetSemiMinor

GDAL_ENTRY double (*pOSRGetInvFlattening)( OGRSpatialReferenceH, 
                                           OGRErr * /*=NULL*/) GDAL_NULL;
#define OSRGetInvFlattening pOSRGetInvFlattening

GDAL_ENTRY OGRErr (*pOSRSetAuthority)( OGRSpatialReferenceH hSRS,
                                       const char * pszTargetKey,
                                       const char * pszAuthority,
                                       int nCode ) GDAL_NULL;
#define OSRSetAuthority pOSRSetAuthority

GDAL_ENTRY OGRErr (*pOSRSetProjParm)( OGRSpatialReferenceH, 
                                      const char *, double ) GDAL_NULL;
#define OSRSetProjParm pOSRSetProjParm

GDAL_ENTRY double (*pOSRGetProjParm)( OGRSpatialReferenceH hSRS,
                                      const char * pszParmName, 
                                      double dfDefault /* = 0.0 */,
                                      OGRErr * /* = NULL */ ) GDAL_NULL;
#define OSRGetProjParm pOSRGetProjParm

GDAL_ENTRY OGRErr (*pOSRSetUTM)( OGRSpatialReferenceH hSRS, 
                                 int nZone, int bNorth ) GDAL_NULL;
#define OSRSetUTM pOSRSetUTM

GDAL_ENTRY int (*pOSRGetUTMZone)( OGRSpatialReferenceH hSRS, 
                                  int *pbNorth ) GDAL_NULL;
#define OSRGetUTMZone pOSRGetUTMZone

/* -------------------------------------------------------------------- */
/*      This is the real entry point.  It tries to load the shared      */
/*      libraries (given a hint of a directory it might be in).  It     */
/*      returns TRUE if it succeeds, or FALSE otherwise.                */
/* -------------------------------------------------------------------- */
int	GDALBridgeInitialize( const char * );
void	*GBGetSymbol( const char *, const char * );

/* -------------------------------------------------------------------- */
/*      Terminate C context.                                            */
/* -------------------------------------------------------------------- */
#ifdef __cplusplus
}
#endif

#endif /* ndef GDALBRIDGE_H_INCLUDED */
