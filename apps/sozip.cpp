/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to build seek-optimized ZIP files
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_time.h"
#include "cpl_progress.h"
#include "gdal_version.h"
#include "gdal_priv.h"
#include "commonutils.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char *pszErrorMsg = nullptr)

{
    printf("Usage: sozip [--quiet|--verbose]\n"
           "             [[-g|--grow] | [--overwrite]]\n"
           "             [-r|--recurse-paths]\n"
           "             [-j|--junk]\n"
           "             [-l|--list]\n"
           "             [--enable-sozip=auto/yes/no]\n"
           "             [--sozip-chunk-size=value]\n"
           "             [--sozip-min-file-size=value]\n"
           "             zip_filename [filename]*\n");

    if (pszErrorMsg != nullptr)
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(nArgc, papszArgv)
{
    bool bOverwrite = false;
    bool bRecurse = false;
    bool bVerbose = false;
    bool bQuiet = false;
    bool bList = false;
    bool bJunkPaths = false;
    const char *pszZipFilename = nullptr;
    CPLStringList aosFiles;
    CPLStringList aosOptions;

    /* -------------------------------------------------------------------- */
    /*      Parse command line.                                             */
    /* -------------------------------------------------------------------- */
    for (int iArg = 1; iArg < nArgc; iArg++)
    {
        if (strcmp(papszArgv[iArg], "--utility_version") == 0)
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME,
                   GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if (strcmp(papszArgv[iArg], "--help") == 0)
        {
            Usage();
        }
        else if (strcmp(papszArgv[iArg], "--quiet") == 0)
        {
            bQuiet = true;
        }
        else if (strcmp(papszArgv[iArg], "--verbose") == 0)
        {
            bVerbose = true;
        }
        else if (strcmp(papszArgv[iArg], "-r") == 0 ||
                 strcmp(papszArgv[iArg], "--recurse-paths") == 0)
        {
            bRecurse = true;
        }
        else if (strcmp(papszArgv[iArg], "-j") == 0 ||
                 strcmp(papszArgv[iArg], "--junk-paths") == 0)
        {
            bJunkPaths = true;
        }
        else if (strcmp(papszArgv[iArg], "-g") == 0 ||
                 strcmp(papszArgv[iArg], "--grow") == 0)
        {
            // Default mode. Nothing to do
        }
        else if (strcmp(papszArgv[iArg], "--overwrite") == 0)
        {
            bOverwrite = true;
        }
        else if (strcmp(papszArgv[iArg], "-l") == 0 ||
                 strcmp(papszArgv[iArg], "--list") == 0)
        {
            bList = true;
        }
        else if (strcmp(papszArgv[iArg], "--enable-sozip") == 0 &&
                 iArg + 1 < nArgc)
        {
            ++iArg;
            aosOptions.SetNameValue("SEEK_OPTIMIZED", papszArgv[iArg]);
        }
        else if (STARTS_WITH(papszArgv[iArg], "--enable-sozip="))
        {
            aosOptions.SetNameValue(
                "SEEK_OPTIMIZED", papszArgv[iArg] + strlen("--enable-sozip="));
        }
        else if (strcmp(papszArgv[iArg], "--sozip-chunk-size") == 0 &&
                 iArg + 1 < nArgc)
        {
            ++iArg;
            aosOptions.SetNameValue("SOZIP_CHUNK_SIZE", papszArgv[iArg]);
        }
        else if (STARTS_WITH(papszArgv[iArg], "--sozip-chunk-size="))
        {
            aosOptions.SetNameValue("SOZIP_CHUNK_SIZE",
                                    papszArgv[iArg] +
                                        strlen("--sozip-chunk-size="));
        }
        else if (strcmp(papszArgv[iArg], "--sozip-min-file-size") == 0 &&
                 iArg + 1 < nArgc)
        {
            ++iArg;
            aosOptions.SetNameValue("SOZIP_MIN_FILE_SIZE", papszArgv[iArg]);
        }
        else if (STARTS_WITH(papszArgv[iArg], "--sozip-min-file-size="))
        {
            aosOptions.SetNameValue("SOZIP_MIN_FILE_SIZE",
                                    papszArgv[iArg] +
                                        strlen("--sozip-min-file-size="));
        }
        else if (papszArgv[iArg][0] == '-')
        {
            Usage(CPLSPrintf("Unhandled option %s", papszArgv[iArg]));
        }
        else if (pszZipFilename == nullptr)
        {
            pszZipFilename = papszArgv[iArg];
        }
        else
        {
            aosFiles.AddString(papszArgv[iArg]);
        }
    }

    if (pszZipFilename == nullptr)
    {
        Usage("Missing zip filename");
        return 1;
    }

    if (!bList)
    {
        if (aosFiles.empty())
        {
            Usage("Missing source filename(s)");
            return 1;
        }
    }
    else
    {
        if (!aosFiles.empty())
        {
            Usage("Unexpect source filename(s) in --list mode");
            return 1;
        }
    }

    if (!EQUAL(CPLGetExtension(pszZipFilename), "zip"))
    {
        Usage("Extension of zip filename should be .zip");
        return 1;
    }

    if (bList)
    {
        VSIDIR *psDir = VSIOpenDir(
            (std::string("/vsizip/") + pszZipFilename).c_str(), -1, nullptr);
        if (psDir == nullptr)
            return 1;
        printf("  Length          DateTime        Seek-optimized / chunk size  "
               "Name\n");
        /* clang-format off */
        printf("-----------  -------------------  ---------------------------  -----------------\n");
        /* clang-format on */
        while (auto psEntry = VSIGetNextDirEntry(psDir))
        {
            if (!VSI_ISDIR(psEntry->nMode))
            {
                struct tm brokenDown;
                CPLUnixTimeToYMDHMS(psEntry->nMTime, &brokenDown);
                char **papszMD = VSIGetFileMetadata((std::string("/vsizip/{") +
                                                     pszZipFilename + "}/" +
                                                     psEntry->pszName)
                                                        .c_str(),
                                                    "ZIP", nullptr);
                bool bSeekOptimized =
                    CSLFetchNameValue(papszMD, "SEEK_OPTIMIZED_VALID") !=
                    nullptr;
                const char *pszChunkSize =
                    CSLFetchNameValue(papszMD, "SOZIP_CHUNK_SIZE");
                printf("%11" CPL_FRMT_GB_WITHOUT_PREFIX
                       "u  %04d-%02d-%02d %02d:%02d:%02d  %s  %s\n",
                       static_cast<GUIntBig>(psEntry->nSize),
                       brokenDown.tm_year + 1900, brokenDown.tm_mon + 1,
                       brokenDown.tm_mday, brokenDown.tm_hour,
                       brokenDown.tm_min, brokenDown.tm_sec,
                       bSeekOptimized
                           ? CPLSPrintf("   yes (%9s bytes)   ", pszChunkSize)
                           : "                           ",
                       psEntry->pszName);
                CSLDestroy(papszMD);
            }
        }
        VSICloseDir(psDir);
        return 0;
    }

    VSIStatBufL sBuf;
    CPLStringList aosOptionsCreateZip;
    if (bOverwrite)
    {
        VSIUnlink(pszZipFilename);
    }
    else
    {
        if (VSIStatExL(pszZipFilename, &sBuf, VSI_STAT_EXISTS_FLAG) == 0)
            aosOptionsCreateZip.SetNameValue("APPEND", "TRUE");
    }

    uint64_t nTotalSize = 0;
    std::vector<uint64_t> anFileSizes;

    if (bRecurse)
    {
        CPLStringList aosNewFiles;
        for (int i = 0; i < aosFiles.size(); ++i)
        {
            if (VSIStatL(aosFiles[i], &sBuf) == 0 && VSI_ISDIR(sBuf.st_mode))
            {
                VSIDIR *psDir = VSIOpenDir(aosFiles[i], -1, nullptr);
                if (psDir == nullptr)
                    return 1;
                while (auto psEntry = VSIGetNextDirEntry(psDir))
                {
                    if (!VSI_ISDIR(psEntry->nMode))
                    {
                        std::string osName(aosFiles[i]);
                        if (osName.back() != '/')
                            osName += '/';
                        osName += psEntry->pszName;
                        aosNewFiles.AddString(osName.c_str());
                        if (aosNewFiles.size() > 10 * 1000 * 1000)
                        {
                            CPLError(CE_Failure, CPLE_NotSupported,
                                     "Too many source files");
                            VSICloseDir(psDir);
                            return 1;
                        }
                    }
                }
                VSICloseDir(psDir);
            }
        }
        aosFiles = std::move(aosNewFiles);
    }

    if (!bVerbose && !bQuiet)
    {
        anFileSizes.resize(aosFiles.size());
        for (int i = 0; i < aosFiles.size(); ++i)
        {
            if (VSIStatL(aosFiles[i], &sBuf) == 0)
            {
                anFileSizes[i] = sBuf.st_size;
                nTotalSize += sBuf.st_size;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s\n",
                         aosFiles[i]);
                return 1;
            }
        }
    }

    void *hZIP = CPLCreateZip(pszZipFilename, aosOptionsCreateZip.List());

    if (hZIP == nullptr)
        return 1;

    uint64_t nCurSize = 0;
    for (int i = 0; i < aosFiles.size(); ++i)
    {
        if (bVerbose)
            printf("Adding %s... (%d/%d)\n", aosFiles[i], i + 1,
                   aosFiles.size());
        void *pScaledProgress = nullptr;
        if (!bVerbose && !bQuiet && nTotalSize != 0)
        {
            pScaledProgress = GDALCreateScaledProgress(
                double(nCurSize) / nTotalSize,
                double(nCurSize + anFileSizes[i]) / nTotalSize,
                GDALTermProgress, nullptr);
        }
        else if (!bQuiet)
        {
            GDALTermProgress(0, nullptr, nullptr);
        }
        if (VSIStatL(aosFiles[i], &sBuf) != 0 || VSI_ISDIR(sBuf.st_mode))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s is not a regular file",
                     aosFiles[i]);
            CPLCloseZip(hZIP);
            return 1;
        }

        std::string osArchiveFilename(aosFiles[i]);
        if (bJunkPaths)
        {
            osArchiveFilename = CPLGetFilename(aosFiles[i]);
        }
        else if (osArchiveFilename[0] == '/')
        {
            osArchiveFilename = osArchiveFilename.substr(1);
        }
        else if (osArchiveFilename.size() > 3 && osArchiveFilename[1] == ':' &&
                 (osArchiveFilename[2] == '/' || osArchiveFilename[2] == '\\'))
        {
            osArchiveFilename = osArchiveFilename.substr(3);
        }

        CPLErr eErr =
            CPLAddFileInZip(hZIP, osArchiveFilename.c_str(), aosFiles[i],
                            nullptr, aosOptions.List(),
                            pScaledProgress ? GDALScaledProgress
                            : bQuiet        ? nullptr
                                            : GDALTermProgress,
                            pScaledProgress ? pScaledProgress : nullptr);
        if (pScaledProgress)
        {
            GDALDestroyScaledProgress(pScaledProgress);
            nCurSize += anFileSizes[i];
        }
        if (eErr != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Failed adding %s",
                     aosFiles[i]);
            CPLCloseZip(hZIP);
            return 1;
        }
    }
    CPLCloseZip(hZIP);
    return 0;
}
MAIN_END
