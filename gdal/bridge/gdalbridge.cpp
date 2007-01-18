/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Bridge 
 * Purpose:  Implementation of GDALBridgeInitialize()
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 * Adapted from cplgetsymbol.cpp.
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
 ****************************************************************************/

/* ==================================================================== */
/*      We #define GDAL_ENTRY to nothing so that when the include 	*/
/*	file is include the real definition of the function pointer     */
/*      variables will occur in this files object file.                 */
/* ==================================================================== */

#define GDAL_ENTRY
#define GDAL_NULL	= NULL

#include "gdalbridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define PATH_SEP '\\'
static const char *papszSOFilenames[] = {
	 "gdal11.dll"
	,"gdal.1.0.dll"
	, NULL };
#else
#define PATH_SEP '/'
static const char *papszSOFilenames[] = {
	 "libgdal.1.1.so"
	,"gdal.1.0.so"
	,"gdal.so.1.0"
	,"libgdal.so.1"
	, NULL };
#endif

#define MAX_SYMBOL	1024

/************************************************************************/
/*                          GBGetSymbolCheck()                          */
/*                                                                      */
/*      Get a symbol, and on error add the missing entry point to a     */
/*      list of missing entry points.                                   */
/************************************************************************/

static void *GBGetSymbolCheck( const char *pszLibrary, 
                               const char *pszSymbolName,
                               char **papszErrorList )

{
    void	*pReturn;
    
    pReturn = GBGetSymbol( pszLibrary, pszSymbolName );

    if( pReturn == NULL && papszErrorList != NULL )
    {
        int	i;

        for( i = 0; papszErrorList[i] != NULL; i++ ) {}

        if( i < MAX_SYMBOL-1 )
        {
            papszErrorList[i] = strdup( pszSymbolName );
            papszErrorList[i+1] = NULL;
        }
    }

    return pReturn;
}

/************************************************************************/
/*                        GDALBridgeInitialize()                        */
/************************************************************************/

int GDALBridgeInitialize( const char * pszTargetDir, FILE *fpReportFailure )

{
    char	szPath[2048];
    void	*pfnTest = NULL;
    int		iSOFile;
    char        *apszFailed[MAX_SYMBOL];
    
/* -------------------------------------------------------------------- */
/*      Do we want to force reporting on?                               */
/* -------------------------------------------------------------------- */
    if( fpReportFailure == NULL
        && (getenv("CPL_DEBUG") != NULL || getenv("GB_DEBUG") != NULL) )
    {
        fpReportFailure = stderr;
    }

/* -------------------------------------------------------------------- */
/*      The first phase is to try and find the shared library.          */
/* -------------------------------------------------------------------- */
    for( iSOFile = 0;
         papszSOFilenames[iSOFile] != NULL && pfnTest == NULL;
         iSOFile++ )
    {
        if( pszTargetDir != NULL )
        {
            sprintf( szPath, "%s%c%s",
                     pszTargetDir, PATH_SEP, papszSOFilenames[iSOFile] );
            pfnTest = GBGetSymbol( szPath, "GDALOpen" );
        }

        if( pfnTest == NULL && getenv( "GDAL_HOME" ) != NULL )
        {
            sprintf( szPath,
                     "%s%c%s", getenv("GDAL_HOME"),
                     PATH_SEP, papszSOFilenames[iSOFile] );
            pfnTest = GBGetSymbol( szPath, "GDALOpen" );
        }

        if( pfnTest == NULL )
        {
            sprintf( szPath, papszSOFilenames[iSOFile] );
            pfnTest = GBGetSymbol( szPath, "GDALOpen" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Did we fail to even find the DLL/.so?                           */
/* -------------------------------------------------------------------- */
    if( pfnTest == NULL )
    {

        if( fpReportFailure == NULL )
            return FALSE;
        

        fprintf( fpReportFailure, 
                 "GBBridgeInitialize() failed to find an suitable GDAL .DLL/.so file.\n" );
        fprintf( fpReportFailure, 
                 "The following filenames were searched for:\n" );
        
        for( iSOFile = 0; papszSOFilenames[iSOFile] != NULL; iSOFile++ )
            fprintf( fpReportFailure, "  o %s\n", papszSOFilenames[iSOFile] );

        fprintf( fpReportFailure, "\n" );
        fprintf( fpReportFailure, "The following locations were searched:\n" );
        
        if( pszTargetDir != NULL )
            fprintf( fpReportFailure, "  o %s\n", pszTargetDir );
        
        if( getenv( "GDAL_HOME" ) != NULL )
            fprintf( fpReportFailure, "  o %s\n", getenv( "GDAL_HOME" ) );
        
        fprintf( fpReportFailure, "  o System default locations.\n" );
        fprintf( fpReportFailure, "\n" );

        fprintf( fpReportFailure, "\n" );
#ifdef __unix__        
        if( getenv("LD_LIBRARY_PATH") != NULL )
        {
            fprintf( fpReportFailure, 
                     "System default locations may be influenced by:\n" );
            fprintf( fpReportFailure, 
                     "LD_LIBRARY_PATH = %s\n", getenv("LD_LIBRARY_PATH") );
        }
#else
        if( getenv("PATH") != NULL )
        {
            fprintf( fpReportFailure, 
                     "System default locations may be influenced by:\n" );
            fprintf( fpReportFailure, 
                     "PATH = %s\n", getenv("PATH") );
        }
#endif        
        
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Start loading functions.                                        */
/* -------------------------------------------------------------------- */
    apszFailed[0] = NULL;
    
    GDALGetDataTypeSize = (int (*)(GDALDataType))
        GBGetSymbolCheck( szPath, "GDALGetDataTypeSize", apszFailed );

    GDALAllRegister = (void (*)(void)) 
        GBGetSymbolCheck( szPath, "GDALAllRegister", apszFailed );

    GDALCreate = (GDALDatasetH (*)(GDALDriverH, const char *, int, int, int,
                                   GDALDataType, char ** ))
        GBGetSymbolCheck( szPath, "GDALCreate", apszFailed );

    GDALOpen = (GDALDatasetH (*)(const char *, GDALAccess))
        GBGetSymbolCheck( szPath, "GDALOpen", apszFailed );

    GDALGetDriverByName = (GDALDriverH (*)(const char *))
        GBGetSymbolCheck( szPath, "GDALGetDriverByName", apszFailed );

    GDALGetDriverShortName = (const char *(*)(GDALDriverH))
        GBGetSymbolCheck( szPath, "GDALGetDriverShortName", apszFailed );

    GDALGetDriverLongName = (const char *(*)(GDALDriverH))
        GBGetSymbolCheck( szPath, "GDALGetDriverLongName", apszFailed );

    GDALGetDatasetDriver = (GDALDriverH (*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALGetDatasetDriver", apszFailed );

    GDALClose = (void (*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALClose", apszFailed );

    GDALGetRasterXSize = (int (*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALGetRasterXSize", apszFailed );

    GDALGetRasterYSize = (int (*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALGetRasterYSize", apszFailed );

    GDALGetRasterCount = (int (*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALGetRasterCount", apszFailed );

    GDALGetRasterBand = (GDALRasterBandH (*)(GDALDatasetH, int))
        GBGetSymbolCheck( szPath, "GDALGetRasterBand", apszFailed );

    GDALGetProjectionRef = (const char *(*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALGetProjectionRef", apszFailed );

    GDALSetProjection = (CPLErr (*)(GDALDatasetH, const char *))
        GBGetSymbolCheck( szPath, "GDALSetProjection", apszFailed );

    GDALGetGeoTransform = (CPLErr (*)(GDALDatasetH, double *))
        GBGetSymbolCheck( szPath, "GDALGetGeoTransform", apszFailed );

    GDALSetGeoTransform = (CPLErr (*)(GDALDatasetH, double *))
        GBGetSymbolCheck( szPath, "GDALSetGeoTransform", apszFailed );

    GDALGetInternalHandle = (void *(*)(GDALDatasetH, const char *))
        GBGetSymbolCheck( szPath, "GDALGetInternalHandle", apszFailed );

    GDALGetGCPCount = (int (*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALGetGCPCount", apszFailed );

    GDALGetGCPProjection = (const char *(*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALGetGCPProjection", apszFailed );

    GDALGetGCPs = (const GDAL_GCP *(*)(GDALDatasetH))
        GBGetSymbolCheck( szPath, "GDALGetGCPs", apszFailed );

    GDALGetRasterDataType = (GDALDataType (*)(GDALRasterBandH))
        GBGetSymbolCheck( szPath, "GDALGetRasterDataType", apszFailed );

    GDALGetRasterBandXSize = (int (*)(GDALRasterBandH))
        GBGetSymbolCheck( szPath, "GDALGetRasterBandXSize", apszFailed );

    GDALGetRasterBandYSize = (int (*)(GDALRasterBandH))
        GBGetSymbolCheck( szPath, "GDALGetRasterBandYSize", apszFailed );

    GDALGetBlockSize = (void (*)(GDALRasterBandH, int *, int *))
        GBGetSymbolCheck( szPath, "GDALGetBlockSize", apszFailed );

    GDALRasterIO = (CPLErr (*)(GDALRasterBandH, GDALRWFlag, int, int, int, int,
                               void *, int, int, GDALDataType, int, int ))
        GBGetSymbolCheck( szPath, "GDALRasterIO", apszFailed );

    GDALReadBlock = (CPLErr (*)(GDALRasterBandH, int, int, void *))
        GBGetSymbolCheck( szPath, "GDALReadBlock", apszFailed );
    
    GDALWriteBlock = (CPLErr (*)(GDALRasterBandH, int, int, void *))
        GBGetSymbolCheck( szPath, "GDALWriteBlock", apszFailed );

    GDALGetOverviewCount = (int (*)(GDALRasterBandH))
        GBGetSymbolCheck( szPath, "GDALGetOverviewCount", apszFailed );

    GDALGetOverview = (GDALRasterBandH (*)(GDALRasterBandH, int))
        GBGetSymbolCheck( szPath, "GDALGetOverview", apszFailed );

    GDALGetRasterNoDataValue = (double (*)(GDALRasterBandH, int*))
        GBGetSymbolCheck( szPath, "GDALGetRasterNoDataValue", apszFailed );

    GDALSetRasterNoDataValue = (CPLErr (*)(GDALRasterBandH, double))
        GBGetSymbolCheck( szPath, "GDALSetRasterNoDataValue", apszFailed );

    GDALFillRaster = (CPLErr (*)(GDALRasterBandH, double, double))
        GBGetSymbolCheck( szPath, "GDALFillRaster", apszFailed );

    GDALGetRasterMinimum = (double (*)(GDALRasterBandH, int *))
        GBGetSymbolCheck( szPath, "GDALGetRasterMinimum", apszFailed );
        
    GDALGetRasterMaximum = (double (*)(GDALRasterBandH, int *))
        GBGetSymbolCheck( szPath, "GDALGetRasterMaximum", apszFailed );
        
    GDALComputeRasterMinMax = (void (*)(GDALRasterBandH, int, double *))
        GBGetSymbolCheck( szPath, "GDALComputeRasterMinMax", apszFailed );
        
    GDALGetRasterColorInterpretation = (GDALColorInterp (*)(GDALRasterBandH))
        GBGetSymbolCheck( szPath, "GDALGetRasterColorInterpretation", apszFailed );

    GDALGetColorInterpretationName = (const char *(*)(GDALColorInterp))
        GBGetSymbolCheck( szPath, "GDALGetColorInterpretationName", apszFailed );

    GDALGetRasterColorTable = (GDALColorTableH (*)(GDALRasterBandH))
        GBGetSymbolCheck( szPath, "GDALGetRasterColorTable", apszFailed );

    GDALDecToDMS = (const char *(*)(double, const char *, int ))
        GBGetSymbolCheck( szPath, "GDALDecToDMS", apszFailed );

    GDALGetPaletteInterpretation = (GDALPaletteInterp (*)(GDALColorTableH))
        GBGetSymbolCheck( szPath, "GDALGetPaletteInterpretation", apszFailed );

    GDALGetPaletteInterpretationName = (const char *(*)(GDALPaletteInterp))
        GBGetSymbolCheck( szPath, "GDALGetPaletteInterpretationName", apszFailed );

    GDALGetColorEntryCount = (int (*)(GDALColorTableH))
        GBGetSymbolCheck( szPath, "GDALGetColorEntryCount", apszFailed );

    GDALGetColorEntry = (const GDALColorEntry *(*)(GDALColorTableH,int))
        GBGetSymbolCheck( szPath, "GDALGetColorEntry", apszFailed );

    GDALGetColorEntryAsRGB = (int (*)(GDALColorTableH,int,
                                      GDALColorEntry*))
        GBGetSymbolCheck( szPath, "GDALGetColorEntryAsRGB", apszFailed );
    
    GDALSetColorEntry = (void (*)(GDALColorTableH, int, const GDALColorEntry*))
        GBGetSymbolCheck( szPath, "GDALSetColorEntry", apszFailed );

/* -------------------------------------------------------------------- */
/*      GDALMajorObject                                                 */
/* -------------------------------------------------------------------- */
    GDALGetMetadata = (char **(*)(GDALMajorObjectH, const char *))
        GBGetSymbolCheck( szPath, "GDALGetMetadata", apszFailed );
    
    GDALSetMetadata = (CPLErr(*)(GDALMajorObjectH, char **, const char *))
        GBGetSymbolCheck( szPath, "GDALSetMetadata", apszFailed );
    
    GDALGetMetadataItem = (const char *(*)(GDALMajorObjectH, const char *,
                                           const char *))
        GBGetSymbolCheck( szPath, "GDALGetMetadataItem", apszFailed );

    GDALSetMetadataItem = (CPLErr (*)(GDALMajorObjectH, const char *, 
                                      const char *, const char *))
        GBGetSymbolCheck( szPath, "GDALSetMetadataItem", apszFailed );
    
/* -------------------------------------------------------------------- */
/*      CPL                                                             */
/* -------------------------------------------------------------------- */
    CPLErrorReset = (void (*)())
        GBGetSymbolCheck( szPath, "CPLErrorReset", apszFailed );

    CPLGetLastErrorNo = (int (*)())
        GBGetSymbolCheck( szPath, "CPLGetLastErrorNo", apszFailed );

    CPLGetLastErrorType = (CPLErr (*)())
        GBGetSymbolCheck( szPath, "CPLGetLastErrorType", apszFailed );

    CPLGetLastErrorMsg = (const char *(*)())
        GBGetSymbolCheck( szPath, "CPLGetLastErrorMsg", apszFailed );

    CPLPushErrorHandler = (void (*)(CPLErrorHandler))
        GBGetSymbolCheck( szPath, "CPLPushErrorHandler", apszFailed );

    CPLPopErrorHandler = (void (*)())
        GBGetSymbolCheck( szPath, "CPLPopErrorHandler", apszFailed );

/* -------------------------------------------------------------------- */
/*      OSR API                                                         */
/* -------------------------------------------------------------------- */
    OSRNewSpatialReference = (OGRSpatialReferenceH (*)( const char * ))
        GBGetSymbolCheck( szPath, "OSRNewSpatialReference", apszFailed );

    OSRCloneGeogCS = (OGRSpatialReferenceH (*)(OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OSRCloneGeogCS", apszFailed );

    OSRDestroySpatialReference = (void (*)(OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OSRDestroySpatialReference", apszFailed );

    OSRReference = (int (*)(OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OSRReference", apszFailed );

    OSRDereference = (int (*)(OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OSRDereference", apszFailed );

    OSRImportFromEPSG = (OGRErr (*)(OGRSpatialReferenceH,int))
        GBGetSymbolCheck( szPath, "OSRImportFromEPSG", apszFailed );

    OSRImportFromWkt = (OGRErr (*)(OGRSpatialReferenceH,char **))
        GBGetSymbolCheck( szPath, "OSRImportFromWkt", apszFailed );

    OSRImportFromProj4 = (OGRErr (*)(OGRSpatialReferenceH,const char *))
        GBGetSymbolCheck( szPath, "OSRImportFromProj4", apszFailed );

    OSRExportToWkt = (OGRErr (*)(OGRSpatialReferenceH, char **))
        GBGetSymbolCheck( szPath, "OSRExportToWkt", apszFailed );
    
    OSRExportToPrettyWkt = (OGRErr (*)(OGRSpatialReferenceH, char **, int))
        GBGetSymbolCheck( szPath, "OSRExportToPrettyWkt", apszFailed );
    
    OSRExportToProj4 = (OGRErr (*)(OGRSpatialReferenceH, char **))
        GBGetSymbolCheck( szPath, "OSRExportToProj4", apszFailed );
    
    OSRSetAttrValue = (OGRErr (*)(OGRSpatialReferenceH, const char *, 
                                  const char *))
        GBGetSymbolCheck( szPath, "OSRSetAttrValue", apszFailed );
    
    OSRGetAttrValue = (const char *(*)(OGRSpatialReferenceH, const char *,int))
        GBGetSymbolCheck( szPath, "OSRGetAttrValue", apszFailed );
    
    OSRSetLinearUnits = (OGRErr (*)(OGRSpatialReferenceH, const char *,double))
        GBGetSymbolCheck( szPath, "OSRSetLinearUnits", apszFailed );
    
    OSRGetLinearUnits = (double (*)(OGRSpatialReferenceH, char **))
        GBGetSymbolCheck( szPath, "OSRGetLinearUnits", apszFailed );
    
    OSRIsGeographic = (int (*)(OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OSRIsGeographic", apszFailed );
    
    OSRIsProjected = (int (*)(OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OSRIsProjected", apszFailed );
    
    OSRIsSameGeogCS = (int (*)(OGRSpatialReferenceH,OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OSRIsSameGeogCS", apszFailed );
    
    OSRIsSame = (int (*)(OGRSpatialReferenceH,OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OSRIsSame", apszFailed );
    
    OSRSetProjCS = (OGRErr (*)(OGRSpatialReferenceH,const char*))
        GBGetSymbolCheck( szPath, "OSRSetProjCS", apszFailed );

    OSRSetWellKnownGeogCS = (OGRErr (*)(OGRSpatialReferenceH, const char *))
        GBGetSymbolCheck( szPath, "OSRSetWellKnownGeogCS", apszFailed );

    OSRSetGeogCS = (OGRErr (*)( OGRSpatialReferenceH hSRS,
                                const char * pszGeogName,
                                const char * pszDatumName,
                                const char * pszEllipsoidName,
                                double dfSemiMajor, double dfInvFlattening,
                                const char * pszPMName /* = NULL */,
                                double dfPMOffset /* = 0.0 */,
                                const char * pszUnits /* = NULL */,
                                double dfConvertToRadians /* = 0.0 */ ))
        GBGetSymbolCheck( szPath, "OSRSetGeogCS", apszFailed );
        
    OSRGetSemiMajor = (double (*)(OGRSpatialReferenceH, OGRErr *))
        GBGetSymbolCheck( szPath, "OSRGetSemiMajor", apszFailed );

    OSRGetSemiMinor = (double (*)(OGRSpatialReferenceH, OGRErr *))
        GBGetSymbolCheck( szPath, "OSRGetSemiMinor", apszFailed );

    OSRGetInvFlattening = (double (*)(OGRSpatialReferenceH, OGRErr *))
        GBGetSymbolCheck( szPath, "OSRGetInvFlattening", apszFailed );

    OSRSetAuthority = (OGRErr (*)(OGRSpatialReferenceH, const char *, 
                                  const char *, int))
        GBGetSymbolCheck( szPath, "OSRSetAuthority", apszFailed );

    OSRSetProjParm = (OGRErr (*)(OGRSpatialReferenceH, const char *, double))
        GBGetSymbolCheck( szPath, "OSRSetProjParm", apszFailed );

    OSRGetProjParm = (double (*)(OGRSpatialReferenceH, const char *, 
                                 double, OGRErr *))
        GBGetSymbolCheck( szPath, "OSRGetProjParm", apszFailed );

    OSRSetUTM = (OGRErr (*)(OGRSpatialReferenceH, int, int))
        GBGetSymbolCheck( szPath, "OSRSetUTM", apszFailed );

    OSRGetUTMZone = (int (*)(OGRSpatialReferenceH, int *))
        GBGetSymbolCheck( szPath, "OSRGetUTMZone", apszFailed );

    OCTNewCoordinateTransformation = (OGRCoordinateTransformationH 
                     (*)(OGRSpatialReferenceH, OGRSpatialReferenceH))
        GBGetSymbolCheck( szPath, "OCTNewCoordinateTransformation",apszFailed);

    OCTDestroyCoordinateTransformation = 
        (void (*)(OGRCoordinateTransformationH))
        GBGetSymbolCheck( szPath, "OCTDestroyCoordinateTransformation",
                          apszFailed );

    OCTTransform = (int (*)(OGRCoordinateTransformationH, int, 
                            double *, double *, double *))
        GBGetSymbolCheck( szPath, "OCTTransform", apszFailed );

/* -------------------------------------------------------------------- */
/*      Did we fail to find any entry points?                           */
/* -------------------------------------------------------------------- */
    if( apszFailed[0] != NULL && fpReportFailure != NULL )
    {
        int	iError;
        
        fprintf( fpReportFailure, 
                 "While a GDAL .DLL/.so was found at `%s'\n"
                 "it appears to be missing the following entry points.\n"
                 "Consider upgrading to a more recent GDAL library.\n",
                 szPath );
        
        for( iError = 0; apszFailed[iError] != NULL; iError++ )
        {
            fprintf( fpReportFailure, "  o %s\n", apszFailed[iError] );
            free( apszFailed[iError] );
        }
    }

    return apszFailed[0] == NULL;
}


