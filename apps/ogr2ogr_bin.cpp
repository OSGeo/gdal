/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for translating between formats.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2015, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <memory>
#include <vector>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdal_utils_priv.h"
#include "gdal_version.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    fprintf(stderr, "%s\n", GDALVectorTranslateGetParserUsage().c_str());
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(nArgc, papszArgv)
{
    // Check strict compilation and runtime library version as we use C++ API.
    if (!GDAL_CHECK_VERSION(papszArgv[0]))
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

    /* -------------------------------------------------------------------- */
    /*      Register format(s).                                             */
    /* -------------------------------------------------------------------- */
    OGRRegisterAll();

    /* -------------------------------------------------------------------- */
    /*      Processing command line arguments.                              */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDS = nullptr;
    GDALDatasetH hODS = nullptr;
    bool bCloseODS = true;
    GDALDatasetH hDstDS = nullptr;
    int nRetCode = 1;
    GDALVectorTranslateOptions *psOptions = nullptr;
    GDALVectorTranslateOptionsForBinary sOptionsForBinary;

    nArgc = OGRGeneralCmdLineProcessor(nArgc, &papszArgv, 0);

    if (nArgc < 1)
    {
        papszArgv = nullptr;
        nRetCode = -nArgc;
        goto exit;
    }

    psOptions =
        GDALVectorTranslateOptionsNew(papszArgv + 1, &sOptionsForBinary);

    if (psOptions == nullptr)
    {
        Usage();
        goto exit;
    }

    if (sOptionsForBinary.osDestDataSource == "/vsistdout/")
        sOptionsForBinary.bQuiet = true;

    /* -------------------------------------------------------------------- */
    /*      Open data source.                                               */
    /* -------------------------------------------------------------------- */

    // Avoid opening twice the same datasource if it is both the input and
    // output Known to cause problems with at least FGdb, SQlite and GPKG
    // drivers. See #4270
    if (sOptionsForBinary.eAccessMode != ACCESS_CREATION &&
        sOptionsForBinary.osDestDataSource == sOptionsForBinary.osDataSource)
    {
        hODS = GDALOpenEx(sOptionsForBinary.osDataSource.c_str(),
                          GDAL_OF_UPDATE | GDAL_OF_VECTOR,
                          sOptionsForBinary.aosAllowInputDrivers.List(),
                          sOptionsForBinary.aosOpenOptions.List(), nullptr);

        GDALDriverH hDriver =
            hODS != nullptr ? GDALGetDatasetDriver(hODS) : nullptr;

        // Restrict to those 3 drivers. For example it is known to break with
        // the PG driver due to the way it manages transactions.
        if (hDriver && !(EQUAL(GDALGetDescription(hDriver), "FileGDB") ||
                         EQUAL(GDALGetDescription(hDriver), "SQLite") ||
                         EQUAL(GDALGetDescription(hDriver), "GPKG")))
        {
            hDS = GDALOpenEx(sOptionsForBinary.osDataSource.c_str(),
                             GDAL_OF_VECTOR,
                             sOptionsForBinary.aosAllowInputDrivers.List(),
                             sOptionsForBinary.aosOpenOptions.List(), nullptr);
        }
        else
        {
            hDS = hODS;
            bCloseODS = false;
        }
    }
    else
    {
        hDS = GDALOpenEx(sOptionsForBinary.osDataSource.c_str(), GDAL_OF_VECTOR,
                         sOptionsForBinary.aosAllowInputDrivers.List(),
                         sOptionsForBinary.aosOpenOptions.List(), nullptr);
    }

    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
    if (hDS == nullptr)
    {
        GDALDriverManager *poDM = GetGDALDriverManager();

        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to open datasource `%s' with the following drivers.",
                 sOptionsForBinary.osDataSource.c_str());

        for (int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++)
        {
            GDALDriver *poIter = poDM->GetDriver(iDriver);
            char **papszDriverMD = poIter->GetMetadata();
            if (CPLTestBool(CSLFetchNameValueDef(papszDriverMD,
                                                 GDAL_DCAP_VECTOR, "FALSE")))
            {
                fprintf(stderr, "  -> `%s'\n", poIter->GetDescription());
            }
        }

        GDALVectorTranslateOptionsFree(psOptions);
        goto exit;
    }

    if (hODS != nullptr && !sOptionsForBinary.osFormat.empty())
    {
        GDALDriverManager *poDM = GetGDALDriverManager();

        GDALDriver *poDriver =
            poDM->GetDriverByName(sOptionsForBinary.osFormat.c_str());
        if (poDriver == nullptr)
        {
            fprintf(stderr, "Unable to find driver `%s'.\n",
                    sOptionsForBinary.osFormat.c_str());
            fprintf(stderr, "The following drivers are available:\n");

            for (int iDriver = 0; iDriver < poDM->GetDriverCount(); iDriver++)
            {
                GDALDriver *poIter = poDM->GetDriver(iDriver);
                char **papszDriverMD = poIter->GetMetadata();
                if (CPLTestBool(CSLFetchNameValueDef(
                        papszDriverMD, GDAL_DCAP_VECTOR, "FALSE")) &&
                    (CPLTestBool(CSLFetchNameValueDef(
                         papszDriverMD, GDAL_DCAP_CREATE, "FALSE")) ||
                     CPLTestBool(CSLFetchNameValueDef(
                         papszDriverMD, GDAL_DCAP_CREATECOPY, "FALSE"))))
                {
                    fprintf(stderr, "  -> `%s'\n", poIter->GetDescription());
                }
            }
            GDALVectorTranslateOptionsFree(psOptions);
            goto exit;
        }
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        GDALVectorTranslateOptionsSetProgress(psOptions, GDALTermProgress,
                                              nullptr);
    }

    {
        // TODO(schwehr): Remove scope after removing gotos
        int bUsageError = FALSE;
        hDstDS = GDALVectorTranslate(sOptionsForBinary.osDestDataSource.c_str(),
                                     hODS, 1, &hDS, psOptions, &bUsageError);
        if (bUsageError)
            Usage();
        else
            nRetCode = hDstDS ? 0 : 1;
    }

    GDALVectorTranslateOptionsFree(psOptions);

    if (hDS)
        GDALClose(hDS);
    if (bCloseODS)
    {
        if (nRetCode == 0)
            CPLErrorReset();
        if (GDALClose(hDstDS) != CE_None)
            nRetCode = 1;
        // TODO: Below code can be removed once all drivers have implemented
        // GDALDataset::Close()
        if (nRetCode == 0 && CPLGetLastErrorType() == CE_Failure)
            nRetCode = 1;
    }

exit:
    CSLDestroy(papszArgv);
    GDALDestroy();

    return nRetCode;
}

MAIN_END
