/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing OGR driver data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_version.h"
#include "gdal_priv.h"
#include "gdal_utils_priv.h"
#include "ogr_p.h"

#include "commonutils.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()
{
    fprintf(stderr, "%s\n", GDALVectorInfoGetParserUsage().c_str());
    exit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    // Check strict compilation and runtime library version as we use C++ API.
    if (!GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    EarlySetConfigOptions(argc, argv);

    OGRRegisterAll();

    argc = OGRGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        exit(-argc);

    auto psOptionsForBinary =
        std::make_unique<GDALVectorInfoOptionsForBinary>();

    GDALVectorInfoOptions *psOptions =
        GDALVectorInfoOptionsNew(argv + 1, psOptionsForBinary.get());
    if (psOptions == nullptr)
        Usage();

/* -------------------------------------------------------------------- */
/*      Open dataset.                                                   */
/* -------------------------------------------------------------------- */
#ifdef __AFL_HAVE_MANUAL_CONTROL
    int iIter = 0;
    while (__AFL_LOOP(1000))
    {
        iIter++;
#endif
        /* --------------------------------------------------------------------
         */
        /*      Open data source. */
        /* --------------------------------------------------------------------
         */
        int nFlags = GDAL_OF_VECTOR;
        bool bMayRetryUpdateMode = false;
        if (psOptionsForBinary->bUpdate)
            nFlags |= GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR;
        else if (psOptionsForBinary->bReadOnly)
            nFlags |= GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR;
        else if (psOptionsForBinary->osSQLStatement.empty())
        {
            nFlags |= GDAL_OF_READONLY;
            // GDALIdentifyDriverEx() might emit an error message, e.g.
            // when opening "/vsizip/foo.zip/" and the zip has more than one
            // file. Cf https://github.com/OSGeo/gdal/issues/9459
            CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
            if (GDALIdentifyDriverEx(
                    psOptionsForBinary->osFilename.c_str(), GDAL_OF_VECTOR,
                    psOptionsForBinary->aosAllowInputDrivers.List(), nullptr))
            {
                bMayRetryUpdateMode = true;
            }
            else
            {
                // And an error Will be emitted
                nFlags |= GDAL_OF_VERBOSE_ERROR;
            }
        }
        else
            nFlags |= GDAL_OF_UPDATE | GDAL_OF_VERBOSE_ERROR;
        GDALDataset *poDS = GDALDataset::Open(
            psOptionsForBinary->osFilename.c_str(), nFlags,
            psOptionsForBinary->aosAllowInputDrivers.List(),
            psOptionsForBinary->aosOpenOptions.List(), nullptr);

        if (poDS == nullptr && !psOptionsForBinary->bReadOnly &&
            !psOptionsForBinary->bUpdate)
        {
            if (psOptionsForBinary->osSQLStatement.empty() &&
                bMayRetryUpdateMode)
            {
                // In some cases (empty geopackage for example), opening in
                // read-only mode fails, so retry in update mode
                poDS = GDALDataset::Open(
                    psOptionsForBinary->osFilename.c_str(),
                    GDAL_OF_UPDATE | GDAL_OF_VECTOR,
                    psOptionsForBinary->aosAllowInputDrivers.List(),
                    psOptionsForBinary->aosOpenOptions.List(), nullptr);
            }
            else if (!psOptionsForBinary->osSQLStatement.empty())
            {
                poDS = GDALDataset::Open(
                    psOptionsForBinary->osFilename.c_str(),
                    GDAL_OF_READONLY | GDAL_OF_VECTOR,
                    psOptionsForBinary->aosAllowInputDrivers.List(),
                    psOptionsForBinary->aosOpenOptions.List(), nullptr);
                if (poDS != nullptr && psOptionsForBinary->bVerbose)
                {
                    printf("Had to open data source read-only.\n");
#ifdef __AFL_HAVE_MANUAL_CONTROL
                    psOptionsForBinary->bReadOnly = true;
#endif
                }
            }
        }

        int nRet = 0;
        if (poDS == nullptr)
        {
            nRet = 1;

            VSIStatBuf sStat;
            CPLString message;
            message.Printf("ogrinfo failed - unable to open '%s'.",
                           psOptionsForBinary->osFilename.c_str());
            if (VSIStat(psOptionsForBinary->osFilename.c_str(), &sStat) == 0)
            {
                GDALDriverH drv =
                    GDALIdentifyDriverEx(psOptionsForBinary->osFilename.c_str(),
                                         GDAL_OF_RASTER, nullptr, nullptr);
                if (drv)
                {
                    message += " Did you intend to call gdalinfo?";
                }
            }
            fprintf(stderr, "%s\n", message.c_str());
        }
        else
        {
            char *pszGDALVectorInfoOutput =
                GDALVectorInfo(GDALDataset::ToHandle(poDS), psOptions);

            if (pszGDALVectorInfoOutput)
                printf("%s", pszGDALVectorInfoOutput);
            else
                nRet = 1;

            CPLFree(pszGDALVectorInfoOutput);
        }

        delete poDS;
#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif

    GDALVectorInfoOptionsFree(psOptions);

    CSLDestroy(argv);

    GDALDumpOpenDatasets(stderr);

    GDALDestroyDriverManager();

    CPLDumpSharedList(nullptr);
    GDALDestroy();

    exit(nRet);
}

MAIN_END
