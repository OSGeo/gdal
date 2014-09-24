/******************************************************************************
 * $Id: gdaltorture.cpp  $
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline utility to torture GDAL API on datasets
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal.h"
#include "cpl_string.h"
#include "cpl_conv.h"

CPL_CVSID("$Id: gdaltorture.cpp $");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: gdaltorture [-r] [-u] [-rw] files*\n" );
    exit( 1 );
}

/************************************************************************/
/*                              TortureDS()                             */
/************************************************************************/

static void TortureBand(GDALRasterBandH hBand, int bReadWriteOperations, int nRecurse)
{
    int             nBlockXSize, nBlockYSize;
    int             nRasterXSize, nRasterYSize;
    int             iOverview, nOverviewCount;
    int             bHasNoData;
    int             bSuccess;
    double          dfMin, dfMax, dfMean, dfStdDev;
    //double          adfMinMax[2];
    float           afSampleBuf;
    GDALRasterBandH hMaskBand;

    if (nRecurse > 5)
        return;

    GDALGetRasterDataType(hBand);
    GDALGetBlockSize(hBand, &nBlockXSize, &nBlockYSize);
    //GDALRasterAdviseRead
    //GDALRasterIO
    //GDALReadBlock 
    //GDALWriteBlock
    nRasterXSize = GDALGetRasterBandXSize(hBand);
    nRasterYSize = GDALGetRasterBandYSize(hBand);
    GDALGetRasterAccess(hBand);
    GDALGetBandNumber(hBand);
    GDALGetBandDataset(hBand);
    GDALGetRasterColorInterpretation(hBand);
    // GDALSetRasterColorInterpretation
    GDALGetRasterColorTable(hBand);
    //GDALSetRasterColorTable
    GDALHasArbitraryOverviews (hBand);

    nOverviewCount = GDALGetOverviewCount(hBand);
    for(iOverview=0;iOverview<nOverviewCount;iOverview++)
    {
        GDALRasterBandH hOverviewBand = GDALGetOverview(hBand, iOverview);
        if (hOverviewBand)
            TortureBand(hOverviewBand, FALSE, nRecurse + 1);
    }

    GDALGetRasterNoDataValue(hBand, &bHasNoData);
    //GDALSetRasterNoDataValue 
    GDALGetRasterCategoryNames(hBand);
    //GDALSetRasterCategoryNames
    GDALGetRasterMinimum(hBand, &bSuccess);
    GDALGetRasterMaximum(hBand, &bSuccess);
    GDALGetRasterStatistics(hBand, TRUE, FALSE, &dfMin, &dfMax, &dfMean, &dfStdDev);
    //GDALComputeRasterStatistics
    //GDALSetRasterStatistics 
    GDALGetRasterUnitType(hBand);
    GDALGetRasterOffset(hBand, &bSuccess);
    //GDALSetRasterOffset
    GDALGetRasterScale(hBand, &bSuccess);
    //GDALSetRasterScale
    //GDALComputeRasterMinMax(hBand, TRUE, adfMinMax);
    //GDALFlushRasterCache(hBand)
    //GDALGetDefaultHistogram (GDALRasterBandH hBand, double *pdfMin, double *pdfMax, int *pnBuckets, int **ppanHistogram, int bForce, GDALProgressFunc pfnProgress, void *pProgressData)
    //GDALSetDefaultHistogram (GDALRasterBandH hBand, double dfMin, double dfMax, int nBuckets, int *panHistogram)
    GDALGetRandomRasterSample(hBand, 1, &afSampleBuf);
    GDALGetRasterSampleOverview(hBand, 0); // returns a hBand
    //GDALFillRaster
    //GDALComputeBandStats
    //GDALOverviewMagnitudeCorrection 
    GDALGetDefaultRAT(hBand);
    //GDALSetDefaultRAT
    //GDALAddDerivedBandPixelFunc
    hMaskBand = GDALGetMaskBand(hBand);
    if (hMaskBand != hBand)
        TortureBand(hMaskBand, FALSE, nRecurse + 1);
    GDALGetMaskFlags(hBand);
    //GDALCreateMaskBand
    
}

/************************************************************************/
/*                              TortureDS()                             */
/************************************************************************/

static void TortureDS(const char *pszTarget, int bReadWriteOperations)
{
    GDALDatasetH    hDS;
    GDALRasterBandH hBand;
    int             nXSize, nYSize;
    int             nBands, iBand;
    double          adfGeotransform[6];
    char          **papszFileList;

    //hDS = GDALOpen(pszTarget, GA_Update);
    //GDALClose(hDS);

    hDS = GDALOpen(pszTarget, GA_ReadOnly);
    if (hDS == NULL)
        return;

    // GDALGetMetadata (GDALMajorObjectH, const char *)
    // GDALSetMetadata (GDALMajorObjectH, char **, const char *)
    // GDALGetMetadataItem (GDALMajorObjectH, const char *, const char *)
    // GDALSetMetadataItem (GDALMajorObjectH, const char *, const char *, const char *)
    GDALGetDescription(hDS);
    //GDALSetDescription
    GDALGetDatasetDriver(hDS);
    papszFileList = GDALGetFileList(hDS);
    CSLDestroy(papszFileList);
    nXSize = GDALGetRasterXSize(hDS);
    nYSize = GDALGetRasterYSize(hDS);
    nBands = GDALGetRasterCount(hDS);
    //GDALAddBand
    //GDALDatasetRasterIO
    GDALGetProjectionRef(hDS);
    //GDALSetProjection
    GDALGetGeoTransform(hDS, adfGeotransform);
    //GDALSetGeoTransform
    GDALGetGCPCount(hDS);
    GDALGetGCPProjection(hDS);
    GDALGetGCPs(hDS);
    // GDALSetGCPs
    // GDALGetInternalHandle
    GDALReferenceDataset(hDS);
    GDALDereferenceDataset(hDS);
    //GDALBuildOverviews
    GDALGetAccess(hDS);
    // GDALFlushCache
    // GDALCreateDatasetMaskBand
    // GDALDatasetCopyWholeRaster
    
    for(iBand=0;iBand<nBands;iBand++)
    {
        hBand = GDALGetRasterBand(hDS, iBand + 1);
        if (hBand == NULL)
            continue;

        TortureBand(hBand, bReadWriteOperations, 0);
    }

    GDALClose(hDS);
}


/************************************************************************/
/*                       ProcessTortureTarget()                         */
/************************************************************************/

static void ProcessTortureTarget( const char *pszTarget, 
                                  char **papszSiblingList, 
                                  int bRecursive, int bReportFailures,
                                  int bReadWriteOperations)

{
    GDALDriverH hDriver;
    VSIStatBufL sStatBuf;
    int i;

    hDriver = GDALIdentifyDriver( pszTarget, papszSiblingList );

    if( hDriver != NULL )
    {
        printf( "%s: %s\n", pszTarget, GDALGetDriverShortName( hDriver ) );
        TortureDS(pszTarget, bReadWriteOperations);
    }
    else if( bReportFailures )
        printf( "%s: unrecognised\n", pszTarget );

    if( !bRecursive || hDriver != NULL )
        return;

    if( VSIStatL( pszTarget, &sStatBuf ) != 0 
        || !VSI_ISDIR( sStatBuf.st_mode ) )
        return;

    papszSiblingList = VSIReadDir( pszTarget );
    for( i = 0; papszSiblingList && papszSiblingList[i]; i++ )
    {
        if( EQUAL(papszSiblingList[i],"..") 
            || EQUAL(papszSiblingList[i],".") )
            continue;

        CPLString osSubTarget = 
            CPLFormFilename( pszTarget, papszSiblingList[i], NULL );

        ProcessTortureTarget( osSubTarget, papszSiblingList, 
                               bRecursive, bReportFailures, bReadWriteOperations );
    }
    CSLDestroy(papszSiblingList);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    int bRecursive = FALSE, bReportFailures = FALSE, bReadWriteOperations = FALSE;
    char** papszArgv;

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    if( argc < 2 )
        Usage();

/* -------------------------------------------------------------------- */
/*      Scan for commandline switches                                   */
/* -------------------------------------------------------------------- */
    papszArgv = argv + 1;
    argc --;

    while( argc > 0 && papszArgv[0][0] == '-' )
    {
        if( EQUAL(papszArgv[0],"-r") )
            bRecursive = TRUE;
        else if( EQUAL(papszArgv[0],"-u") )
            bReportFailures = TRUE;
        else if( EQUAL(papszArgv[0],"-rw") )
            bReadWriteOperations = TRUE;
        else
            Usage();

        papszArgv++;
        argc--;
    }

/* -------------------------------------------------------------------- */
/*      Process given files.                                            */
/* -------------------------------------------------------------------- */
    while( argc > 0 )
    {
        ProcessTortureTarget( papszArgv[0], NULL, 
                              bRecursive, bReportFailures, bReadWriteOperations );
        argc--;
        papszArgv++;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CSLDestroy( argv );
    GDALDestroyDriverManager();

    return 0;
}

