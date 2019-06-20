/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to do image enhancement.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gdal_version.h"
#include "gdal.h"
#include "vrtdataset.h"
#include "commonutils.h"

#include <algorithm>

CPL_CVSID("$Id$")

static int
ComputeEqualizationLUTs( GDALDatasetH hDataset,  int nLUTBins,
                         double **ppadfScaleMin, double **padfScaleMax,
                         int ***ppapanLUTs, GDALProgressFunc pfnProgress );

static CPLErr EnhancerCallback( void *hCBData,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void *pData );

typedef struct {
    GDALRasterBand *poSrcBand;
    GDALDataType    eWrkType;
    double          dfScaleMin;
    double          dfScaleMax;
    int             nLUTBins;
    const int      *panLUT;
} EnhanceCBInfo;

/* ******************************************************************** */
/*                               Usage()                                */
/* ******************************************************************** */

static void Usage()

{
    printf( "Usage: gdalenhance [--help-general]\n"
            "       [-of format] [-co \"NAME=VALUE\"]*\n"
            "       [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
            "             CInt16/CInt32/CFloat32/CFloat64}]\n"
//            "       [-src_scale[_n] src_min src_max]\n"
//            "       [-dst_scale[_n] dst_min dst_max]\n"
//            "       [-lutbins count]\n"
//            "       [-s_nodata[_n] value]\n"
//            "       [-stddev multiplier]\n"
            "       [-equalize]\n"
            "       [-config filename]\n"
            "       src_dataset dst_dataset\n\n" );
    printf( "%s\n\n", GDALVersionInfo( "--version" ) );
    exit( 1 );
}

/************************************************************************/
/*                             ProxyMain()                              */
/************************************************************************/

MAIN_START(argc, argv)

{
    GDALDatasetH        hDataset, hOutDS;
    int                 i;
    const char          *pszSource=nullptr, *pszDest=nullptr, *pszFormat = nullptr;
    GDALDriverH         hDriver = nullptr;
    GDALDataType        eOutputType = GDT_Unknown;
    char                **papszCreateOptions = nullptr;
    GDALProgressFunc    pfnProgress = GDALTermProgress;
    int                 nLUTBins = 256;
    const char         *pszMethod = "minmax";
//    double              dfStdDevMult = 0.0;
    double             *padfScaleMin = nullptr;
    double             *padfScaleMax = nullptr;
    int               **papanLUTs = nullptr;
    int                 iBand;
    const char         *pszConfigFile = nullptr;

    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(argv[0]))
        exit(1);
/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( i < argc-1 && (EQUAL(argv[i],"-of") || EQUAL(argv[i],"-f")) )
        {
            pszFormat = argv[++i];
        }

        else if( i < argc-1 && EQUAL(argv[i],"-ot") )
        {
            int iType;

            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName(static_cast<GDALDataType>(iType)) != nullptr
                    && EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                             argv[i+1]) )
                {
                    eOutputType = static_cast<GDALDataType>(iType);
                }
            }

            if( eOutputType == GDT_Unknown )
            {
                printf( "Unknown output pixel type: %s\n", argv[i+1] );
                Usage();
            }
            i++;
        }

        else if( STARTS_WITH_CI(argv[i], "-s_nodata") )
        {
            // TODO
            i += 1;
        }

        else if( i < argc-1 && EQUAL(argv[i],"-co") )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
        }

        else if( i < argc-1 && STARTS_WITH_CI(argv[i], "-src_scale") )
        {
            // TODO
            i += 2;
        }

        else if( i < argc-2 && STARTS_WITH_CI(argv[i], "-dst_scale") )
        {
            // TODO
            i += 2;
        }

        else if( i < argc-1 && EQUAL(argv[i],"-config") )
        {
            pszConfigFile = argv[++i];
        }

        else if( EQUAL(argv[i],"-equalize") )
        {
            pszMethod = "equalize";
        }

        else if( EQUAL(argv[i],"-quiet") )
        {
            pfnProgress = GDALDummyProgress;
        }

        else if( argv[i][0] == '-' )
        {
            printf( "Option %s incomplete, or not recognised.\n\n",
                    argv[i] );
            Usage();
        }
        else if( pszSource == nullptr )
        {
            pszSource = argv[i];
        }
        else if( pszDest == nullptr )
        {
            pszDest = argv[i];
        }

        else
        {
            printf( "Too many command options.\n\n" );
            Usage();
        }
    }

    if( pszSource == nullptr )
    {
        Usage();
    }

/* -------------------------------------------------------------------- */
/*      Attempt to open source file.                                    */
/* -------------------------------------------------------------------- */

    hDataset = GDALOpenShared( pszSource, GA_ReadOnly );

    if( hDataset == nullptr )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        GDALDestroyDriverManager();
        exit( 1 );
    }

    int nBandCount = GDALGetRasterCount(hDataset);

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    CPLString osFormat;
    if( pszFormat == nullptr && pszDest != nullptr )
    {
        osFormat = GetOutputDriverForRaster(pszDest);
        if( osFormat.empty() )
        {
            GDALDestroyDriverManager();
            exit( 1 );
        }
    }
    else if( pszFormat != nullptr )
    {
        osFormat = pszFormat;
    }

    if( !osFormat.empty() )
    {
        hDriver = GDALGetDriverByName( osFormat );
        if( hDriver == nullptr )
        {
            int iDr;

            printf( "Output driver `%s' not recognised.\n", osFormat.c_str() );
            printf( "The following format drivers are configured and support output:\n" );
            for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
            {
                hDriver = GDALGetDriver(iDr);

                if( GDALGetMetadataItem( hDriver, GDAL_DCAP_RASTER, nullptr) != nullptr &&
                    (GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, nullptr ) != nullptr
                    || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, nullptr ) != nullptr) )
                {
                    printf( "  %s: %s\n",
                            GDALGetDriverShortName( hDriver  ),
                            GDALGetDriverLongName( hDriver ) );
                }
            }
            printf( "\n" );
            Usage();
        }
    }

/* -------------------------------------------------------------------- */
/*      If histogram equalization is requested, do it now.              */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszMethod,"equalize") )
    {
        ComputeEqualizationLUTs( hDataset, nLUTBins,
                                 &padfScaleMin, &padfScaleMax,
                                 &papanLUTs, pfnProgress );
    }

/* -------------------------------------------------------------------- */
/*      If we have a config file, assume it is for input and read       */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    else if( pszConfigFile != nullptr )
    {
        char **papszLines = CSLLoad( pszConfigFile );
        if( CSLCount(papszLines) == 0 )
            exit( 1 );

        if( CSLCount(papszLines) != nBandCount )
        {
            fprintf( stderr, "Did not get %d lines in config file as expected.\n", nBandCount );
            exit( 1 );
        }

        padfScaleMin =
            static_cast<double *>(CPLCalloc(nBandCount,sizeof(double)));
        padfScaleMax =
            static_cast<double *>(CPLCalloc(nBandCount,sizeof(double)));

        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            int iLUT;
            char **papszTokens = CSLTokenizeString( papszLines[iBand] );

            if( CSLCount(papszTokens) < 3
                || atoi(papszTokens[0]) != iBand+1 )
            {
                fprintf( stderr, "Line %d seems to be corrupt.\n", iBand+1 );
                exit( 1 );
            }

            // Process scale min/max

            padfScaleMin[iBand] = CPLAtof(papszTokens[1]);
            padfScaleMax[iBand] = CPLAtof(papszTokens[2]);

            if( CSLCount(papszTokens) == 3 )
                continue;

            // process lut
            if( papanLUTs == nullptr )
            {
                nLUTBins = CSLCount(papszTokens) - 3;
                papanLUTs =
                    static_cast<int **>(CPLCalloc(sizeof(int*), nBandCount));
            }

            papanLUTs[iBand] =
                static_cast<int *>(CPLCalloc(nLUTBins, sizeof(int)));

            for( iLUT = 0; iLUT < nLUTBins; iLUT++ )
                papanLUTs[iBand][iLUT] = atoi(papszTokens[iLUT+3]);

            CSLDestroy( papszTokens );
        }
    }

/* -------------------------------------------------------------------- */
/*      If there is no destination, just report the scaling values      */
/*      and luts.                                                       */
/* -------------------------------------------------------------------- */
    if( pszDest == nullptr )
    {
        FILE *fpConfig = stdout;
        if( pszConfigFile )
            fpConfig = fopen( pszConfigFile, "w" );

        for( iBand = 0; iBand < nBandCount; iBand++ )
        {
            fprintf( fpConfig, "%d:Band ", iBand+1 );
            if( padfScaleMin != nullptr )
                fprintf( fpConfig, "%g:ScaleMin %g:ScaleMax ",
                         padfScaleMin[iBand], padfScaleMax[iBand] );

            if( papanLUTs )
            {
                int iLUT;

                for( iLUT = 0; iLUT < nLUTBins; iLUT++ )
                    fprintf( fpConfig, "%d ", papanLUTs[iBand][iLUT] );
            }
            fprintf( fpConfig, "\n" );
        }

        if( pszConfigFile )
            fclose( fpConfig );

        exit( 0 );
    }

    if (padfScaleMin == nullptr || padfScaleMax == nullptr)
    {
        fprintf( stderr, "-equalize or -config filename command line options must be specified.\n");
        exit(1);
    }

/* ==================================================================== */
/*      Create a virtual dataset.                                       */
/* ==================================================================== */
    EnhanceCBInfo *pasEInfo = static_cast<EnhanceCBInfo *>(
        CPLCalloc(nBandCount, sizeof(EnhanceCBInfo)));

/* -------------------------------------------------------------------- */
/*      Make a virtual clone.                                           */
/* -------------------------------------------------------------------- */
    VRTDataset *poVDS =
        new VRTDataset(GDALGetRasterXSize(hDataset),
                       GDALGetRasterYSize(hDataset));

    if( GDALGetGCPCount(hDataset) == 0 )
    {
        double adfGeoTransform[6];

        const char *pszProjection = GDALGetProjectionRef(hDataset);
        if( pszProjection != nullptr && strlen(pszProjection) > 0 )
            poVDS->SetProjection( pszProjection );

        if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
            poVDS->SetGeoTransform( adfGeoTransform );
    }
    else
    {
        poVDS->SetGCPs( GDALGetGCPCount(hDataset),
                        GDALGetGCPs(hDataset),
                        GDALGetGCPProjection( hDataset ) );
    }

    poVDS->SetMetadata( static_cast<GDALDataset*>(hDataset)->GetMetadata() );

    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        VRTSourcedRasterBand   *poVRTBand;
        GDALRasterBand  *poSrcBand;
        GDALDataType    eBandType;

        poSrcBand = static_cast<GDALDataset*>(hDataset)->GetRasterBand(iBand+1);

/* -------------------------------------------------------------------- */
/*      Select output data type to match source.                        */
/* -------------------------------------------------------------------- */
        if( eOutputType == GDT_Unknown )
            eBandType = GDT_Byte;
        else
            eBandType = eOutputType;

/* -------------------------------------------------------------------- */
/*      Create this band.                                               */
/* -------------------------------------------------------------------- */
        poVDS->AddBand( eBandType, nullptr );
        poVRTBand = cpl::down_cast<VRTSourcedRasterBand *>(poVDS->GetRasterBand( iBand+1 ));

/* -------------------------------------------------------------------- */
/*      Create a function based source with info on how to apply the    */
/*      enhancement.                                                    */
/* -------------------------------------------------------------------- */
        pasEInfo[iBand].poSrcBand = poSrcBand;
        pasEInfo[iBand].eWrkType = eBandType;
        pasEInfo[iBand].dfScaleMin = padfScaleMin[iBand];
        pasEInfo[iBand].dfScaleMax = padfScaleMax[iBand];
        pasEInfo[iBand].nLUTBins = nLUTBins;

        if( papanLUTs )
            pasEInfo[iBand].panLUT = papanLUTs[iBand];

        poVRTBand->AddFuncSource( EnhancerCallback, pasEInfo + iBand );

/* -------------------------------------------------------------------- */
/*      copy over some other information of interest.                   */
/* -------------------------------------------------------------------- */
        poVRTBand->CopyCommonInfoFrom( poSrcBand );
    }

/* -------------------------------------------------------------------- */
/*      Write to the output file using CopyCreate().                    */
/* -------------------------------------------------------------------- */
    hOutDS = GDALCreateCopy( hDriver, pszDest, static_cast<GDALDatasetH>(poVDS),
                             FALSE, papszCreateOptions,
                             pfnProgress, nullptr );
    if( hOutDS != nullptr )
        GDALClose( hOutDS );

    GDALClose(poVDS);

    GDALClose(hDataset);

/* -------------------------------------------------------------------- */
/*      Cleanup and exit.                                               */
/* -------------------------------------------------------------------- */
    GDALDumpOpenDatasets( stderr );
    GDALDestroyDriverManager();
    CSLDestroy( argv );
    CSLDestroy( papszCreateOptions );

    exit( 0 );
}
MAIN_END

/************************************************************************/
/*                      ComputeEqualizationLUTs()                       */
/*                                                                      */
/*      Get an image histogram, and compute equalization luts from      */
/*      it.                                                             */
/************************************************************************/

static int
ComputeEqualizationLUTs( GDALDatasetH hDataset, int nLUTBins,
                         double **ppadfScaleMin, double **ppadfScaleMax,
                         int ***ppapanLUTs,
                         GDALProgressFunc pfnProgress )

{
    int iBand;
    int nBandCount = GDALGetRasterCount(hDataset);
    int nHistSize = 0;
    GUIntBig *panHistogram = nullptr;

    // For now we always compute min/max
    *ppadfScaleMin =
        static_cast<double *>(CPLCalloc(sizeof(double), nBandCount));
    *ppadfScaleMax =
        static_cast<double *>(CPLCalloc(sizeof(double), nBandCount));

    *ppapanLUTs = static_cast<int **>(CPLCalloc(sizeof(int *), nBandCount));

/* ==================================================================== */
/*      Process all bands.                                              */
/* ==================================================================== */
    for( iBand = 0; iBand < nBandCount; iBand++ )
    {
        GDALRasterBandH hBand = GDALGetRasterBand( hDataset, iBand+1 );
        CPLErr eErr;

/* -------------------------------------------------------------------- */
/*      Get a reasonable histogram.                                     */
/* -------------------------------------------------------------------- */
        eErr =
            GDALGetDefaultHistogramEx( hBand,
                                     *ppadfScaleMin + iBand,
                                     *ppadfScaleMax + iBand,
                                     &nHistSize, &panHistogram,
                                     TRUE, pfnProgress, nullptr );

        if( eErr != CE_None )
            return FALSE;

        panHistogram[0] = 0; // zero out extremes (nodata, etc)
        panHistogram[nHistSize-1] = 0;

/* -------------------------------------------------------------------- */
/*      Total histogram count, and build cumulative histogram.          */
/*      We take care to use big integers as there may be more than 4    */
/*      Gigapixels.                                                     */
/* -------------------------------------------------------------------- */
        GUIntBig *panCumHist =
            static_cast<GUIntBig *>(CPLCalloc(sizeof(GUIntBig), nHistSize));
        GUIntBig nTotal = 0;
        int iHist;

        for( iHist = 0; iHist < nHistSize; iHist++ )
        {
            panCumHist[iHist] = nTotal + panHistogram[iHist]/2;
            nTotal += panHistogram[iHist];
        }

        CPLFree( panHistogram );

        if( nTotal == 0 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Zero value entries in histogram, results will not be meaningful." );
            nTotal = 1;
        }

/* -------------------------------------------------------------------- */
/*      Now compute a LUT from the cumulative histogram.                */
/* -------------------------------------------------------------------- */
        int *panLUT = static_cast<int *>(CPLCalloc(sizeof(int), nLUTBins));
        int iLUT;

        for( iLUT = 0; iLUT < nLUTBins; iLUT++ )
        {
            iHist = (iLUT * nHistSize) / nLUTBins;
            int nValue =
                static_cast<int>((panCumHist[iHist] * nLUTBins) / nTotal);

            panLUT[iLUT] = std::max(0, std::min(nLUTBins - 1, nValue));
        }

        (*ppapanLUTs)[iBand] = panLUT;
    }

    return TRUE;
}

/************************************************************************/
/*                          EnhancerCallback()                          */
/*                                                                      */
/*      This is the VRT callback that actually does the image rescaling.*/
/************************************************************************/

static CPLErr EnhancerCallback( void *hCBData,
                                int nXOff, int nYOff, int nXSize, int nYSize,
                                void *pData )

{
    const EnhanceCBInfo *psEInfo = static_cast<const EnhanceCBInfo *>(hCBData);

    if( psEInfo->eWrkType != GDT_Byte )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Currently gdalenhance only supports Byte output." );
        exit( 2 );
    }

    GByte *pabyOutImage = static_cast<GByte *>(pData);
    CPLErr eErr;
    float *pafSrcImage =
        static_cast<float *>(CPLCalloc(sizeof(float), nXSize * nYSize));

    eErr = psEInfo->poSrcBand->
        RasterIO( GF_Read, nXOff, nYOff, nXSize, nYSize,
                  pafSrcImage, nXSize, nYSize, GDT_Float32, 0, 0, nullptr );

    if( eErr != CE_None )
    {
        CPLFree( pafSrcImage );
        return eErr;
    }

    int nPixelCount = nXSize * nYSize;
    int iPixel;
    int bHaveNoData;
    float fNoData = static_cast<float>(psEInfo->poSrcBand->GetNoDataValue( &bHaveNoData ));
    double dfScale =
        psEInfo->nLUTBins / (psEInfo->dfScaleMax - psEInfo->dfScaleMin);

    for( iPixel = 0; iPixel < nPixelCount; iPixel++ )
    {
        if( bHaveNoData && pafSrcImage[iPixel] == fNoData )
        {
            pabyOutImage[iPixel] = static_cast<GByte>(fNoData);
            continue;
        }

        int iBin = static_cast<int>(
            (pafSrcImage[iPixel] - psEInfo->dfScaleMin) * dfScale);
        iBin = std::max(0, std::min(psEInfo->nLUTBins - 1, iBin));

        if( psEInfo->panLUT )
            pabyOutImage[iPixel] = static_cast<GByte>(psEInfo->panLUT[iBin]);
        else
            pabyOutImage[iPixel] = static_cast<GByte>(iBin);
    }

    CPLFree( pafSrcImage );

    return CE_None;
}
