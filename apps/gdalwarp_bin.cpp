/******************************************************************************
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Test program for high performance warper API.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging
 *                          Fort Collin, CO
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_error_internal.h"
#include "gdal_version.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

#include <vector>

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
    fprintf(stderr, "%s\n", GDALWarpAppGetParserUsage().c_str());
    GDALExit(1);
}

/************************************************************************/
/*                          WarpTermProgress()                          */
/************************************************************************/

static int gnSrcCount = 0;

static int CPL_STDCALL WarpTermProgress(double dfProgress,
                                        const char *pszMessage, void *)
{
    static CPLString osLastMsg;
    static int iSrc = -1;
    if (pszMessage == nullptr)
    {
        iSrc = 0;
    }
    else if (pszMessage != osLastMsg)
    {
        if (!osLastMsg.empty())
            GDALTermProgress(1.0, nullptr, nullptr);
        printf("%s : ", pszMessage);
        osLastMsg = pszMessage;
        iSrc++;
    }
    return GDALTermProgress(dfProgress * gnSrcCount - iSrc, nullptr, nullptr);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    EarlySetConfigOptions(argc, argv);
    CPLDebugOnly("GDAL", "Start");

    /* -------------------------------------------------------------------- */
    /*      Register standard GDAL drivers, and process generic GDAL        */
    /*      command options.                                                */
    /* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);
    if (argc < 1)
        GDALExit(-argc);

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

    GDALWarpAppOptionsForBinary sOptionsForBinary;
    /* coverity[tainted_data] */
    GDALWarpAppOptions *psOptions =
        GDALWarpAppOptionsNew(argv + 1, &sOptionsForBinary);
    CSLDestroy(argv);

    if (psOptions == nullptr)
    {
        Usage();
    }

    if (sOptionsForBinary.aosSrcFiles.size() == 1 &&
        sOptionsForBinary.aosSrcFiles[0] == sOptionsForBinary.osDstFilename &&
        sOptionsForBinary.bOverwrite)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Source and destination datasets must be different.\n");
        GDALExit(1);
    }

    /* -------------------------------------------------------------------- */
    /*      Open source files.                                              */
    /* -------------------------------------------------------------------- */
    GDALDatasetH *pahSrcDS = nullptr;
    int nSrcCount = 0;
    for (int i = 0; i < sOptionsForBinary.aosSrcFiles.size(); ++i)
    {
        nSrcCount++;
        pahSrcDS = static_cast<GDALDatasetH *>(
            CPLRealloc(pahSrcDS, sizeof(GDALDatasetH) * nSrcCount));
        pahSrcDS[i] =
            GDALOpenEx(sOptionsForBinary.aosSrcFiles[i],
                       GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                       sOptionsForBinary.aosAllowedInputDrivers.List(),
                       sOptionsForBinary.aosOpenOptions.List(), nullptr);

        if (pahSrcDS[i] == nullptr)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to open source file %s\n",
                     sOptionsForBinary.aosSrcFiles[i]);
            while (nSrcCount--)
            {
                GDALClose(pahSrcDS[nSrcCount]);
                pahSrcDS[nSrcCount] = nullptr;
            }
            CPLFree(pahSrcDS);
            GDALWarpAppOptionsFree(psOptions);
            GDALExit(2);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Does the output dataset already exist?                          */
    /* -------------------------------------------------------------------- */

    /* FIXME ? source filename=target filename and -overwrite is definitely */
    /* an error. But I can't imagine of a valid case (without -overwrite), */
    /* where it would make sense. In doubt, let's keep that dubious
     * possibility... */

    bool bOutStreaming = false;
    if (sOptionsForBinary.osDstFilename == "/vsistdout/")
    {
        sOptionsForBinary.bQuiet = true;
        bOutStreaming = true;
    }
#ifdef S_ISFIFO
    else
    {
        VSIStatBufL sStat;
        if (VSIStatExL(sOptionsForBinary.osDstFilename.c_str(), &sStat,
                       VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0 &&
            S_ISFIFO(sStat.st_mode))
        {
            bOutStreaming = true;
        }
    }
#endif

    GDALDatasetH hDstDS = nullptr;
    if (bOutStreaming)
    {
        GDALWarpAppOptionsSetWarpOption(psOptions, "STREAMABLE_OUTPUT", "YES");
    }
    else
    {
        std::vector<CPLErrorHandlerAccumulatorStruct> aoErrors;
        CPLInstallErrorHandlerAccumulator(aoErrors);
        hDstDS = GDALOpenEx(
            sOptionsForBinary.osDstFilename.c_str(),
            GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR | GDAL_OF_UPDATE, nullptr,
            sOptionsForBinary.aosDestOpenOptions.List(), nullptr);
        CPLUninstallErrorHandlerAccumulator();
        if (hDstDS != nullptr)
        {
            for (size_t i = 0; i < aoErrors.size(); i++)
            {
                CPLError(aoErrors[i].type, aoErrors[i].no, "%s",
                         aoErrors[i].msg.c_str());
            }
        }
    }

    if (hDstDS != nullptr && sOptionsForBinary.bOverwrite)
    {
        GDALClose(hDstDS);
        hDstDS = nullptr;
    }

    bool bCheckExistingDstFile =
        !bOutStreaming && hDstDS == nullptr && !sOptionsForBinary.bOverwrite;

    if (hDstDS != nullptr && sOptionsForBinary.bCreateOutput)
    {
        if (sOptionsForBinary.aosCreateOptions.FetchBool("APPEND_SUBDATASET",
                                                         false))
        {
            GDALClose(hDstDS);
            hDstDS = nullptr;
            bCheckExistingDstFile = false;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Output dataset %s exists,\n"
                     "but some command line options were provided indicating a "
                     "new dataset\n"
                     "should be created.  Please delete existing dataset and "
                     "run again.\n",
                     sOptionsForBinary.osDstFilename.c_str());
            GDALExit(1);
        }
    }

    /* Avoid overwriting an existing destination file that cannot be opened in
     */
    /* update mode with a new GTiff file */
    if (bCheckExistingDstFile)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        hDstDS = GDALOpen(sOptionsForBinary.osDstFilename.c_str(), GA_ReadOnly);
        CPLPopErrorHandler();

        if (hDstDS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Output dataset %s exists, but cannot be opened in update "
                     "mode\n",
                     sOptionsForBinary.osDstFilename.c_str());
            GDALClose(hDstDS);
            GDALExit(1);
        }
    }

    if (!(sOptionsForBinary.bQuiet))
    {
        gnSrcCount = nSrcCount;
        GDALWarpAppOptionsSetProgress(psOptions, WarpTermProgress, nullptr);
        GDALWarpAppOptionsSetQuiet(psOptions, false);
    }

    int bUsageError = FALSE;
    GDALDatasetH hOutDS =
        GDALWarp(sOptionsForBinary.osDstFilename.c_str(), hDstDS, nSrcCount,
                 pahSrcDS, psOptions, &bUsageError);
    if (bUsageError)
        Usage();
    int nRetCode = (hOutDS) ? 0 : 1;

    GDALWarpAppOptionsFree(psOptions);

    // Close first hOutDS since it might reference sources (case of VRT)
    if (GDALClose(hOutDS ? hOutDS : hDstDS) != CE_None)
        nRetCode = 1;

    for (int i = 0; i < nSrcCount; i++)
    {
        GDALClose(pahSrcDS[i]);
    }
    CPLFree(pahSrcDS);

    GDALDumpOpenDatasets(stderr);

    OGRCleanupAll();

    return nRetCode;
}

MAIN_END
