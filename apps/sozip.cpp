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

#include <limits>

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
           "             [--validate]\n"
           "             [--optimize-from=input.zip]\n"
           "             [--enable-sozip=auto/yes/no]\n"
           "             [--sozip-chunk-size=value]\n"
           "             [--sozip-min-file-size=value]\n"
           "             [--content-type=value]\n"
           "             zip_filename [filename]*\n");

    if (pszErrorMsg)
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit(1);
}

/************************************************************************/
/*                          Validate()                                  */
/************************************************************************/

static int Validate(const char *pszZipFilename, bool bVerbose)
{
    VSIDIR *psDir = VSIOpenDir(
        (std::string("/vsizip/") + pszZipFilename).c_str(), -1, nullptr);
    if (!psDir)
    {
        fprintf(stderr, "%s is not a valid .zip file\n", pszZipFilename);
        return 1;
    }

    int nCountInvalidSOZIP = 0;
    int nCountValidSOZIP = 0;
    int ret = 0;
    while (auto psEntry = VSIGetNextDirEntry(psDir))
    {
        if (!VSI_ISDIR(psEntry->nMode))
        {
            const std::string osFilenameInZip = std::string("/vsizip/{") +
                                                pszZipFilename + "}/" +
                                                psEntry->pszName;
            if (bVerbose)
                printf("Testing %s...\n", psEntry->pszName);

            char **papszMD =
                VSIGetFileMetadata(osFilenameInZip.c_str(), "ZIP", nullptr);
            bool bSeekOptimizedFound =
                CSLFetchNameValue(papszMD, "SOZIP_FOUND") != nullptr;
            bool bSeekOptimizedValid =
                CSLFetchNameValue(papszMD, "SOZIP_VALID") != nullptr;
            const char *pszChunkSize =
                CSLFetchNameValue(papszMD, "SOZIP_CHUNK_SIZE");
            if (bSeekOptimizedValid)
            {
                if (bVerbose)
                    printf("  %s has an associated .sozip.idx file\n",
                           psEntry->pszName);

                const char *pszStartIdxDataOffset =
                    CSLFetchNameValue(papszMD, "SOZIP_START_DATA_OFFSET");
                const vsi_l_offset nStartIdxOffset =
                    std::strtoull(pszStartIdxDataOffset, nullptr, 10);
                VSILFILE *fpRaw = VSIFOpenL(pszZipFilename, "rb");
                CPLAssert(fpRaw);

                if (VSIFSeekL(fpRaw, nStartIdxOffset + 4, SEEK_SET) != 0)
                {
                    fprintf(stderr, "VSIFSeekL() failed.\n");
                    ret = 1;
                }
                uint32_t nToSkip = 0;
                if (VSIFReadL(&nToSkip, sizeof(nToSkip), 1, fpRaw) != 1)
                {
                    fprintf(stderr, "VSIFReadL() failed.\n");
                    ret = 1;
                }
                CPL_LSBPTR32(&nToSkip);

                if (VSIFSeekL(fpRaw, nStartIdxOffset + 32 + nToSkip,
                              SEEK_SET) != 0)
                {
                    fprintf(stderr, "VSIFSeekL() failed.\n");
                    ret = 1;
                }
                const int nChunkSize = atoi(pszChunkSize);
                const uint64_t nCompressedSize = std::strtoull(
                    CSLFetchNameValue(papszMD, "COMPRESSED_SIZE"), nullptr, 10);
                const uint64_t nUncompressedSize = std::strtoull(
                    CSLFetchNameValue(papszMD, "UNCOMPRESSED_SIZE"), nullptr,
                    10);
                if (nChunkSize == 0 ||  // cannot happen
                    (nUncompressedSize - 1) / nChunkSize >
                        static_cast<uint64_t>(std::numeric_limits<int>::max()))
                {
                    fprintf(
                        stderr,
                        "* File %s has a SOZip index, but (nUncompressedSize - "
                        "1) / nChunkSize > INT_MAX !\n",
                        psEntry->pszName);
                    nCountInvalidSOZIP++;
                    ret = 1;
                    CSLDestroy(papszMD);
                    continue;
                }
                int nChunksItems =
                    static_cast<int>((nUncompressedSize - 1) / nChunkSize);

                if (bVerbose)
                    printf("  %s: checking index offset values...\n",
                           psEntry->pszName);

                std::vector<uint64_t> anOffsets;
                try
                {
                    anOffsets.reserve(nChunksItems);
                }
                catch (const std::exception &)
                {
                    nChunksItems = 0;
                    fprintf(stderr,
                            "Cannot allocate memory for chunk offsets.\n");
                    ret = 1;
                }

                for (int i = 0; i < nChunksItems; ++i)
                {
                    uint64_t nOffset64 = 0;
                    if (VSIFReadL(&nOffset64, sizeof(nOffset64), 1, fpRaw) != 1)
                    {
                        fprintf(stderr, "VSIFReadL() failed.\n");
                        ret = 1;
                    }
                    CPL_LSBPTR64(&nOffset64);
                    if (nOffset64 >= nCompressedSize)
                    {
                        bSeekOptimizedValid = false;
                        fprintf(stderr,
                                "Error: file %s, offset[%d] (= " CPL_FRMT_GUIB
                                ") >= compressed_size is invalid.\n",
                                psEntry->pszName, i,
                                static_cast<GUIntBig>(nOffset64));
                    }
                    if (!anOffsets.empty())
                    {
                        const auto nPrevOffset = anOffsets.back();
                        if (nOffset64 <= nPrevOffset)
                        {
                            bSeekOptimizedValid = false;
                            fprintf(
                                stderr,
                                "Error: file %s, offset[%d] (= " CPL_FRMT_GUIB
                                ") <= offset[%d] (= " CPL_FRMT_GUIB ")\n",
                                psEntry->pszName, i + 1,
                                static_cast<GUIntBig>(nOffset64), i,
                                static_cast<GUIntBig>(nPrevOffset));
                        }
                    }
                    else if (nOffset64 < 9)
                    {
                        bSeekOptimizedValid = false;
                        fprintf(stderr,
                                "Error: file %s, offset[0] (= " CPL_FRMT_GUIB
                                ") is invalid.\n",
                                psEntry->pszName,
                                static_cast<GUIntBig>(nOffset64));
                    }
                    anOffsets.push_back(nOffset64);
                }

                if (bVerbose)
                    printf("  %s: checking chunks can be independently "
                           "decompressed...\n",
                           psEntry->pszName);

                const char *pszStartDataOffset =
                    CSLFetchNameValue(papszMD, "START_DATA_OFFSET");
                const vsi_l_offset nStartOffset =
                    std::strtoull(pszStartDataOffset, nullptr, 10);
                VSILFILE *fp = VSIFOpenL(osFilenameInZip.c_str(), "rb");
                if (!fp)
                {
                    bSeekOptimizedValid = false;
                    fprintf(stderr, "Error: cannot open %s\n",
                            osFilenameInZip.c_str());
                }
                std::vector<GByte> abyData;
                try
                {
                    abyData.resize(nChunkSize);
                }
                catch (const std::exception &)
                {
                    fprintf(stderr, "Cannot allocate memory for chunk data.\n");
                    ret = 1;
                }
                for (int i = 0; fp != nullptr && i < nChunksItems; ++i)
                {
                    if (VSIFSeekL(fpRaw, nStartOffset + anOffsets[i] - 9,
                                  SEEK_SET) != 0)
                    {
                        fprintf(stderr, "VSIFSeekL() failed.\n");
                        ret = 1;
                    }
                    GByte abyEnd[9] = {0};
                    if (VSIFReadL(abyEnd, 9, 1, fpRaw) != 1)
                    {
                        fprintf(stderr, "VSIFReadL() failed.\n");
                        ret = 1;
                    }
                    if (memcmp(abyEnd, "\x00\x00\xFF\xFF\x00\x00\x00\xFF\xFF",
                               9) != 0)
                    {
                        bSeekOptimizedValid = false;
                        fprintf(
                            stderr,
                            "Error: file %s, chunk[%d] is not terminated by "
                            "\\x00\\x00\\xFF\\xFF\\x00\\x00\\x00\\xFF\\xFF.\n",
                            psEntry->pszName, i);
                    }
                    if (!abyData.empty())
                    {
                        if (VSIFSeekL(fp,
                                      static_cast<vsi_l_offset>(i) * nChunkSize,
                                      SEEK_SET) != 0)
                        {
                            fprintf(stderr, "VSIFSeekL() failed.\n");
                            ret = 1;
                        }
                        const size_t nRead =
                            VSIFReadL(&abyData[0], 1, nChunkSize, fp);
                        if (nRead != static_cast<size_t>(nChunkSize))
                        {
                            bSeekOptimizedValid = false;
                            fprintf(stderr,
                                    "Error: file %s, chunk[%d] cannot be fully "
                                    "read.\n",
                                    psEntry->pszName, i);
                        }
                    }
                }

                if (fp)
                {
                    if (VSIFSeekL(fp,
                                  static_cast<vsi_l_offset>(nChunksItems) *
                                      nChunkSize,
                                  SEEK_SET) != 0)
                    {
                        fprintf(stderr, "VSIFSeekL() failed.\n");
                        ret = 1;
                    }
                    const size_t nRead =
                        VSIFReadL(&abyData[0], 1, nChunkSize, fp);
                    if (nRead != static_cast<size_t>(
                                     nUncompressedSize -
                                     static_cast<vsi_l_offset>(nChunksItems) *
                                         nChunkSize))
                    {
                        bSeekOptimizedValid = false;
                        fprintf(
                            stderr,
                            "Error: file %s, chunk[%d] cannot be fully read.\n",
                            psEntry->pszName, nChunksItems);
                    }

                    VSIFCloseL(fp);
                }

                VSIFCloseL(fpRaw);
            }

            if (bSeekOptimizedValid)
            {
                printf("* File %s has a valid SOZip index, using chunk_size = "
                       "%s.\n",
                       psEntry->pszName, pszChunkSize);
                nCountValidSOZIP++;
            }
            else if (bSeekOptimizedFound)
            {
                fprintf(stderr,
                        "* File %s has a SOZip index, but is is invalid!\n",
                        psEntry->pszName);
                nCountInvalidSOZIP++;
                ret = 1;
            }
            CSLDestroy(papszMD);
        }
    }

    VSICloseDir(psDir);

    if (ret == 0)
    {
        if (nCountValidSOZIP > 0)
        {
            printf("-----\n");
            printf("%s is a valid .zip file, and contains %d SOZip-enabled "
                   "file(s).\n",
                   pszZipFilename, nCountValidSOZIP);
        }
        else
            printf("%s is a valid .zip file, but does not contain any "
                   "SOZip-enabled files.\n",
                   pszZipFilename);
    }
    else
    {
        if (nCountInvalidSOZIP > 0)
            printf("-----\n");
        fprintf(stderr, "%s is not a valid SOZip file!\n", pszZipFilename);
    }

    return ret;
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
    bool bValidate = false;
    const char *pszZipFilename = nullptr;
    const char *pszOptimizeFrom = nullptr;
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
        else if (strcmp(papszArgv[iArg], "--validate") == 0)
        {
            bValidate = true;
        }
        else if (strcmp(papszArgv[iArg], "--optimize-from") == 0 &&
                 iArg + 1 < nArgc)
        {
            ++iArg;
            pszOptimizeFrom = papszArgv[iArg];
        }
        else if (STARTS_WITH(papszArgv[iArg], "--optimize-from="))
        {
            pszOptimizeFrom = papszArgv[iArg] + strlen("--optimize-from=");
        }
        else if (strcmp(papszArgv[iArg], "--enable-sozip") == 0 &&
                 iArg + 1 < nArgc)
        {
            ++iArg;
            aosOptions.SetNameValue("SOZIP_ENABLED", papszArgv[iArg]);
        }
        else if (STARTS_WITH(papszArgv[iArg], "--enable-sozip="))
        {
            aosOptions.SetNameValue(
                "SOZIP_ENABLED", papszArgv[iArg] + strlen("--enable-sozip="));
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
        else if (strcmp(papszArgv[iArg], "--content-type") == 0 &&
                 iArg + 1 < nArgc)
        {
            ++iArg;
            aosOptions.SetNameValue("CONTENT_TYPE", papszArgv[iArg]);
        }
        else if (STARTS_WITH(papszArgv[iArg], "--content-type="))
        {
            aosOptions.SetNameValue(
                "CONTENT_TYPE", papszArgv[iArg] + strlen("--content-type="));
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

    if (!pszZipFilename)
    {
        Usage("Missing zip filename");
        return 1;
    }

    if ((bValidate ? 1 : 0) + (bList ? 1 : 0) + (!aosFiles.empty() ? 1 : 0) +
            (pszOptimizeFrom != nullptr ? 1 : 0) >
        1)
    {
        Usage("--validate, --list, --optimize-from and create/append modes are "
              "mutually exclusive");
        return 1;
    }

    if (!bList && !bValidate && pszOptimizeFrom == nullptr && aosFiles.empty())
    {
        Usage("Missing source filename(s)");
        return 1;
    }

    if (!EQUAL(CPLGetExtension(pszZipFilename), "zip"))
    {
        Usage("Extension of zip filename should be .zip");
        return 1;
    }

    if (bValidate)
    {
        return Validate(pszZipFilename, bVerbose);
    }

    if (bList)
    {
        VSIDIR *psDir = VSIOpenDir(
            (std::string("/vsizip/") + pszZipFilename).c_str(), -1, nullptr);
        if (psDir == nullptr)
            return 1;
        printf("  Length          DateTime        Seek-optimized / chunk size  "
               "Name               Properties\n");
        /* clang-format off */
        printf("-----------  -------------------  ---------------------------  -----------------  --------------\n");
        /* clang-format on */
        while (auto psEntry = VSIGetNextDirEntry(psDir))
        {
            if (!VSI_ISDIR(psEntry->nMode))
            {
                struct tm brokenDown;
                CPLUnixTimeToYMDHMS(psEntry->nMTime, &brokenDown);
                const std::string osFilename = std::string("/vsizip/{") +
                                               pszZipFilename + "}/" +
                                               psEntry->pszName;
                std::string osProperties;
                char **papszMDGeneric =
                    VSIGetFileMetadata(osFilename.c_str(), nullptr, nullptr);
                for (char **papszIter = papszMDGeneric;
                     papszIter && papszIter[0]; ++papszIter)
                {
                    if (!osProperties.empty())
                        osProperties += ',';
                    osProperties += *papszIter;
                }
                CSLDestroy(papszMDGeneric);
                char **papszMD =
                    VSIGetFileMetadata(osFilename.c_str(), "ZIP", nullptr);
                bool bSeekOptimized =
                    CSLFetchNameValue(papszMD, "SOZIP_VALID") != nullptr;
                const char *pszChunkSize =
                    CSLFetchNameValue(papszMD, "SOZIP_CHUNK_SIZE");
                printf("%11" CPL_FRMT_GB_WITHOUT_PREFIX
                       "u  %04d-%02d-%02d %02d:%02d:%02d  %s  %s               "
                       "%s\n",
                       static_cast<GUIntBig>(psEntry->nSize),
                       brokenDown.tm_year + 1900, brokenDown.tm_mon + 1,
                       brokenDown.tm_mday, brokenDown.tm_hour,
                       brokenDown.tm_min, brokenDown.tm_sec,
                       bSeekOptimized
                           ? CPLSPrintf("   yes (%9s bytes)   ", pszChunkSize)
                           : "                           ",
                       psEntry->pszName, osProperties.c_str());
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
        {
            if (pszOptimizeFrom)
            {
                fprintf(
                    stderr,
                    "%s already exists. Use --overwrite or delete it before.\n",
                    pszZipFilename);
                return 1;
            }
            aosOptionsCreateZip.SetNameValue("APPEND", "TRUE");
        }
    }

    uint64_t nTotalSize = 0;
    std::vector<uint64_t> anFileSizes;

    std::string osRemovePrefix;
    if (pszOptimizeFrom)
    {
        VSIDIR *psDir = VSIOpenDir(
            (std::string("/vsizip/") + pszOptimizeFrom).c_str(), -1, nullptr);
        if (psDir == nullptr)
        {
            fprintf(stderr, "%s is not a valid .zip file\n", pszOptimizeFrom);
            return 1;
        }

        osRemovePrefix = std::string("/vsizip/{") + pszOptimizeFrom + "}/";
        while (auto psEntry = VSIGetNextDirEntry(psDir))
        {
            if (!VSI_ISDIR(psEntry->nMode))
            {
                const std::string osFilenameInZip =
                    osRemovePrefix + psEntry->pszName;
                aosFiles.AddString(osFilenameInZip.c_str());
            }
        }
        VSICloseDir(psDir);
    }
    else if (bRecurse)
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

    if (!hZIP)
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
        else if (!osRemovePrefix.empty() &&
                 STARTS_WITH(osArchiveFilename.c_str(), osRemovePrefix.c_str()))
        {
            osArchiveFilename = osArchiveFilename.substr(osRemovePrefix.size());
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
