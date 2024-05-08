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
#include "gdalargumentparser.h"

#include <limits>

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
    EarlySetConfigOptions(nArgc, papszArgv);
    nArgc = GDALGeneralCmdLineProcessor(nArgc, &papszArgv, 0);
    CPLStringList aosArgv;
    aosArgv.Assign(papszArgv, /* bTakeOwnership= */ true);
    if (nArgc < 1)
        std::exit(-nArgc);

    GDALArgumentParser argParser(aosArgv[0], /* bForBinary=*/true);

    argParser.add_description(_("Generate a seek-optimized ZIP (SOZip) file."));

    argParser.add_epilog(
        _("For more details, consult https://gdal.org/programs/sozip.html"));

    std::string osZipFilename;
    argParser.add_argument("zip_filename")
        .metavar("<zip_filename>")
        .store_into(osZipFilename)
        .help(_("ZIP filename."));

    bool bRecurse = false;
    argParser.add_argument("-r", "--recurse-paths")
        .store_into(bRecurse)
        .help(_("Travels the directory structure of the specified directories "
                "recursively."));

    bool bOverwrite = false;
    {
        auto &group = argParser.add_mutually_exclusive_group();
        group.add_argument("-g", "--grow")
            .flag()  // Default mode. Nothing to do
            .help(
                _("Grow an existing zip file with the content of the specified "
                  "filename(s). Default mode."));
        group.add_argument("--overwrite")
            .store_into(bOverwrite)
            .help(_("Overwrite the target zip file if it already exists."));
    }

    bool bList = false;
    bool bValidate = false;
    std::string osOptimizeFrom;
    std::vector<std::string> aosFiles;
    {
        auto &group = argParser.add_mutually_exclusive_group();
        group.add_argument("-l", "--list")
            .store_into(bList)
            .help(_("List the files contained in the zip file."));
        group.add_argument("--validate")
            .store_into(bValidate)
            .help(_("Validates a ZIP/SOZip file."));
        group.add_argument("--optimize-from")
            .metavar("<input.zip>")
            .store_into(osOptimizeFrom)
            .help(
                _("Re-process {input.zip} to generate a SOZip-optimized .zip"));
        group.add_argument("input_files")
            .metavar("<input_files>")
            .store_into(aosFiles)
            .help(_("Filename of the file to add."))
            .nargs(argparse::nargs_pattern::any);
    }

    bool bQuiet = false;
    bool bVerbose = false;
    argParser.add_group("Advanced options");
    {
        auto &group = argParser.add_mutually_exclusive_group();
        group.add_argument("--quiet").store_into(bQuiet).help(
            _("Quiet mode. No progress message is emitted on the standard "
              "output."));
        group.add_argument("--verbose")
            .store_into(bVerbose)
            .help(_("Verbose mode."));
    }
    bool bJunkPaths = false;
    argParser.add_argument("-j", "--junk-paths")
        .store_into(bJunkPaths)
        .help(
            _("Store just the name of a saved file (junk the path), and do not "
              "store directory names."));

    CPLStringList aosOptions;
    argParser.add_argument("--enable-sozip")
        .choices("auto", "yes", "no")
        .metavar("auto|yes|no")
        .action([&aosOptions](const std::string &s)
                { aosOptions.SetNameValue("SOZIP_ENABLED", s.c_str()); })
        .help(_("In auto mode, a file is seek-optimized only if its size is "
                "above the value of\n"
                "--sozip-chunk-size. In yes mode, all input files will be "
                "seek-optimized.\n"
                "In no mode, no input files will be seek-optimized."));
    argParser.add_argument("--sozip-chunk-size")
        .metavar("<value in bytes or with K/M suffix>")
        .action([&aosOptions](const std::string &s)
                { aosOptions.SetNameValue("SOZIP_CHUNK_SIZE", s.c_str()); })
        .help(_(
            "Chunk size for a seek-optimized file. Defaults to 32768 bytes."));
    argParser.add_argument("--sozip-min-file-size")
        .metavar("<value in bytes or with K/M/G suffix>")
        .action([&aosOptions](const std::string &s)
                { aosOptions.SetNameValue("SOZIP_MIN_FILE_SIZE", s.c_str()); })
        .help(
            _("Minimum file size to decide if a file should be seek-optimized. "
              "Defaults to 1 MB byte."));
    argParser.add_argument("--content-type")
        .metavar("<string>")
        .action([&aosOptions](const std::string &s)
                { aosOptions.SetNameValue("CONTENT_TYPE", s.c_str()); })
        .help(_("Store the Content-Type for the file being added."));

    try
    {
        argParser.parse_args(aosArgv);
    }
    catch (const std::exception &err)
    {
        argParser.display_error_and_usage(err);
        std::exit(1);
    }

    if (!bList && !bValidate && osOptimizeFrom.empty() && aosFiles.empty())
    {
        std::cerr << _("Missing source filename(s)") << std::endl << std::endl;
        std::cerr << argParser << std::endl;
        std::exit(1);
    }

    const char *pszZipFilename = osZipFilename.c_str();
    if (!EQUAL(CPLGetExtension(pszZipFilename), "zip"))
    {
        std::cerr << _("Extension of zip filename should be .zip") << std::endl
                  << std::endl;
        std::cerr << argParser << std::endl;
        std::exit(1);
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
                const CPLStringList aosMDGeneric(
                    VSIGetFileMetadata(osFilename.c_str(), nullptr, nullptr));
                for (const char *pszMDGeneric : aosMDGeneric)
                {
                    if (!osProperties.empty())
                        osProperties += ',';
                    osProperties += pszMDGeneric;
                }

                const CPLStringList aosMD(
                    VSIGetFileMetadata(osFilename.c_str(), "ZIP", nullptr));
                const bool bSeekOptimized =
                    aosMD.FetchNameValue("SOZIP_VALID") != nullptr;
                const char *pszChunkSize =
                    aosMD.FetchNameValue("SOZIP_CHUNK_SIZE");
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
            if (!osOptimizeFrom.empty())
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
    if (!osOptimizeFrom.empty())
    {
        VSIDIR *psDir = VSIOpenDir(
            (std::string("/vsizip/") + osOptimizeFrom).c_str(), -1, nullptr);
        if (psDir == nullptr)
        {
            fprintf(stderr, "%s is not a valid .zip file\n",
                    osOptimizeFrom.c_str());
            return 1;
        }

        osRemovePrefix =
            std::string("/vsizip/{").append(osOptimizeFrom).append("}/");
        while (auto psEntry = VSIGetNextDirEntry(psDir))
        {
            if (!VSI_ISDIR(psEntry->nMode))
            {
                const std::string osFilenameInZip =
                    osRemovePrefix + psEntry->pszName;
                aosFiles.push_back(osFilenameInZip);
            }
        }
        VSICloseDir(psDir);
    }
    else if (bRecurse)
    {
        std::vector<std::string> aosNewFiles;
        for (const std::string &osFile : aosFiles)
        {
            if (VSIStatL(osFile.c_str(), &sBuf) == 0 && VSI_ISDIR(sBuf.st_mode))
            {
                VSIDIR *psDir = VSIOpenDir(osFile.c_str(), -1, nullptr);
                if (psDir == nullptr)
                    return 1;
                while (auto psEntry = VSIGetNextDirEntry(psDir))
                {
                    if (!VSI_ISDIR(psEntry->nMode))
                    {
                        std::string osName(osFile);
                        if (osName.back() != '/')
                            osName += '/';
                        osName += psEntry->pszName;
                        aosNewFiles.push_back(osName);
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
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        anFileSizes.resize(aosFiles.size());
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        for (size_t i = 0; i < aosFiles.size(); ++i)
        {
            if (VSIStatL(aosFiles[i].c_str(), &sBuf) == 0)
            {
                anFileSizes[i] = sBuf.st_size;
                nTotalSize += sBuf.st_size;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s\n",
                         aosFiles[i].c_str());
                return 1;
            }
        }
    }

    void *hZIP = CPLCreateZip(pszZipFilename, aosOptionsCreateZip.List());

    if (!hZIP)
        return 1;

    uint64_t nCurSize = 0;
    for (size_t i = 0; i < aosFiles.size(); ++i)
    {
        if (bVerbose)
            printf("Adding %s... (%d/%d)\n", aosFiles[i].c_str(), int(i + 1),
                   static_cast<int>(aosFiles.size()));
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
        if (VSIStatL(aosFiles[i].c_str(), &sBuf) != 0 ||
            VSI_ISDIR(sBuf.st_mode))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s is not a regular file",
                     aosFiles[i].c_str());
            CPLCloseZip(hZIP);
            return 1;
        }

        std::string osArchiveFilename(aosFiles[i]);
        if (bJunkPaths)
        {
            osArchiveFilename = CPLGetFilename(aosFiles[i].c_str());
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
            CPLAddFileInZip(hZIP, osArchiveFilename.c_str(),
                            aosFiles[i].c_str(), nullptr, aosOptions.List(),
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
                     aosFiles[i].c_str());
            CPLCloseZip(hZIP);
            return 1;
        }
    }
    CPLCloseZip(hZIP);
    return 0;
}

MAIN_END
