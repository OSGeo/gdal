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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.12  2001/09/06 01:54:31  warmerda
 * added gcp functions
 *
 * Revision 1.10  2000/09/26 15:20:32  warmerda
 * added GDALGetRasterBand{X,Y}Size
 *
 * Revision 1.9  2000/09/01 19:12:09  warmerda
 * fixed const mismatch
 *
 * Revision 1.8  2000/08/28 20:16:14  warmerda
 * added lots of OGRSpatialReference stuff
 *
 * Revision 1.7  2000/08/25 20:03:40  warmerda
 * added more entry points
 *
 * Revision 1.6  1999/09/17 03:18:08  warmerda
 * change to search for a list of GDAL .so/.dll files
 *
 * Revision 1.5  1999/05/07 14:08:49  warmerda
 * change .so name
 *
 * Revision 1.4  1999/04/22 13:35:11  warmerda
 * Fixed copyright header.
 *
 */

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


/************************************************************************/
/*                        GDALBridgeInitialize()                        */
/************************************************************************/

int GDALBridgeInitialize( const char * pszTargetDir )

{
    char	szPath[2048];
    void	*pfnTest = NULL;
    int		iSOFile;
    
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

    if( pfnTest == NULL )
        return FALSE;
    
/* -------------------------------------------------------------------- */
/*      Start loading functions.                                        */
/* -------------------------------------------------------------------- */

    GDALGetDataTypeSize = (int (*)(GDALDataType))
        GBGetSymbol( szPath, "GDALGetDataTypeSize" );

    GDALAllRegister = (void (*)(void)) 
        GBGetSymbol( szPath, "GDALAllRegister" );

    GDALCreate = (GDALDatasetH (*)(GDALDriverH, const char *, int, int, int,
                                   GDALDataType, char ** ))
        GBGetSymbol( szPath, "GDALCreate" );

    GDALOpen = (GDALDatasetH (*)(const char *, GDALAccess))
        GBGetSymbol( szPath, "GDALOpen" );

    GDALGetDriverByName = (GDALDriverH (*)(const char *))
        GBGetSymbol( szPath, "GDALGetDriverByName" );

    GDALClose = (void (*)(GDALDatasetH))
        GBGetSymbol( szPath, "GDALClose" );

    GDALGetRasterXSize = (int (*)(GDALDatasetH))
        GBGetSymbol( szPath, "GDALGetRasterXSize" );

    GDALGetRasterYSize = (int (*)(GDALDatasetH))
        GBGetSymbol( szPath, "GDALGetRasterYSize" );

    GDALGetRasterCount = (int (*)(GDALDatasetH))
        GBGetSymbol( szPath, "GDALGetRasterCount" );

    GDALGetRasterBand = (GDALRasterBandH (*)(GDALDatasetH, int))
        GBGetSymbol( szPath, "GDALGetRasterBand" );

    GDALGetProjectionRef = (const char *(*)(GDALDatasetH))
        GBGetSymbol( szPath, "GDALGetProjectionRef" );

    GDALSetProjection = (CPLErr (*)(GDALDatasetH, const char *))
        GBGetSymbol( szPath, "GDALSetProjection" );

    GDALGetGeoTransform = (CPLErr (*)(GDALDatasetH, double *))
        GBGetSymbol( szPath, "GDALGetGeoTransform" );

    GDALSetGeoTransform = (CPLErr (*)(GDALDatasetH, double *))
        GBGetSymbol( szPath, "GDALSetGeoTransform" );

    GDALGetInternalHandle = (void *(*)(GDALDatasetH, const char *))
        GBGetSymbol( szPath, "GDALGetInternalHandle" );

    GDALGetGCPCount = (int (*)(GDALDatasetH))
        GBGetSymbol( szPath, "GDALGetGCPCount" );

    GDALGetGCPProjection = (const char *(*)(GDALDatasetH))
        GBGetSymbol( szPath, "GDALGetGCPProjection" );

    GDALGetGCPs = (const GDAL_GCP *(*)(GDALDatasetH))
        GBGetSymbol( szPath, "GDALGetGCPs" );

    GDALGetRasterDataType = (GDALDataType (*)(GDALRasterBandH))
        GBGetSymbol( szPath, "GDALGetRasterDataType" );

    GDALGetRasterBandXSize = (int (*)(GDALRasterBandH))
        GBGetSymbol( szPath, "GDALGetRasterBandXSize" );

    GDALGetRasterBandYSize = (int (*)(GDALRasterBandH))
        GBGetSymbol( szPath, "GDALGetRasterBandYSize" );

    GDALGetBlockSize = (void (*)(GDALRasterBandH, int *, int *))
        GBGetSymbol( szPath, "GDALGetBlockSize" );

    GDALRasterIO = (CPLErr (*)(GDALRasterBandH, GDALRWFlag, int, int, int, int,
                               void *, int, int, GDALDataType, int, int ))
        GBGetSymbol( szPath, "GDALRasterIO" );

    GDALReadBlock = (CPLErr (*)(GDALRasterBandH, int, int, void *))
        GBGetSymbol( szPath, "GDALReadBlock" );
    
    GDALWriteBlock = (CPLErr (*)(GDALRasterBandH, int, int, void *))
        GBGetSymbol( szPath, "GDALWriteBlock" );

    GDALGetOverviewCount = (int (*)(GDALRasterBandH))
        GBGetSymbol( szPath, "GDALGetOverviewCount" );

    GDALGetOverview = (GDALRasterBandH (*)(GDALRasterBandH, int))
        GBGetSymbol( szPath, "GDALGetOverview" );

    GDALGetRasterNoDataValue = (double (*)(GDALRasterBandH, int*))
        GBGetSymbol( szPath, "GDALGetRasterNoDataValue" );

    GDALSetRasterNoDataValue = (CPLErr (*)(GDALRasterBandH, double))
        GBGetSymbol( szPath, "GDALSetRasterNoDataValue" );

    GDALGetRasterColorInterpretation = (GDALColorInterp (*)(GDALRasterBandH))
        GBGetSymbol( szPath, "GDALGetRasterColorInterpretation" );

    GDALGetColorInterpretationName = (const char *(*)(GDALColorInterp))
        GBGetSymbol( szPath, "GDALGetColorInterpretationName" );

    GDALGetRasterColorTable = (GDALColorTableH (*)(GDALRasterBandH))
        GBGetSymbol( szPath, "GDALGetRasterColorTable" );

    GDALCreateProjDef = (GDALProjDefH (*)(const char *))
        GBGetSymbol( szPath, "GDALCreateProjDef" );

    GDALReprojectToLongLat = (CPLErr (*)(GDALProjDefH, double *, double *))
        GBGetSymbol( szPath, "GDALReprojectToLongLat" );
    
    GDALReprojectFromLongLat = (CPLErr (*)(GDALProjDefH, double *, double *))
        GBGetSymbol( szPath, "GDALReprojectFromLongLat" );

    GDALDestroyProjDef = (void (*)(GDALProjDefH))
        GBGetSymbol( szPath, "GDALDestroyProjDef" );

    GDALDecToDMS = (const char *(*)(double, const char *, int ))
        GBGetSymbol( szPath, "GDALDecToDMS" );

    GDALGetPaletteInterpretation = (GDALPaletteInterp (*)(GDALColorTableH))
        GBGetSymbol( szPath, "GDALGetPaletteInterpretation" );

    GDALGetPaletteInterpretationName = (const char *(*)(GDALPaletteInterp))
        GBGetSymbol( szPath, "GDALGetPaletteInterpretationName" );

    GDALGetColorEntryCount = (int (*)(GDALColorTableH))
        GBGetSymbol( szPath, "GDALGetColorEntryCount" );

    GDALGetColorEntry = (const GDALColorEntry *(*)(GDALColorTableH,int))
        GBGetSymbol( szPath, "GDALGetColorEntry" );

    GDALGetColorEntryAsRGB = (int (*)(GDALColorTableH,int,
                                      GDALColorEntry*))
        GBGetSymbol( szPath, "GDALGetColorEntryAsRGB" );
    
    GDALSetColorEntry = (void (*)(GDALColorTableH, int, const GDALColorEntry*))
        GBGetSymbol( szPath, "GDALSetColorEntry" );

/* -------------------------------------------------------------------- */
/*      OSR API                                                         */
/* -------------------------------------------------------------------- */
    OSRNewSpatialReference = (OGRSpatialReferenceH (*)( const char * ))
        GBGetSymbol( szPath, "OSRNewSpatialReference" );

    OSRCloneGeogCS = (OGRSpatialReferenceH (*)(OGRSpatialReferenceH))
        GBGetSymbol( szPath, "OSRCloneGeogCS" );

    OSRDestroySpatialReference = (void (*)(OGRSpatialReferenceH))
        GBGetSymbol( szPath, "OSRDestroySpatialReference" );

    OSRReference = (int (*)(OGRSpatialReferenceH))
        GBGetSymbol( szPath, "OSRReference" );

    OSRDereference = (int (*)(OGRSpatialReferenceH))
        GBGetSymbol( szPath, "OSRDereference" );

    OSRImportFromEPSG = (OGRErr (*)(OGRSpatialReferenceH,int))
        GBGetSymbol( szPath, "OSRImportFromEPSG" );

    OSRImportFromWkt = (OGRErr (*)(OGRSpatialReferenceH,char **))
        GBGetSymbol( szPath, "OSRImportFromWkt" );

    OSRImportFromProj4 = (OGRErr (*)(OGRSpatialReferenceH,const char *))
        GBGetSymbol( szPath, "OSRImportFromProj4" );

    OSRExportToWkt = (OGRErr (*)(OGRSpatialReferenceH, char **))
        GBGetSymbol( szPath, "OSRExportToWkt" );
    
    OSRExportToPrettyWkt = (OGRErr (*)(OGRSpatialReferenceH, char **, int))
        GBGetSymbol( szPath, "OSRExportToPrettyWkt" );
    
    OSRExportToProj4 = (OGRErr (*)(OGRSpatialReferenceH, char **))
        GBGetSymbol( szPath, "OSRExportToProj4" );
    
    OSRSetAttrValue = (OGRErr (*)(OGRSpatialReferenceH, const char *, 
                                  const char *))
        GBGetSymbol( szPath, "OSRSetAttrValue" );
    
    OSRGetAttrValue = (const char *(*)(OGRSpatialReferenceH, const char *,int))
        GBGetSymbol( szPath, "OSRGetAttrValue" );
    
    OSRSetLinearUnits = (OGRErr (*)(OGRSpatialReferenceH, const char *,double))
        GBGetSymbol( szPath, "OSRSetLinearUnits" );
    
    OSRGetLinearUnits = (double (*)(OGRSpatialReferenceH, char **))
        GBGetSymbol( szPath, "OSRGetLinearUnits" );
    
    OSRIsGeographic = (int (*)(OGRSpatialReferenceH))
        GBGetSymbol( szPath, "OSRIsGeographic" );
    
    OSRIsProjected = (int (*)(OGRSpatialReferenceH))
        GBGetSymbol( szPath, "OSRIsProjected" );
    
    OSRIsSameGeogCS = (int (*)(OGRSpatialReferenceH,OGRSpatialReferenceH))
        GBGetSymbol( szPath, "OSRIsSameGeogCS" );
    
    OSRIsSame = (int (*)(OGRSpatialReferenceH,OGRSpatialReferenceH))
        GBGetSymbol( szPath, "OSRIsSame" );
    
    OSRSetProjCS = (OGRErr (*)(OGRSpatialReferenceH,const char*))
        GBGetSymbol( szPath, "OSRSetProjCS" );

    OSRSetWellKnownGeogCS = (OGRErr (*)(OGRSpatialReferenceH, const char *))
        GBGetSymbol( szPath, "OSRSetWellKnownGeogCS" );

    OSRSetGeogCS = (OGRErr (*)( OGRSpatialReferenceH hSRS,
                                const char * pszGeogName,
                                const char * pszDatumName,
                                const char * pszEllipsoidName,
                                double dfSemiMajor, double dfInvFlattening,
                                const char * pszPMName /* = NULL */,
                                double dfPMOffset /* = 0.0 */,
                                const char * pszUnits /* = NULL */,
                                double dfConvertToRadians /* = 0.0 */ ))
        GBGetSymbol( szPath, "OSRSetGeogCS" );
        
    OSRGetSemiMajor = (double (*)(OGRSpatialReferenceH, OGRErr *))
        GBGetSymbol( szPath, "OSRGetSemiMajor" );

    OSRGetSemiMinor = (double (*)(OGRSpatialReferenceH, OGRErr *))
        GBGetSymbol( szPath, "OSRGetSemiMinor" );

    OSRGetInvFlattening = (double (*)(OGRSpatialReferenceH, OGRErr *))
        GBGetSymbol( szPath, "OSRGetInvFlattening" );

    OSRSetAuthority = (OGRErr (*)(OGRSpatialReferenceH, const char *, 
                                  const char *, int))
        GBGetSymbol( szPath, "OSRSetAuthority" );

    OSRSetProjParm = (OGRErr (*)(OGRSpatialReferenceH, const char *, double))
        GBGetSymbol( szPath, "OSRSetProjParm" );

    OSRGetProjParm = (double (*)(OGRSpatialReferenceH, const char *, 
                                 double, OGRErr *))
        GBGetSymbol( szPath, "OSRGetProjParm" );

    OSRSetUTM = (OGRErr (*)(OGRSpatialReferenceH, int, int))
        GBGetSymbol( szPath, "OSRSetUTM" );

    OSRGetUTMZone = (int (*)(OGRSpatialReferenceH, int *))
        GBGetSymbol( szPath, "OSRGetUTMZone" );

    return TRUE;
}

