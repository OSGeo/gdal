/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Image Translator Program
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_version.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

/*  ******************************************************************* */
/*                               Usage()                                */
/* ******************************************************************** */

static void Usage()
{
    fprintf(stderr, "%s\n", GDALTranslateGetParserUsage().c_str());

    exit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    /* Check strict compilation and runtime library version as we use C++ API */
    if (!GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    EarlySetConfigOptions(argc, argv);

    /* -------------------------------------------------------------------- */
    /*      Register standard GDAL drivers, and process generic GDAL        */
    /*      command options.                                                */
    /* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    /* -------------------------------------------------------------------- */
    /*      Set optimal setting for best performance with huge input VRT.   */
    /*      The rationale for 450 is that typical Linux process allow       */
    /*      only 1024 file descriptors per process and we need to keep some */
    /*      spare for shared libraries, etc. so let's go down to 900.       */
    /*      And some datasets may need 2 file descriptors, so divide by 2   */
    /*      for security.                                                   */
    /* -------------------------------------------------------------------- */
    if (CPLGetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", nullptr) == nullptr)
    {
#if defined(__MACH__) && defined(__APPLE__)
        // On Mach, the default limit is 256 files per process
        // TODO We should eventually dynamically query the limit for all OS
        CPLSetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "100");
#else
        CPLSetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "450");
#endif
    }

    GDALTranslateOptionsForBinary sOptionsForBinary;
    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(argv + 1, &sOptionsForBinary);
    CSLDestroy(argv);

    if (psOptions == nullptr)
    {
        Usage();
    }

    if (sOptionsForBinary.osDest == "/vsistdout/")
    {
        sOptionsForBinary.bQuiet = true;
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALTranslateOptionsSetProgress(psOptions, GDALTermProgress, nullptr);
    }

    if (!sOptionsForBinary.osFormat.empty())
    {
        GDALDriverH hDriver =
            GDALGetDriverByName(sOptionsForBinary.osFormat.c_str());
        if (hDriver == nullptr)
        {
            fprintf(stderr, "Output driver `%s' not recognised.\n",
                    sOptionsForBinary.osFormat.c_str());
            fprintf(stderr, "The following format drivers are configured and "
                            "support output:\n");
            for (int iDr = 0; iDr < GDALGetDriverCount(); iDr++)
            {
                hDriver = GDALGetDriver(iDr);

                if (GDALGetMetadataItem(hDriver, GDAL_DCAP_RASTER, nullptr) !=
                        nullptr &&
                    (GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) !=
                         nullptr ||
                     GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY,
                                         nullptr) != nullptr))
                {
                    fprintf(stderr, "  %s: %s\n",
                            GDALGetDriverShortName(hDriver),
                            GDALGetDriverLongName(hDriver));
                }
            }

            GDALTranslateOptionsFree(psOptions);

            GDALDestroyDriverManager();
            exit(1);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Attempt to open source file.                                    */
    /* -------------------------------------------------------------------- */

    GDALDatasetH hDataset =
        GDALOpenEx(sOptionsForBinary.osSource.c_str(),
                   GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                   sOptionsForBinary.aosAllowedInputDrivers.List(),
                   sOptionsForBinary.aosOpenOptions.List(), nullptr);

    if (hDataset == nullptr)
    {
        GDALDestroyDriverManager();
        exit(1);
    }

    /* -------------------------------------------------------------------- */
    /*      Handle subdatasets.                                             */
    /* -------------------------------------------------------------------- */
    if (!sOptionsForBinary.bCopySubDatasets &&
        GDALGetRasterCount(hDataset) == 0 &&
        CSLCount(GDALGetMetadata(hDataset, "SUBDATASETS")) > 0)
    {
        fprintf(stderr, "Input file contains subdatasets. Please, select one "
                        "of them for reading.\n");
        GDALClose(hDataset);
        GDALDestroyDriverManager();
        exit(1);
    }

    int bUsageError = FALSE;
    GDALDatasetH hOutDS = nullptr;
    GDALDriverH hOutDriver = nullptr;

    if (sOptionsForBinary.osFormat.empty())
    {
        hOutDriver = GDALGetDriverByName(
            GetOutputDriverForRaster(sOptionsForBinary.osDest.c_str()));
    }
    else
    {
        hOutDriver = GDALGetDriverByName(sOptionsForBinary.osFormat.c_str());
    }

    if (hOutDriver == nullptr)
    {
        fprintf(stderr, "Output driver not found.\n");
        GDALClose(hDataset);
        GDALDestroyDriverManager();
        exit(1);
    }

    bool bCopyCreateSubDatasets =
        (GDALGetMetadataItem(hOutDriver, GDAL_DCAP_SUBCREATECOPY, nullptr) !=
         nullptr);

    if (sOptionsForBinary.bCopySubDatasets &&
        CSLCount(GDALGetMetadata(hDataset, "SUBDATASETS")) > 0)
    {
        if (bCopyCreateSubDatasets)
        {
            // GDAL sets the size of the dataset with subdatasets to 512x512
            // this removes the srcwin function from this operation
            hOutDS = GDALTranslate(sOptionsForBinary.osDest.c_str(), hDataset,
                                   psOptions, &bUsageError);
            GDALClose(hOutDS);
        }
        else
        {
            char **papszSubdatasets = GDALGetMetadata(hDataset, "SUBDATASETS");
            char *pszSubDest = static_cast<char *>(
                CPLMalloc(strlen(sOptionsForBinary.osDest.c_str()) + 32));

            CPLString osPath = CPLGetPath(sOptionsForBinary.osDest.c_str());
            CPLString osBasename =
                CPLGetBasename(sOptionsForBinary.osDest.c_str());
            CPLString osExtension =
                CPLGetExtension(sOptionsForBinary.osDest.c_str());
            CPLString osTemp;

            const char *pszFormat = nullptr;
            if (CSLCount(papszSubdatasets) / 2 < 10)
            {
                pszFormat = "%s_%d";
            }
            else if (CSLCount(papszSubdatasets) / 2 < 100)
            {
                pszFormat = "%s_%002d";
            }
            else
            {
                pszFormat = "%s_%003d";
            }

            const char *pszDest = pszSubDest;

            for (int i = 0; papszSubdatasets[i] != nullptr; i += 2)
            {
                char *pszSource =
                    CPLStrdup(strstr(papszSubdatasets[i], "=") + 1);
                osTemp = CPLSPrintf(pszFormat, osBasename.c_str(), i / 2 + 1);
                osTemp = CPLFormFilename(osPath, osTemp, osExtension);
                strcpy(pszSubDest, osTemp.c_str());
                hDataset = GDALOpenEx(pszSource, GDAL_OF_RASTER, nullptr,
                                      sOptionsForBinary.aosOpenOptions.List(),
                                      nullptr);
                CPLFree(pszSource);
                if (!sOptionsForBinary.bQuiet)
                    printf("Input file size is %d, %d\n",
                           GDALGetRasterXSize(hDataset),
                           GDALGetRasterYSize(hDataset));
                hOutDS =
                    GDALTranslate(pszDest, hDataset, psOptions, &bUsageError);
                if (hOutDS == nullptr)
                    break;
                GDALClose(hOutDS);
            }

            CPLFree(pszSubDest);
        }

        if (bUsageError == TRUE)
            Usage();
        GDALClose(hDataset);
        GDALTranslateOptionsFree(psOptions);

        GDALDestroy();
        return 0;
    }

    if (!sOptionsForBinary.bQuiet)
        printf("Input file size is %d, %d\n", GDALGetRasterXSize(hDataset),
               GDALGetRasterYSize(hDataset));

    hOutDS = GDALTranslate(sOptionsForBinary.osDest.c_str(), hDataset,
                           psOptions, &bUsageError);
    if (bUsageError == TRUE)
        Usage();
    int nRetCode = hOutDS ? 0 : 1;

    /* Close hOutDS before hDataset for the -f VRT case */
    if (GDALClose(hOutDS) != CE_None)
    {
        nRetCode = 1;
        if (CPLGetLastErrorType() == CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown error occurred in GDALClose()");
        }
    }
    GDALClose(hDataset);
    GDALTranslateOptionsFree(psOptions);

    GDALDestroy();

    return nRetCode;
}

MAIN_END
