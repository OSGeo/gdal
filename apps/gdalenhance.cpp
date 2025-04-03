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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gdal_version.h"
#include "gdal.h"
#include "vrtdataset.h"
#include "commonutils.h"

#include <algorithm>

static int ComputeEqualizationLUTs(GDALDatasetH hDataset, int nLUTBins,
                                   double **ppadfScaleMin,
                                   double **padfScaleMax, int ***ppapanLUTs,
                                   GDALProgressFunc pfnProgress);

static CPLErr ReadLUTs(const char *pszConfigFile, int nBandCount, int nLUTBins,
                       int ***ppapanLUTs, double **ppadfScaleMin,
                       double **ppadfScaleMax);
static void WriteLUTs(int **papanLUTs, int nBandCount, int nLUTBins,
                      double *padfScaleMin, double *padfScaleMax,
                      const char *pszConfigFile);
static CPLErr WriteEnhanced(GDALDatasetH hDataset, int **papanLUTs,
                            int nLUTBins, double *padfScaleMin,
                            double *padfScaleMax, GDALDataType eOutputType,
                            GDALDriverH hDriver, const char *pszDest,
                            char **papszCreateOptions,
                            GDALProgressFunc pfnProgress);

static CPLErr EnhancerCallback(void *hCBData, int nXOff, int nYOff, int nXSize,
                               int nYSize, void *pData);

typedef struct
{
    GDALRasterBand *poSrcBand;
    GDALDataType eWrkType;
    double dfScaleMin;
    double dfScaleMax;
    int nLUTBins;
    const int *panLUT;
} EnhanceCBInfo;

/* ******************************************************************** */
/*                               Usage()                                */
/* ******************************************************************** */

static void Usage()

{
    printf("Usage: gdalenhance [--help] [--help-general]\n"
           "       [-of <format>] [-co <NAME>=<VALUE>]...\n"
           "       [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
           "             CInt16/CInt32/CFloat32/CFloat64}]\n"
           //            "       [-src_scale[_n] src_min src_max]\n"
           //            "       [-dst_scale[_n] dst_min dst_max]\n"
           //            "       [-lutbins count]\n"
           //            "       [-s_nodata[_n] value]\n"
           //            "       [-stddev multiplier]\n"
           "       [-equalize]\n"
           "       [-config <filename>]\n"
           "       <src_dataset> <dst_dataset>\n\n");
    printf("%s\n\n", GDALVersionInfo("--version"));
    exit(1);
}

/************************************************************************/
/*                             ProxyMain()                              */
/************************************************************************/

MAIN_START(argc, argv)

{
    GDALDatasetH hDataset = nullptr;
    const char *pszSource = nullptr, *pszDest = nullptr, *pszFormat = nullptr;
    GDALDriverH hDriver = nullptr;
    GDALDataType eOutputType = GDT_Unknown;
    char **papszCreateOptions = nullptr;
    GDALProgressFunc pfnProgress = GDALTermProgress;
    int nBandCount = 0;
    int nLUTBins = 256;
    const char *pszMethod = "minmax";
    //    double              dfStdDevMult = 0.0;
    double *padfScaleMin = nullptr;
    double *padfScaleMax = nullptr;
    int **papanLUTs = nullptr;
    const char *pszConfigFile = nullptr;
    int nRetCode = 0;

    /* Check strict compilation and runtime library version as we use C++ API */
    if (!GDAL_CHECK_VERSION(argv[0]))
        exit(1);
    /* -------------------------------------------------------------------- */
    /*      Register standard GDAL drivers, and process generic GDAL        */
    /*      command options.                                                */
    /* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
    {
        GDALDestroyDriverManager();
        exit(0);
    }

    /* -------------------------------------------------------------------- */
    /*      Handle command line arguments.                                  */
    /* -------------------------------------------------------------------- */
    for (int i = 1; i < argc; i++)
    {
        if (EQUAL(argv[i], "--utility_version"))
        {
            printf("%s was compiled against GDAL %s and is running against "
                   "GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            goto exit;
        }
        else if (EQUAL(argv[i], "--help"))
        {
            Usage();
        }
        else if (i < argc - 1 &&
                 (EQUAL(argv[i], "-of") || EQUAL(argv[i], "-f")))
        {
            pszFormat = argv[++i];
        }

        else if (i < argc - 1 && EQUAL(argv[i], "-ot"))
        {
            for (int iType = 1; iType < GDT_TypeCount; iType++)
            {
                if (GDALGetDataTypeName(static_cast<GDALDataType>(iType)) !=
                        nullptr &&
                    EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                          argv[i + 1]))
                {
                    eOutputType = static_cast<GDALDataType>(iType);
                }
            }

            if (eOutputType == GDT_Unknown)
            {
                printf("Unknown output pixel type: %s\n", argv[i + 1]);
                Usage();
            }
            i++;
        }

        else if (STARTS_WITH_CI(argv[i], "-s_nodata"))
        {
            // TODO
            i += 1;
        }

        else if (i < argc - 1 && EQUAL(argv[i], "-co"))
        {
            papszCreateOptions = CSLAddString(papszCreateOptions, argv[++i]);
        }

        else if (i < argc - 1 && STARTS_WITH_CI(argv[i], "-src_scale"))
        {
            // TODO
            i += 2;
        }

        else if (i < argc - 2 && STARTS_WITH_CI(argv[i], "-dst_scale"))
        {
            // TODO
            i += 2;
        }

        else if (i < argc - 1 && EQUAL(argv[i], "-config"))
        {
            pszConfigFile = argv[++i];
        }

        else if (EQUAL(argv[i], "-equalize"))
        {
            pszMethod = "equalize";
        }

        else if (EQUAL(argv[i], "-quiet"))
        {
            pfnProgress = GDALDummyProgress;
        }

        else if (argv[i][0] == '-')
        {
            printf("Option %s incomplete, or not recognised.\n\n", argv[i]);
            Usage();
        }
        else if (pszSource == nullptr)
        {
            pszSource = argv[i];
        }
        else if (pszDest == nullptr)
        {
            pszDest = argv[i];
        }

        else
        {
            printf("Too many command options.\n\n");
            Usage();
        }
    }

    if (pszSource == nullptr)
    {
        Usage();
    }

    /* -------------------------------------------------------------------- */
    /*      Attempt to open source file.                                    */
    /* -------------------------------------------------------------------- */

    hDataset = GDALOpenShared(pszSource, GA_ReadOnly);

    if (hDataset == nullptr)
    {
        fprintf(stderr, "GDALOpen failed - %d\n%s\n", CPLGetLastErrorNo(),
                CPLGetLastErrorMsg());
        goto exit;
    }

    nBandCount = GDALGetRasterCount(hDataset);

    /* -------------------------------------------------------------------- */
    /*      Find the output driver.                                         */
    /* -------------------------------------------------------------------- */
    {
        CPLString osFormat;
        if (pszFormat == nullptr && pszDest != nullptr)
        {
            osFormat = GetOutputDriverForRaster(pszDest);
            if (osFormat.empty())
            {
                GDALDestroyDriverManager();
                exit(1);
            }
        }
        else if (pszFormat != nullptr)
        {
            osFormat = pszFormat;
        }

        if (!osFormat.empty())
        {
            hDriver = GDALGetDriverByName(osFormat);
            if (hDriver == nullptr)
            {
                int iDr;

                printf("Output driver `%s' not recognised.\n",
                       osFormat.c_str());
                printf("The following format drivers are enabled and support "
                       "writing:\n");
                for (iDr = 0; iDr < GDALGetDriverCount(); iDr++)
                {
                    hDriver = GDALGetDriver(iDr);

                    if (GDALGetMetadataItem(hDriver, GDAL_DCAP_RASTER,
                                            nullptr) != nullptr &&
                        (GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE,
                                             nullptr) != nullptr ||
                         GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY,
                                             nullptr) != nullptr))
                    {
                        printf("  %s: %s\n", GDALGetDriverShortName(hDriver),
                               GDALGetDriverLongName(hDriver));
                    }
                }
                printf("\n");
                goto exit;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If histogram equalization is requested, do it now.              */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszMethod, "equalize"))
    {
        ComputeEqualizationLUTs(hDataset, nLUTBins, &padfScaleMin,
                                &padfScaleMax, &papanLUTs, pfnProgress);
    }

    /* -------------------------------------------------------------------- */
    /*      If we have a config file, assume it is for input and read       */
    /*      it.                                                             */
    /* -------------------------------------------------------------------- */
    else if (pszConfigFile != nullptr)
    {
        if (ReadLUTs(pszConfigFile, nBandCount, nLUTBins, &papanLUTs,
                     &padfScaleMin, &padfScaleMax) != CE_None)
        {
            nRetCode = 1;
            goto exit;
        }
    }

    if (padfScaleMin == nullptr || padfScaleMax == nullptr)
    {
        fprintf(stderr, "-equalize or -config filename command line options "
                        "must be specified.\n");
        Usage();
    }

    /* -------------------------------------------------------------------- */
    /*      If there is no destination, just report the scaling values      */
    /*      and luts.                                                       */
    /* -------------------------------------------------------------------- */
    if (pszDest == nullptr)
    {
        WriteLUTs(papanLUTs, nBandCount, nLUTBins, padfScaleMin, padfScaleMax,
                  pszConfigFile);
    }
    else
    {
        if (WriteEnhanced(hDataset, papanLUTs, nLUTBins, padfScaleMin,
                          padfScaleMax, eOutputType, hDriver, pszDest,
                          papszCreateOptions, pfnProgress) != CE_None)
        {
            nRetCode = 1;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup and exit.                                               */
    /* -------------------------------------------------------------------- */
exit:
    GDALClose(hDataset);
    GDALDumpOpenDatasets(stderr);
    GDALDestroyDriverManager();
    CSLDestroy(argv);
    CSLDestroy(papszCreateOptions);
    if (papanLUTs)
    {
        for (int iBand = 0; iBand < nBandCount; iBand++)
        {
            CPLFree(papanLUTs[iBand]);
        }
        CPLFree(papanLUTs);
    }
    CPLFree(padfScaleMin);
    CPLFree(padfScaleMax);

    exit(nRetCode);
}

MAIN_END

/************************************************************************/
/*                      ComputeEqualizationLUTs()                       */
/*                                                                      */
/*      Get an image histogram, and compute equalization luts from      */
/*      it.                                                             */
/************************************************************************/

static int ComputeEqualizationLUTs(GDALDatasetH hDataset, int nLUTBins,
                                   double **ppadfScaleMin,
                                   double **ppadfScaleMax, int ***ppapanLUTs,
                                   GDALProgressFunc pfnProgress)

{
    int nBandCount = GDALGetRasterCount(hDataset);

    // For now we always compute min/max
    *ppadfScaleMin =
        static_cast<double *>(CPLCalloc(sizeof(double), nBandCount));
    *ppadfScaleMax =
        static_cast<double *>(CPLCalloc(sizeof(double), nBandCount));

    *ppapanLUTs = static_cast<int **>(CPLCalloc(sizeof(int *), nBandCount));

    /* ==================================================================== */
    /*      Process all bands.                                              */
    /* ==================================================================== */
    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        GDALRasterBandH hBand = GDALGetRasterBand(hDataset, iBand + 1);
        GUIntBig *panHistogram = nullptr;
        int nHistSize = 0;

        /* ----------------------------------------------------------------- */
        /*      Get a reasonable histogram.                                  */
        /* ----------------------------------------------------------------- */
        CPLErr eErr = GDALGetDefaultHistogramEx(
            hBand, *ppadfScaleMin + iBand, *ppadfScaleMax + iBand, &nHistSize,
            &panHistogram, TRUE, pfnProgress, nullptr);

        if (eErr != CE_None)
            return FALSE;

        panHistogram[0] = 0;  // zero out extremes (nodata, etc)
        panHistogram[nHistSize - 1] = 0;

        /* ----------------------------------------------------------------- */
        /*      Total histogram count, and build cumulative histogram.       */
        /*      We take care to use big integers as there may be more than 4 */
        /*      Gigapixels.                                                  */
        /* ----------------------------------------------------------------- */
        GUIntBig *panCumHist =
            static_cast<GUIntBig *>(CPLCalloc(sizeof(GUIntBig), nHistSize));
        GUIntBig nTotal = 0;

        for (int iHist = 0; iHist < nHistSize; iHist++)
        {
            panCumHist[iHist] = nTotal + panHistogram[iHist] / 2;
            nTotal += panHistogram[iHist];
        }

        CPLFree(panHistogram);

        if (nTotal == 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Zero value entries in histogram, results will not be "
                     "meaningful.");
            nTotal = 1;
        }

        /* ----------------------------------------------------------------- */
        /*      Now compute a LUT from the cumulative histogram.             */
        /* ----------------------------------------------------------------- */
        int *panLUT = static_cast<int *>(CPLCalloc(sizeof(int), nLUTBins));

        for (int iLUT = 0; iLUT < nLUTBins; iLUT++)
        {
            int iHist = (iLUT * nHistSize) / nLUTBins;
            int nValue =
                static_cast<int>((panCumHist[iHist] * nLUTBins) / nTotal);

            panLUT[iLUT] = std::max(0, std::min(nLUTBins - 1, nValue));
        }

        CPLFree(panCumHist);

        (*ppapanLUTs)[iBand] = panLUT;
    }

    return TRUE;
}

/************************************************************************/
/*                          EnhancerCallback()                          */
/*                                                                      */
/*      This is the VRT callback that actually does the image rescaling.*/
/************************************************************************/

static CPLErr EnhancerCallback(void *hCBData, int nXOff, int nYOff, int nXSize,
                               int nYSize, void *pData)

{
    const EnhanceCBInfo *psEInfo = static_cast<const EnhanceCBInfo *>(hCBData);

    if (psEInfo->eWrkType != GDT_Byte)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Currently gdalenhance only supports Byte output.");
        return CE_Failure;
    }

    GByte *pabyOutImage = static_cast<GByte *>(pData);
    float *pafSrcImage = static_cast<float *>(
        CPLCalloc(sizeof(float), static_cast<size_t>(nXSize) * nYSize));

    CPLErr eErr = psEInfo->poSrcBand->RasterIO(
        GF_Read, nXOff, nYOff, nXSize, nYSize, pafSrcImage, nXSize, nYSize,
        GDT_Float32, 0, 0, nullptr);

    if (eErr != CE_None)
    {
        CPLFree(pafSrcImage);
        return eErr;
    }

    int nPixelCount = nXSize * nYSize;
    int bHaveNoData;
    float fNoData =
        static_cast<float>(psEInfo->poSrcBand->GetNoDataValue(&bHaveNoData));
    double dfScale =
        psEInfo->nLUTBins / (psEInfo->dfScaleMax - psEInfo->dfScaleMin);

    for (int iPixel = 0; iPixel < nPixelCount; iPixel++)
    {
        if (bHaveNoData && pafSrcImage[iPixel] == fNoData)
        {
            pabyOutImage[iPixel] = static_cast<GByte>(fNoData);
            continue;
        }

        int iBin = static_cast<int>(
            (pafSrcImage[iPixel] - psEInfo->dfScaleMin) * dfScale);
        iBin = std::max(0, std::min(psEInfo->nLUTBins - 1, iBin));

        if (psEInfo->panLUT)
            pabyOutImage[iPixel] = static_cast<GByte>(psEInfo->panLUT[iBin]);
        else
            pabyOutImage[iPixel] = static_cast<GByte>(iBin);
    }

    CPLFree(pafSrcImage);

    return CE_None;
}

/************************************************************************/
/*                      ReadLUTs()                                      */
/*                                                                      */
/*               Read a LUT for each band from a file.                  */
/************************************************************************/

CPLErr ReadLUTs(const char *pszConfigFile, int nBandCount, int nLUTBins,
                int ***ppapanLUTs, double **ppadfScaleMin,
                double **ppadfScaleMax)
{
    const CPLStringList aosLines(CSLLoad(pszConfigFile));

    if (aosLines.size() != nBandCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Did not get %d lines in config file as expected.\n",
                 nBandCount);
        return CE_Failure;
    }

    *ppadfScaleMin =
        static_cast<double *>(CPLCalloc(nBandCount, sizeof(double)));
    *ppadfScaleMax =
        static_cast<double *>(CPLCalloc(nBandCount, sizeof(double)));
    *ppapanLUTs = static_cast<int **>(CPLCalloc(sizeof(int *), nBandCount));

    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        const CPLStringList aosTokens(CSLTokenizeString(aosLines[iBand]));

        if (aosTokens.size() < (nLUTBins + 3) ||
            atoi(aosTokens[0]) != iBand + 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Line %d seems to be corrupt.\n", iBand + 1);
            return CE_Failure;
        }

        // Process scale min/max

        (*ppadfScaleMin)[iBand] = CPLAtof(aosTokens[1]);
        (*ppadfScaleMax)[iBand] = CPLAtof(aosTokens[2]);

        // process lut

        (*ppapanLUTs)[iBand] =
            static_cast<int *>(CPLCalloc(nLUTBins, sizeof(int)));

        for (int iLUT = 0; iLUT < nLUTBins; iLUT++)
            (*ppapanLUTs)[iBand][iLUT] = atoi(aosTokens[iLUT + 3]);
    }

    return CE_None;
}

/************************************************************************/
/*                      WriteLUTs()                                     */
/*                                                                      */
/*      Write the LUT for each band to a file or stdout.                */
/************************************************************************/

void WriteLUTs(int **papanLUTs, int nBandCount, int nLUTBins,
               double *padfScaleMin, double *padfScaleMax,
               const char *pszConfigFile)
{
    FILE *fpConfig = stdout;
    if (pszConfigFile)
        fpConfig = fopen(pszConfigFile, "w");

    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        fprintf(fpConfig, "%d:Band ", iBand + 1);
        fprintf(fpConfig, "%g:ScaleMin %g:ScaleMax ", padfScaleMin[iBand],
                padfScaleMax[iBand]);

        if (papanLUTs)
        {
            for (int iLUT = 0; iLUT < nLUTBins; iLUT++)
                fprintf(fpConfig, "%d ", papanLUTs[iBand][iLUT]);
        }
        fprintf(fpConfig, "\n");
    }

    if (pszConfigFile)
        fclose(fpConfig);
}

/************************************************************************/
/*                      WriteEnhanced()                                 */
/*                                                                      */
/*      Write an enhanced image using the provided LUTs.                */
/************************************************************************/

CPLErr WriteEnhanced(GDALDatasetH hDataset, int **papanLUTs, int nLUTBins,
                     double *padfScaleMin, double *padfScaleMax,
                     GDALDataType eOutputType, GDALDriverH hDriver,
                     const char *pszDest, char **papszCreateOptions,
                     GDALProgressFunc pfnProgress)
{
    int nBandCount = GDALGetRasterCount(hDataset);

    EnhanceCBInfo *pasEInfo = static_cast<EnhanceCBInfo *>(
        CPLCalloc(nBandCount, sizeof(EnhanceCBInfo)));

    /* -------------------------------------------------------------------- */
    /*      Make a virtual clone.                                           */
    /* -pixe------------------------------------------------------------------- */
    VRTDataset *poVDS = new VRTDataset(GDALGetRasterXSize(hDataset),
                                       GDALGetRasterYSize(hDataset));

    if (GDALGetGCPCount(hDataset) == 0)
    {
        double adfGeoTransform[6];

        const char *pszProjection = GDALGetProjectionRef(hDataset);
        if (pszProjection != nullptr && strlen(pszProjection) > 0)
            poVDS->SetProjection(pszProjection);

        if (GDALGetGeoTransform(hDataset, adfGeoTransform) == CE_None)
            poVDS->SetGeoTransform(adfGeoTransform);
    }
    else
    {
        poVDS->SetGCPs(GDALGetGCPCount(hDataset), GDALGetGCPs(hDataset),
                       GDALGetGCPProjection(hDataset));
    }

    poVDS->SetMetadata(GDALDataset::FromHandle(hDataset)->GetMetadata());

    for (int iBand = 0; iBand < nBandCount; iBand++)
    {
        VRTSourcedRasterBand *poVRTBand;
        GDALRasterBand *poSrcBand;
        GDALDataType eBandType;

        poSrcBand = GDALDataset::FromHandle(hDataset)->GetRasterBand(iBand + 1);

        /* ---------------------------------------------------------------- */
        /*      Select output data type to match source.                    */
        /* ---------------------------------------------------------------- */
        if (eOutputType == GDT_Unknown)
            eBandType = GDT_Byte;
        else
            eBandType = eOutputType;

        /* ---------------------------------------------------------------- */
        /*      Create this band.                                           */
        /* ---------------------------------------------------------------- */
        poVDS->AddBand(eBandType, nullptr);
        poVRTBand = cpl::down_cast<VRTSourcedRasterBand *>(
            poVDS->GetRasterBand(iBand + 1));

        /* ---------------------------------------------------------------- */
        /*     Create a function based source with info on how to apply the */
        /*     enhancement.                                                 */
        /* ---------------------------------------------------------------- */
        pasEInfo[iBand].poSrcBand = poSrcBand;
        pasEInfo[iBand].eWrkType = eBandType;
        pasEInfo[iBand].dfScaleMin = padfScaleMin[iBand];
        pasEInfo[iBand].dfScaleMax = padfScaleMax[iBand];
        pasEInfo[iBand].nLUTBins = nLUTBins;

        if (papanLUTs)
            pasEInfo[iBand].panLUT = papanLUTs[iBand];

        poVRTBand->AddFuncSource(EnhancerCallback, pasEInfo + iBand);

        /* ---------------------------------------------------------------- */
        /*      copy over some other information of interest.               */
        /* ---------------------------------------------------------------- */
        poVRTBand->CopyCommonInfoFrom(poSrcBand);
    }

    /* -------------------------------------------------------------------- */
    /*      Write to the output file using CopyCreate().                    */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hOutDS =
        GDALCreateCopy(hDriver, pszDest, static_cast<GDALDatasetH>(poVDS),
                       FALSE, papszCreateOptions, pfnProgress, nullptr);
    CPLErr eErr = CE_None;
    if (hOutDS == nullptr)
    {
        eErr = CE_Failure;
    }
    else
    {
        GDALClose(hOutDS);
    }

    GDALClose(poVDS);
    CPLFree(pasEInfo);

    return eErr;
}
