/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to list info about a file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2015, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_version.h"
#include "gdal.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

/************************************************************************/
/*                               GDALExit()                             */
/*  This function exits and cleans up GDAL and OGR resources            */
/*  Perhaps it should be added to C api and used in all apps?           */
/************************************************************************/

static int GDALExit(int nCode)
{
    const char *pszDebug = CPLGetConfigOption("CPL_DEBUG", nullptr);
    if (pszDebug && (EQUAL(pszDebug, "ON") || EQUAL(pszDebug, "")))
    {
        GDALDumpOpenDatasets(stderr);
        CPLDumpSharedList(nullptr);
    }

    GDALDestroyDriverManager();

    OGRCleanupAll();

    exit(nCode);
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    fprintf(stderr, "%s\n", GDALInfoAppGetParserUsage().c_str());
    GDALExit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    EarlySetConfigOptions(argc, argv);

    /* -------------------------------------------------------------------- */
    /*      Register standard GDAL drivers, and process generic GDAL        */
    /*      command options.                                                */
    /* -------------------------------------------------------------------- */

    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        GDALExit(-argc);

    /* -------------------------------------------------------------------- */
    /*      Parse command line                                              */
    /* -------------------------------------------------------------------- */

    GDALInfoOptionsForBinary sOptionsForBinary;

    std::unique_ptr<GDALInfoOptions, decltype(&GDALInfoOptionsFree)> psOptions{
        GDALInfoOptionsNew(argv + 1, &sOptionsForBinary), GDALInfoOptionsFree};
    CSLDestroy(argv);

    if (!psOptions)
    {
        Usage();
    }

/* -------------------------------------------------------------------- */
/*      Open dataset.                                                   */
/* -------------------------------------------------------------------- */
#ifdef __AFL_HAVE_MANUAL_CONTROL
    int iIter = 0;
    while (__AFL_LOOP(1000))
    {
        iIter++;
#endif

        GDALDatasetH hDataset = GDALOpenEx(
            sOptionsForBinary.osFilename.c_str(),
            GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
            sOptionsForBinary.aosAllowedInputDrivers,
            sOptionsForBinary.aosOpenOptions, nullptr);

        if (hDataset == nullptr)
        {
#ifdef __AFL_HAVE_MANUAL_CONTROL
            continue;
#else
        VSIStatBuf sStat;
        CPLString message;
        message.Printf("gdalinfo failed - unable to open '%s'.",
                       sOptionsForBinary.osFilename.c_str());
        if (VSIStat(sOptionsForBinary.osFilename.c_str(), &sStat) == 0)
        {
            GDALDriverH drv =
                GDALIdentifyDriverEx(sOptionsForBinary.osFilename.c_str(),
                                     GDAL_OF_VECTOR, nullptr, nullptr);
            if (drv)
            {
                message += " Did you intend to call ogrinfo?";
            }
        }
        fprintf(stderr, "%s\n", message.c_str());

        /* --------------------------------------------------------------------
         */
        /*      If argument is a VSIFILE, then print its contents */
        /* --------------------------------------------------------------------
         */
        if (STARTS_WITH(sOptionsForBinary.osFilename.c_str(), "/vsizip/") ||
            STARTS_WITH(sOptionsForBinary.osFilename.c_str(), "/vsitar/"))
        {
            const char *const apszOptions[] = {"NAME_AND_TYPE_ONLY=YES",
                                               nullptr};
            VSIDIR *psDir = VSIOpenDir(sOptionsForBinary.osFilename.c_str(), -1,
                                       apszOptions);
            if (psDir)
            {
                fprintf(stdout,
                        "Unable to open source `%s' directly.\n"
                        "The archive contains several files:\n",
                        sOptionsForBinary.osFilename.c_str());
                int nCount = 0;
                while (auto psEntry = VSIGetNextDirEntry(psDir))
                {
                    if (VSI_ISDIR(psEntry->nMode) && psEntry->pszName[0] &&
                        psEntry->pszName[strlen(psEntry->pszName) - 1] != '/')
                    {
                        fprintf(stdout, "       %s/%s/\n",
                                sOptionsForBinary.osFilename.c_str(),
                                psEntry->pszName);
                    }
                    else
                    {
                        fprintf(stdout, "       %s/%s\n",
                                sOptionsForBinary.osFilename.c_str(),
                                psEntry->pszName);
                    }
                    nCount++;
                    if (nCount == 100)
                    {
                        fprintf(stdout, "[...trimmed...]\n");
                        break;
                    }
                }
                VSICloseDir(psDir);
            }
        }

        GDALDumpOpenDatasets(stderr);

        GDALDestroyDriverManager();

        CPLDumpSharedList(nullptr);

        exit(1);
#endif
        }

        /* --------------------------------------------------------------------
         */
        /*      Read specified subdataset if requested. */
        /* --------------------------------------------------------------------
         */
        if (sOptionsForBinary.nSubdataset > 0)
        {
            char **papszSubdatasets = GDALGetMetadata(hDataset, "SUBDATASETS");
            int nSubdatasets = CSLCount(papszSubdatasets);

            if (nSubdatasets > 0 &&
                sOptionsForBinary.nSubdataset <= nSubdatasets)
            {
                char szKeyName[1024];
                char *pszSubdatasetName;

                snprintf(szKeyName, sizeof(szKeyName), "SUBDATASET_%d_NAME",
                         sOptionsForBinary.nSubdataset);
                szKeyName[sizeof(szKeyName) - 1] = '\0';
                pszSubdatasetName =
                    CPLStrdup(CSLFetchNameValue(papszSubdatasets, szKeyName));
                GDALClose(hDataset);
                hDataset = GDALOpen(pszSubdatasetName, GA_ReadOnly);
                CPLFree(pszSubdatasetName);
            }
            else
            {
                fprintf(stderr,
                        "gdalinfo warning: subdataset %d of %d requested. "
                        "Reading the main dataset.\n",
                        sOptionsForBinary.nSubdataset, nSubdatasets);
            }
        }

        char *pszGDALInfoOutput = GDALInfo(hDataset, psOptions.get());

        if (pszGDALInfoOutput)
            printf("%s", pszGDALInfoOutput);

        CPLFree(pszGDALInfoOutput);

        GDALClose(hDataset);
#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif

    GDALDumpOpenDatasets(stderr);

    GDALDestroyDriverManager();

    CPLDumpSharedList(nullptr);

    GDALDestroy();

    exit(0);
}

MAIN_END
