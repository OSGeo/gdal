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
	 "gdal.1.0.dll"
	, NULL };
#else
#define PATH_SEP '/'
static const char *papszSOFilenames[] = {
	 "gdal.1.0.so"
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

    GDALGetRasterDataType = (GDALDataType (*)(GDALRasterBandH))
        GBGetSymbol( szPath, "GDALGetRasterDataType" );

    GDALGetBlockSize = (void (*)(GDALRasterBandH, int *, int *))
        GBGetSymbol( szPath, "GDALGetBlockSize" );

    GDALRasterIO = (CPLErr (*)(GDALRasterBandH, GDALRWFlag, int, int, int, int,
                               void *, int, int, GDALDataType, int, int ))
        GBGetSymbol( szPath, "GDALRasterIO" );

    GDALReadBlock = (CPLErr (*)(GDALRasterBandH, int, int, void *))
        GBGetSymbol( szPath, "GDALReadBlock" );
    
    GDALWriteBlock = (CPLErr (*)(GDALRasterBandH, int, int, void *))
        GBGetSymbol( szPath, "GDALWriteBlock" );

    GDALCreateProjDef = (GDALProjDefH (*)(const char *))
        GBGetSymbol( szPath, "GDALCreateProjDef" );

    GDALReprojectToLongLat = (CPLErr (*)(GDALProjDefH, double *, double *))
        GBGetSymbol( szPath, "GDALReprojectToLongLat" );
    
    GDALReprojectFromLongLat = (CPLErr (*)(GDALProjDefH, double *, double *))
        GBGetSymbol( szPath, "GDALReprojectFromLongLat" );

    GDALDestroyProjDef = (void (*)(GDALProjDefH *))
        GBGetSymbol( szPath, "GDALDestroyProjDef" );

    GDALDecToDMS = (const char *(*)(double, const char *, int ))
        GBGetSymbol( szPath, "GDALDecToDMS" );

    return TRUE;
}

