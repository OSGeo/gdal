/* ****************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Dump overviews to external files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 2005, Frank Warmerdam
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

static void DumpBand( GDALDatasetH hBaseDS, GDALRasterBandH hBand,
                      const char *pszName );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/
static void Usage() 

{
    printf( "Usage: dumpoverviews [-masks] <filename> [overview]*\n" );
    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    const char *pszSrcFilename = NULL;
    int iArg;
    int anReqOverviews[1000];
    int nReqOverviewCount = 0;
    int bMasks = FALSE;

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Process arguments.                                              */
/* -------------------------------------------------------------------- */
    for( iArg = 1; iArg < argc; iArg++ )
    {
        if( EQUAL(argv[iArg],"-masks") )
        {
            bMasks = TRUE;
        }
        else if( pszSrcFilename == NULL )
        {
            pszSrcFilename = argv[iArg];
        }
        else if( atoi(argv[iArg]) > 0 || EQUAL(argv[iArg],"0") )
        {
            anReqOverviews[nReqOverviewCount++] = atoi(argv[iArg]);
        }
        else
        {
            Usage();
        }
    }

    if( pszSrcFilename == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open the input file.                                            */
/* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS = GDALOpen( pszSrcFilename, GA_ReadOnly );

    if( hSrcDS == NULL )
        exit( 1 );

/* ==================================================================== */
/*      Process all bands.                                              */
/* ==================================================================== */
    int iBand;
    int nBandCount = GDALGetRasterCount( hSrcDS );

    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBandH hBaseBand = GDALGetRasterBand( hSrcDS, iBand+1 );
        
/* -------------------------------------------------------------------- */
/*      Process all overviews.                                          */
/* -------------------------------------------------------------------- */
        int iOverview;
        int nOverviewCount = GDALGetOverviewCount( hBaseBand );
        
        for( iOverview = 0; iOverview < nOverviewCount; iOverview++ )
        {
            GDALRasterBandH hSrcOver = GDALGetOverview( hBaseBand, iOverview );
            
            if (hSrcOver == NULL)
            {
                fprintf(stderr, "skipping overview %d as being null\n", iOverview);
                continue;
            }

/* -------------------------------------------------------------------- */
/*      Is this a requested overview?                                   */
/* -------------------------------------------------------------------- */
            if( nReqOverviewCount > 0 )
            {
                int i; 

                for( i = 0; i < nReqOverviewCount; i++ )
                {
                    if( anReqOverviews[i] == iOverview )
                        break;
                }
                
                if( i == nReqOverviewCount )
                    continue;
            }

/* -------------------------------------------------------------------- */
/*      Create matching output file.                                    */
/* -------------------------------------------------------------------- */
            CPLString osFilename;
            osFilename.Printf( "%s_%d_%d.tif",
                               CPLGetBasename(pszSrcFilename),
                               iBand+1, iOverview );
            DumpBand( hSrcDS, hSrcOver, osFilename );

            if( bMasks )
            {
                CPLString osFilename;
                osFilename.Printf( "%s_%d_%d_mask.tif",
                                CPLGetBasename(pszSrcFilename),
                                iBand+1, iOverview );
                DumpBand( hSrcDS, GDALGetMaskBand(hSrcOver), osFilename );
            }
        }

/* -------------------------------------------------------------------- */
/*      Do we dump the mask?                                            */
/* -------------------------------------------------------------------- */
        if( bMasks )
        {
            CPLString osFilename;
            osFilename.Printf( "%s_%d_mask.tif",
                               CPLGetBasename(pszSrcFilename),
                               iBand+1 );
            DumpBand( hSrcDS, GDALGetMaskBand(hBaseBand), osFilename );
        }
    }

    GDALClose( hSrcDS );

    CSLDestroy( argv );
    GDALDestroyDriverManager();
    
    return 0;
}

/************************************************************************/
/*                              DumpBand()                              */
/************************************************************************/

static void DumpBand( GDALDatasetH hBaseDS, GDALRasterBandH hSrcOver,
                      const char *pszName )

{
    int nOrigXSize, nOrigYSize;
    double adfGeoTransform[6];
    int    bHaveGT;

/* -------------------------------------------------------------------- */
/*      Get base ds info.                                               */
/* -------------------------------------------------------------------- */
    bHaveGT = GDALGetGeoTransform( hBaseDS, adfGeoTransform ) == CE_None;
    nOrigXSize = GDALGetRasterXSize( hBaseDS );
    nOrigYSize = GDALGetRasterYSize( hBaseDS );

/* -------------------------------------------------------------------- */
/*      Create matching output file.                                    */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS;
    int nXSize = GDALGetRasterBandXSize( hSrcOver );
    int nYSize = GDALGetRasterBandYSize( hSrcOver );
    GDALDataType eDT = GDALGetRasterDataType( hSrcOver );
    GDALDriverH hDriver = GDALGetDriverByName( "GTiff" );

    hDstDS = GDALCreate( hDriver, pszName, nXSize, nYSize,
                         1, eDT, NULL );

    if( hDstDS == NULL )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Apply corresponding georeferencing, scaled to size.             */
/* -------------------------------------------------------------------- */
    if( bHaveGT )
    {
        double adfOvGeoTransform[6];
        
        memcpy( adfOvGeoTransform, adfGeoTransform, 
                sizeof(double) * 6 );
        
        adfOvGeoTransform[1] *= (nOrigXSize / (double) nXSize);
        adfOvGeoTransform[2] *= (nOrigXSize / (double) nXSize);
        adfOvGeoTransform[4] *= (nOrigYSize / (double) nYSize);
        adfOvGeoTransform[5] *= (nOrigYSize / (double) nYSize);
            
        GDALSetGeoTransform( hDstDS, adfOvGeoTransform );
            
        GDALSetProjection( hDstDS, GDALGetProjectionRef( hBaseDS ) );
    }

/* -------------------------------------------------------------------- */
/*      Copy over all the image data.                                   */
/* -------------------------------------------------------------------- */
    void *pData = CPLMalloc(64 * nXSize);
    int iLine;

    for( iLine = 0; iLine < nYSize; iLine++ )
    {
        GDALRasterIO( hSrcOver, GF_Read, 0, iLine, nXSize, 1, 
                      pData, nXSize, 1, eDT, 0, 0 );
        GDALRasterIO( GDALGetRasterBand( hDstDS, 1 ), GF_Write, 
                      0, iLine, nXSize, 1, 
                      pData, nXSize, 1, eDT, 0, 0 );
    }
    CPLFree( pData );

    GDALClose( hDstDS );
}
