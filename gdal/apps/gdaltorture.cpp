/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line utility to torture GDAL API on datasets
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include <cassert>

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf("Usage: gdaltorture [-r] [-u] [-rw] files*\n");
    exit(1);
}

/************************************************************************/
/*                              TortureDS()                             */
/************************************************************************/

static void TortureBand( GDALRasterBandH hBand, int /* bReadWriteOperations */,
                         int nRecurse )
{
    if( nRecurse > 5 )
        return;

    GDALGetRasterDataType(hBand);
    int nBlockXSize;
    int nBlockYSize;
    GDALGetBlockSize(hBand, &nBlockXSize, &nBlockYSize);
    // GDALRasterAdviseRead
    // GDALRasterIO
    // GDALReadBlock
    // GDALWriteBlock
    const int nRasterXSize = GDALGetRasterBandXSize(hBand);
    assert(nRasterXSize >= 0);
    const int nRasterYSize = GDALGetRasterBandYSize(hBand);
    assert(nRasterYSize >= 0);
    GDALGetRasterAccess(hBand);
    GDALGetBandNumber(hBand);
    GDALGetBandDataset(hBand);
    GDALGetRasterColorInterpretation(hBand);
    // GDALSetRasterColorInterpretation
    GDALGetRasterColorTable(hBand);
    // GDALSetRasterColorTable
    GDALHasArbitraryOverviews (hBand);

    const int nOverviewCount = GDALGetOverviewCount(hBand);
    for(int iOverview=0;iOverview<nOverviewCount;iOverview++)
    {
        GDALRasterBandH hOverviewBand = GDALGetOverview(hBand, iOverview);
        if( hOverviewBand )
            TortureBand(hOverviewBand, false, nRecurse + 1);
    }

    int bHasNoData;
    GDALGetRasterNoDataValue(hBand, &bHasNoData);
    // GDALSetRasterNoDataValue
    GDALGetRasterCategoryNames(hBand);
    // GDALSetRasterCategoryNames
    int bSuccess;
    GDALGetRasterMinimum(hBand, &bSuccess);
    GDALGetRasterMaximum(hBand, &bSuccess);
    double dfMin, dfMax, dfMean, dfStdDev;
    GDALGetRasterStatistics(hBand, TRUE, FALSE, &dfMin, &dfMax,
                            &dfMean, &dfStdDev);
    // GDALComputeRasterStatistics
    // GDALSetRasterStatistics
    GDALGetRasterUnitType(hBand);
    GDALGetRasterOffset(hBand, &bSuccess);
    // GDALSetRasterOffset
    GDALGetRasterScale(hBand, &bSuccess);
    // GDALSetRasterScale
    // double adfMinMax[2];
    // GDALComputeRasterMinMax(hBand, TRUE, adfMinMax);
    // GDALFlushRasterCache(hBand)
    // GDALGetDefaultHistogram(
    //     GDALRasterBandH hBand, double *pdfMin, double *pdfMax,
    //     int *pnBuckets, int **ppanHistogram, int bForce,
    //     GDALProgressFunc pfnProgress, void *pProgressData);
    // GDALSetDefaultHistogram(
    //     GDALRasterBandH hBand, double dfMin, double dfMax,
    //     int nBuckets, int *panHistogram);
    float afSampleBuf;
    GDALGetRandomRasterSample(hBand, 1, &afSampleBuf);
    GDALGetRasterSampleOverview(hBand, 0); // returns a hBand
    // GDALFillRaster
    // GDALComputeBandStats
    // GDALOverviewMagnitudeCorrection
    GDALGetDefaultRAT(hBand);
    // GDALSetDefaultRAT
    // GDALAddDerivedBandPixelFunc
    GDALRasterBandH hMaskBand = GDALGetMaskBand(hBand);
    if( hMaskBand != hBand )
        TortureBand(hMaskBand, false, nRecurse + 1);
    GDALGetMaskFlags(hBand);
    // GDALCreateMaskBand
}

/************************************************************************/
/*                              TortureDS()                             */
/************************************************************************/

static void TortureDS( const char *pszTarget, bool bReadWriteOperations )
{
    // hDS = GDALOpen(pszTarget, GA_Update);
    // GDALClose(hDS);

    GDALDatasetH hDS = GDALOpen(pszTarget, GA_ReadOnly);
    if( hDS == nullptr )
        return;

    // GDALGetMetadata(GDALMajorObjectH, const char *);
    // GDALSetMetadata(GDALMajorObjectH, char **, const char *);
    // GDALGetMetadataItem(GDALMajorObjectH, const char *, const char *);
    // GDALSetMetadataItem(GDALMajorObjectH, const char *, const char *,
    //                     const char *);
    GDALGetDescription(hDS);
    //GDALSetDescription
    GDALGetDatasetDriver(hDS);
    char **papszFileList = GDALGetFileList(hDS);
    CSLDestroy(papszFileList);
    const int nXSize = GDALGetRasterXSize(hDS);
    assert(nXSize >= 0);
    const int nYSize = GDALGetRasterYSize(hDS);
    assert(nYSize >= 0);
    const int nBands = GDALGetRasterCount(hDS);
    // GDALAddBand
    // GDALDatasetRasterIO
    GDALGetProjectionRef(hDS);
    // GDALSetProjection
    double adfGeotransform[6] = {};
    GDALGetGeoTransform(hDS, adfGeotransform);
    // GDALSetGeoTransform
    GDALGetGCPCount(hDS);
    GDALGetGCPProjection(hDS);
    GDALGetGCPs(hDS);
    // GDALSetGCPs
    // GDALGetInternalHandle
    GDALReferenceDataset(hDS);
    GDALDereferenceDataset(hDS);
    // GDALBuildOverviews
    GDALGetAccess(hDS);
    // GDALFlushCache
    // GDALCreateDatasetMaskBand
    // GDALDatasetCopyWholeRaster

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        GDALRasterBandH hBand = GDALGetRasterBand(hDS, iBand + 1);
        if( hBand == nullptr )
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
                                  bool bRecursive, bool bReportFailures,
                                  bool bReadWriteOperations )
{
    GDALDriverH hDriver = GDALIdentifyDriver(pszTarget, papszSiblingList);

    if( hDriver != nullptr )
    {
        printf("%s: %s\n", pszTarget, GDALGetDriverShortName(hDriver));
        TortureDS(pszTarget, bReadWriteOperations);
    }
    else if( bReportFailures )
    {
        printf("%s: unrecognized\n", pszTarget);
    }

    if( !bRecursive || hDriver != nullptr )
        return;

    VSIStatBufL sStatBuf;
    if( VSIStatL(pszTarget, &sStatBuf) != 0 ||
        !VSI_ISDIR(sStatBuf.st_mode) )
        return;

    papszSiblingList = VSIReadDir(pszTarget);
    for( int i = 0; papszSiblingList && papszSiblingList[i]; i++ )
    {
        if( EQUAL(papszSiblingList[i],"..") ||
            EQUAL(papszSiblingList[i],".") )
            continue;

        const CPLString osSubTarget =
            CPLFormFilename(pszTarget, papszSiblingList[i], nullptr);

        ProcessTortureTarget(osSubTarget, papszSiblingList,
                             bRecursive, bReportFailures, bReadWriteOperations);
    }
    CSLDestroy(papszSiblingList);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char **argv )
{
    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if( argc < 1 )
        exit(-argc);

    if( argc < 2 )
        Usage();

/* -------------------------------------------------------------------- */
/*      Scan for command line switches                                   */
/* -------------------------------------------------------------------- */
    char** papszArgv = argv + 1;
    argc--;

    bool bRecursive = false;
    bool bReportFailures = false;
    bool bReadWriteOperations = false;

    while( argc > 0 && papszArgv[0][0] == '-' )
    {
        if( EQUAL(papszArgv[0], "-r") )
            bRecursive = true;
        else if( EQUAL(papszArgv[0], "-u") )
            bReportFailures = true;
        else if( EQUAL(papszArgv[0], "-rw") )
            bReadWriteOperations = true;
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
        ProcessTortureTarget(papszArgv[0], nullptr,
                             bRecursive, bReportFailures, bReadWriteOperations);
        argc--;
        papszArgv++;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CSLDestroy(argv);
    GDALDestroyDriverManager();

    return 0;
}
