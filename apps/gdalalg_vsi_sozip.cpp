/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "sozip" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vsi_sozip.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_time.h"

#include <limits>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALVSISOZIPCreateBaseAlgorithm                   */
/************************************************************************/

class GDALVSISOZIPCreateBaseAlgorithm /* non final */ : public GDALAlgorithm
{
  protected:
    GDALVSISOZIPCreateBaseAlgorithm(const std::string &name,
                                    const std::string &description,
                                    const std::string &helpURL,
                                    bool optimizeFrom)
        : GDALAlgorithm(name, description, helpURL),
          m_optimizeFrom(optimizeFrom)
    {
        AddProgressArg();
        if (optimizeFrom)
            AddArg("input", 'i', _("Input ZIP filename"), &m_inputFilenames)
                .SetRequired()
                .SetPositional()
                .SetMaxCount(1);
        else
            AddArg("input", 'i', _("Input filenames"), &m_inputFilenames)
                .SetRequired()
                .SetPositional();
        AddArg("output", 'o', _("Output ZIP filename"), &m_zipFilename)
            .SetRequired()
            .SetPositional()
            .AddValidationAction(
                [this]()
                {
                    if (!EQUAL(
                            CPLGetExtensionSafe(m_zipFilename.c_str()).c_str(),
                            "zip"))
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "Extension of zip filename should be .zip");
                        return false;
                    }
                    return true;
                });
        AddOverwriteArg(&m_overwrite);
        if (!optimizeFrom)
        {
            AddArg("recursive", 'r',
                   _("Travels the directory structure of the specified "
                     "directories recursively"),
                   &m_recursive)
                .AddHiddenAlias("recurse");
        }
        if (!optimizeFrom)
        {
            AddArg("no-paths", 'j',
                   _("Store just the name of a saved file, and do not store "
                     "directory names"),
                   &m_noDirName)
                .AddAlias("junk-paths");
        }
        AddArg("enable-sozip", 0,
               _("Whether to automatically/systematically/never apply the "
                 "SOZIP optimization"),
               &m_mode)
            .SetDefault(m_mode)
            .SetChoices("auto", "yes", "no");
        AddArg("sozip-chunk-size", 0, _("Chunk size for a seek-optimized file"),
               &m_chunkSize)
            .SetMetaVar("<value in bytes or with K/M suffix>")
            .SetDefault(m_chunkSize)
            .SetMinCharCount(1);
        AddArg(
            "sozip-min-file-size", 0,
            _("Minimum file size to decide if a file should be seek-optimized"),
            &m_minFileSize)
            .SetMetaVar("<value in bytes or with K/M/G suffix>")
            .SetDefault(m_minFileSize)
            .SetMinCharCount(1);
        if (!optimizeFrom)
            AddArg("content-type", 0,
                   _("Store the Content-Type of the file being added."),
                   &m_contentType)
                .SetMinCharCount(1);

        AddOutputStringArg(&m_output);
        AddArg("quiet", 'q', _("Quiet mode"), &m_quiet).SetOnlyForCLI();
        AddArg("stdout", 0,
               _("Directly output on stdout. If enabled, "
                 "output-string will be empty"),
               &m_stdout)
            .SetHiddenForCLI();
    }

  private:
    const bool m_optimizeFrom;
    std::vector<std::string> m_inputFilenames{};
    std::string m_zipFilename{};
    bool m_overwrite = false;
    bool m_recursive = false;
    bool m_noDirName = false;
    std::string m_mode = "auto";
    std::string m_chunkSize = "32768";
    std::string m_minFileSize = "1 MB";
    std::string m_contentType{};
    std::string m_output{};
    bool m_stdout = false;
    bool m_quiet = false;

    bool RunImpl(GDALProgressFunc, void *) override;

    void Output(const std::string &s)
    {
        if (!m_quiet)
        {
            if (m_stdout)
                printf("%s", s.c_str());
            else
                m_output += s;
        }
    }
};

/************************************************************************/
/*                GDALVSISOZIPCreateBaseAlgorithm::RunImpl()            */
/************************************************************************/

bool GDALVSISOZIPCreateBaseAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
    CPLStringList aosOptions;
    aosOptions.SetNameValue("SOZIP_ENABLED", m_mode.c_str());
    aosOptions.SetNameValue("SOZIP_CHUNK_SIZE", m_chunkSize.c_str());
    aosOptions.SetNameValue("SOZIP_MIN_FILE_SIZE", m_minFileSize.c_str());
    if (!m_contentType.empty())
        aosOptions.SetNameValue("CONTENT_TYPE", m_contentType.c_str());

    VSIStatBufL sBuf;
    CPLStringList aosOptionsCreateZip;
    if (m_overwrite)
    {
        VSIUnlink(m_zipFilename.c_str());
    }
    else
    {
        if (VSIStatExL(m_zipFilename.c_str(), &sBuf, VSI_STAT_EXISTS_FLAG) == 0)
        {
            if (m_optimizeFrom)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "%s already exists. Use --overwrite",
                            m_zipFilename.c_str());
                return false;
            }
            aosOptionsCreateZip.SetNameValue("APPEND", "TRUE");
        }
    }

    std::vector<std::string> aosFiles = m_inputFilenames;
    std::string osRemovePrefix;
    if (m_optimizeFrom)
    {
        std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDir(
            VSIOpenDir(
                std::string("/vsizip/").append(m_inputFilenames[0]).c_str(), -1,
                nullptr),
            VSICloseDir);
        if (!psDir)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "%s is not a valid .zip file",
                        m_inputFilenames[0].c_str());
            return false;
        }

        osRemovePrefix =
            std::string("/vsizip/{").append(m_inputFilenames[0]).append("}/");
        while (const auto psEntry = VSIGetNextDirEntry(psDir.get()))
        {
            if (!VSI_ISDIR(psEntry->nMode))
            {
                aosFiles.push_back(osRemovePrefix + psEntry->pszName);
            }
        }
    }
    else if (m_recursive)
    {
        std::vector<std::string> aosNewFiles;
        for (const std::string &osFile : m_inputFilenames)
        {
            if (VSIStatL(osFile.c_str(), &sBuf) == 0 && VSI_ISDIR(sBuf.st_mode))
            {
                std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDir(
                    VSIOpenDir(osFile.c_str(), -1, nullptr), VSICloseDir);
                if (!psDir)
                    return false;
                while (const auto psEntry = VSIGetNextDirEntry(psDir.get()))
                {
                    if (!VSI_ISDIR(psEntry->nMode))
                    {
                        std::string osName(osFile);
                        if (osName.back() != '/')
                            osName += '/';
                        osName += psEntry->pszName;
                        aosNewFiles.push_back(std::move(osName));
                        if (aosNewFiles.size() > 10 * 1000 * 1000)
                        {
                            ReportError(CE_Failure, CPLE_NotSupported,
                                        "Too many source files");
                            return false;
                        }
                    }
                }
            }
        }
        aosFiles = std::move(aosNewFiles);
    }

    uint64_t nTotalSize = 0;
    std::vector<uint64_t> anFileSizes;

    if (pfnProgress)
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
                ReportError(CE_Failure, CPLE_AppDefined, "%s does not exist",
                            aosFiles[i].c_str());
                return false;
            }
        }
    }

    std::unique_ptr<void, decltype(&CPLCloseZip)> hZIP(
        CPLCreateZip(m_zipFilename.c_str(), aosOptionsCreateZip.List()),
        CPLCloseZip);
    if (!hZIP)
        return false;

    uint64_t nCurSize = 0;
    for (size_t i = 0; i < aosFiles.size(); ++i)
    {
        if (!m_quiet)
        {
            Output(CPLSPrintf("Adding %s... (%d/%d)\n", aosFiles[i].c_str(),
                              int(i + 1), static_cast<int>(aosFiles.size())));
        }

        if (VSIStatL(aosFiles[i].c_str(), &sBuf) != 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "%s does not exist",
                        aosFiles[i].c_str());
            return false;
        }
        else if (VSI_ISDIR(sBuf.st_mode))
        {
            ReportError(CE_Failure, CPLE_AppDefined, "%s is a directory",
                        aosFiles[i].c_str());
            return false;
        }

        std::string osArchiveFilename(aosFiles[i]);
        if (m_noDirName)
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

        std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)>
            pScaledProgress(nullptr, GDALDestroyScaledProgress);
        if (nTotalSize != 0)
        {
            pScaledProgress.reset(GDALCreateScaledProgress(
                double(nCurSize) / nTotalSize,
                double(nCurSize + anFileSizes[i]) / nTotalSize, pfnProgress,
                pProgressData));
            nCurSize += anFileSizes[i];
        }

        const CPLErr eErr = CPLAddFileInZip(
            hZIP.get(), osArchiveFilename.c_str(), aosFiles[i].c_str(), nullptr,
            aosOptions.List(), pScaledProgress ? GDALScaledProgress : nullptr,
            pScaledProgress.get());
        if (eErr != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Failed adding %s",
                        aosFiles[i].c_str());
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                    GDALVSISOZIPCreateAlgorithm                       */
/************************************************************************/

class GDALVSISOZIPCreateAlgorithm final : public GDALVSISOZIPCreateBaseAlgorithm
{
  public:
    static constexpr const char *NAME = "create";
    static constexpr const char *DESCRIPTION =
        "Create a Seek-optimized ZIP (SOZIP) file.";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_sozip.html";

    GDALVSISOZIPCreateAlgorithm()
        : GDALVSISOZIPCreateBaseAlgorithm(NAME, DESCRIPTION, HELP_URL, false)
    {
    }
};

/************************************************************************/
/*                  GDALVSISOZIPOptimizeAlgorithm                       */
/************************************************************************/

class GDALVSISOZIPOptimizeAlgorithm final
    : public GDALVSISOZIPCreateBaseAlgorithm
{
  public:
    static constexpr const char *NAME = "optimize";
    static constexpr const char *DESCRIPTION =
        "Create a Seek-optimized ZIP (SOZIP) file from a regular ZIP file.";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_sozip.html";

    GDALVSISOZIPOptimizeAlgorithm()
        : GDALVSISOZIPCreateBaseAlgorithm(NAME, DESCRIPTION, HELP_URL, true)
    {
    }
};

/************************************************************************/
/*                      GDALVSISOZIPListAlgorithm                       */
/************************************************************************/

class GDALVSISOZIPListAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "list";
    static constexpr const char *DESCRIPTION =
        "List content of a ZIP file, with SOZIP related information.";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_sozip.html";

    GDALVSISOZIPListAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        AddArg("input", 'i', _("Input ZIP filename"), &m_zipFilename)
            .SetRequired()
            .SetPositional();
        AddOutputStringArg(&m_output);
    }

  private:
    std::string m_zipFilename{};
    std::string m_output{};

    bool RunImpl(GDALProgressFunc, void *) override;
};

/************************************************************************/
/*                 GDALVSISOZIPListAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALVSISOZIPListAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDir(
        VSIOpenDir(std::string("/vsizip/").append(m_zipFilename).c_str(), -1,
                   nullptr),
        VSICloseDir);
    if (!psDir)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "%s is not a valid .zip file",
                    m_zipFilename.c_str());
        return false;
    }

    m_output = "  Length          DateTime        Seek-optimized / chunk size  "
               "Name               Properties\n";
    /* clang-format off */
    m_output += "-----------  -------------------  ---------------------------  -----------------  --------------\n";
    /* clang-format on */

    while (const auto psEntry = VSIGetNextDirEntry(psDir.get()))
    {
        if (!VSI_ISDIR(psEntry->nMode))
        {
            struct tm brokenDown;
            CPLUnixTimeToYMDHMS(psEntry->nMTime, &brokenDown);
            const std::string osFilename = std::string("/vsizip/{")
                                               .append(m_zipFilename)
                                               .append("}/")
                                               .append(psEntry->pszName);
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
            const char *pszChunkSize = aosMD.FetchNameValue("SOZIP_CHUNK_SIZE");
            m_output += CPLSPrintf(
                "%11" CPL_FRMT_GB_WITHOUT_PREFIX
                "u  %04d-%02d-%02d %02d:%02d:%02d  %s  %s               "
                "%s\n",
                static_cast<GUIntBig>(psEntry->nSize),
                brokenDown.tm_year + 1900, brokenDown.tm_mon + 1,
                brokenDown.tm_mday, brokenDown.tm_hour, brokenDown.tm_min,
                brokenDown.tm_sec,
                bSeekOptimized
                    ? CPLSPrintf("   yes (%9s bytes)   ", pszChunkSize)
                    : "                           ",
                psEntry->pszName, osProperties.c_str());
        }
    }
    return true;
}

/************************************************************************/
/*                      GDALVSISOZIPValidateAlgorithm                   */
/************************************************************************/

class GDALVSISOZIPValidateAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "validate";
    static constexpr const char *DESCRIPTION =
        "Validate a ZIP file, possibly using SOZIP optimization.";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_sozip.html";

    GDALVSISOZIPValidateAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        AddArg("input", 'i', _("Input ZIP filename"), &m_zipFilename)
            .SetRequired()
            .SetPositional();
        AddOutputStringArg(&m_output);
        AddArg("quiet", 'q', _("Quiet mode"), &m_quiet)
            .SetOnlyForCLI()
            .SetMutualExclusionGroup("quiet-verbose");
        AddArg("verbose", 'v', _("Turn on verbose mode"), &m_verbose)
            .SetOnlyForCLI()
            .SetMutualExclusionGroup("quiet-verbose");
        AddArg("stdout", 0,
               _("Directly output on stdout. If enabled, "
                 "output-string will be empty"),
               &m_stdout)
            .SetHiddenForCLI();
    }

  private:
    std::string m_zipFilename{};
    std::string m_output{};
    bool m_stdout = false;
    bool m_quiet = false;
    bool m_verbose = false;

    bool RunImpl(GDALProgressFunc, void *) override;

    void Output(const std::string &s)
    {
        if (!m_quiet)
        {
            if (m_stdout)
                printf("%s", s.c_str());
            else
                m_output += s;
        }
    }
};

/************************************************************************/
/*                 GDALVSISOZIPValidateAlgorithm::RunImpl()             */
/************************************************************************/

bool GDALVSISOZIPValidateAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> psDir(
        VSIOpenDir(std::string("/vsizip/").append(m_zipFilename).c_str(), -1,
                   nullptr),
        VSICloseDir);
    if (!psDir)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "%s is not a valid .zip file",
                    m_zipFilename.c_str());
        return false;
    }

    int nCountValidSOZIP = 0;
    bool ret = true;
    const bool bVerbose = m_verbose;
    while (const auto psEntry = VSIGetNextDirEntry(psDir.get()))
    {
        if (!VSI_ISDIR(psEntry->nMode))
        {
            const std::string osFilenameInZip = std::string("/vsizip/{")
                                                    .append(m_zipFilename)
                                                    .append("}/")
                                                    .append(psEntry->pszName);
            if (bVerbose)
                Output(CPLSPrintf("Testing %s...\n", psEntry->pszName));

            const CPLStringList aosMD(
                VSIGetFileMetadata(osFilenameInZip.c_str(), "ZIP", nullptr));
            bool bSeekOptimizedFound =
                aosMD.FetchNameValue("SOZIP_FOUND") != nullptr;
            bool bSeekOptimizedValid =
                aosMD.FetchNameValue("SOZIP_VALID") != nullptr;
            const char *pszChunkSize = aosMD.FetchNameValue("SOZIP_CHUNK_SIZE");
            if (bSeekOptimizedValid)
            {
                if (bVerbose)
                {
                    Output(
                        CPLSPrintf("  %s has an associated .sozip.idx file\n",
                                   psEntry->pszName));
                }

                const char *pszStartIdxDataOffset =
                    aosMD.FetchNameValue("SOZIP_START_DATA_OFFSET");
                const vsi_l_offset nStartIdxOffset =
                    std::strtoull(pszStartIdxDataOffset, nullptr, 10);
                VSILFILE *fpRaw = VSIFOpenL(m_zipFilename.c_str(), "rb");
                CPLAssert(fpRaw);

                if (VSIFSeekL(fpRaw, nStartIdxOffset + 4, SEEK_SET) != 0)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "VSIFSeekL() failed.");
                    ret = false;
                }
                uint32_t nToSkip = 0;
                if (VSIFReadL(&nToSkip, sizeof(nToSkip), 1, fpRaw) != 1)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "VSIFReadL() failed.");
                    ret = false;
                }
                CPL_LSBPTR32(&nToSkip);

                if (VSIFSeekL(fpRaw, nStartIdxOffset + 32 + nToSkip,
                              SEEK_SET) != 0)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "VSIFSeekL() failed.");
                    ret = false;
                }
                const int nChunkSize = atoi(pszChunkSize);
                const uint64_t nCompressedSize = std::strtoull(
                    aosMD.FetchNameValue("COMPRESSED_SIZE"), nullptr, 10);
                const uint64_t nUncompressedSize = std::strtoull(
                    aosMD.FetchNameValue("UNCOMPRESSED_SIZE"), nullptr, 10);
                if (nChunkSize == 0 ||  // cannot happen
                    (nUncompressedSize - 1) / nChunkSize >
                        static_cast<uint64_t>(std::numeric_limits<int>::max()))
                {
                    ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "* File %s has a SOZip index, but (nUncompressedSize - "
                        "1) / nChunkSize > INT_MAX !",
                        psEntry->pszName);
                    ret = false;
                    continue;
                }

                int nChunksItems =
                    static_cast<int>((nUncompressedSize - 1) / nChunkSize);

                if (bVerbose)
                {
                    Output(CPLSPrintf("  %s: checking index offset values...\n",
                                      psEntry->pszName));
                }

                std::vector<uint64_t> anOffsets;
                try
                {
                    anOffsets.reserve(nChunksItems);
                }
                catch (const std::exception &)
                {
                    nChunksItems = 0;
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot allocate memory for chunk offsets.");
                    ret = false;
                }

                for (int i = 0; i < nChunksItems; ++i)
                {
                    uint64_t nOffset64 = 0;
                    if (VSIFReadL(&nOffset64, sizeof(nOffset64), 1, fpRaw) != 1)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "VSIFReadL() failed.");
                        ret = false;
                    }
                    CPL_LSBPTR64(&nOffset64);
                    if (nOffset64 >= nCompressedSize)
                    {
                        bSeekOptimizedValid = false;
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Error: file %s, offset[%d] (= " CPL_FRMT_GUIB
                            ") >= compressed_size is invalid.",
                            psEntry->pszName, i,
                            static_cast<GUIntBig>(nOffset64));
                    }
                    if (!anOffsets.empty())
                    {
                        const auto nPrevOffset = anOffsets.back();
                        if (nOffset64 <= nPrevOffset)
                        {
                            bSeekOptimizedValid = false;
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Error: file %s, offset[%d] (= " CPL_FRMT_GUIB
                                ") <= offset[%d] (= " CPL_FRMT_GUIB ")",
                                psEntry->pszName, i + 1,
                                static_cast<GUIntBig>(nOffset64), i,
                                static_cast<GUIntBig>(nPrevOffset));
                        }
                    }
                    else if (nOffset64 < 9)
                    {
                        bSeekOptimizedValid = false;
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Error: file %s, offset[0] (= " CPL_FRMT_GUIB
                            ") is invalid.",
                            psEntry->pszName, static_cast<GUIntBig>(nOffset64));
                    }
                    anOffsets.push_back(nOffset64);
                }

                if (bVerbose)
                {
                    Output(CPLSPrintf("  %s: checking if chunks can be "
                                      "independently decompressed...\n",
                                      psEntry->pszName));
                }

                const char *pszStartDataOffset =
                    aosMD.FetchNameValue("START_DATA_OFFSET");
                const vsi_l_offset nStartOffset =
                    std::strtoull(pszStartDataOffset, nullptr, 10);
                VSILFILE *fp = VSIFOpenL(osFilenameInZip.c_str(), "rb");
                if (!fp)
                {
                    bSeekOptimizedValid = false;
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Error: cannot open %s",
                                osFilenameInZip.c_str());
                }
                std::vector<GByte> abyData;
                try
                {
                    abyData.resize(nChunkSize);
                }
                catch (const std::exception &)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot allocate memory for chunk data.");
                    ret = false;
                }
                for (int i = 0; fp != nullptr && i < nChunksItems; ++i)
                {
                    if (VSIFSeekL(fpRaw, nStartOffset + anOffsets[i] - 9,
                                  SEEK_SET) != 0)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "VSIFSeekL() failed.");
                        ret = false;
                    }
                    GByte abyEnd[9] = {0};
                    if (VSIFReadL(abyEnd, 9, 1, fpRaw) != 1)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "VSIFReadL() failed.");
                        ret = false;
                    }
                    if (memcmp(abyEnd, "\x00\x00\xFF\xFF\x00\x00\x00\xFF\xFF",
                               9) != 0)
                    {
                        bSeekOptimizedValid = false;
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Error: file %s, chunk[%d] is not terminated by "
                            "\\x00\\x00\\xFF\\xFF\\x00\\x00\\x00\\xFF\\xFF.",
                            psEntry->pszName, i);
                    }
                    if (!abyData.empty())
                    {
                        if (VSIFSeekL(fp,
                                      static_cast<vsi_l_offset>(i) * nChunkSize,
                                      SEEK_SET) != 0)
                        {
                            ReportError(CE_Failure, CPLE_AppDefined,
                                        "VSIFSeekL() failed.");
                            ret = false;
                        }
                        const size_t nRead =
                            VSIFReadL(&abyData[0], 1, nChunkSize, fp);
                        if (nRead != static_cast<size_t>(nChunkSize))
                        {
                            bSeekOptimizedValid = false;
                            ReportError(
                                CE_Failure, CPLE_AppDefined,
                                "Error: file %s, chunk[%d] cannot be fully "
                                "read.",
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
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "VSIFSeekL() failed.");
                        ret = false;
                    }
                    const size_t nRead =
                        VSIFReadL(&abyData[0], 1, nChunkSize, fp);
                    if (nRead != static_cast<size_t>(
                                     nUncompressedSize -
                                     static_cast<vsi_l_offset>(nChunksItems) *
                                         nChunkSize))
                    {
                        bSeekOptimizedValid = false;
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "Error: file %s, chunk[%d] cannot be fully read.",
                            psEntry->pszName, nChunksItems);
                    }

                    VSIFCloseL(fp);
                }

                VSIFCloseL(fpRaw);
            }

            if (bSeekOptimizedValid)
            {
                Output(CPLSPrintf(
                    "* File %s has a valid SOZip index, using chunk_size = "
                    "%s.\n",
                    psEntry->pszName, pszChunkSize));
                nCountValidSOZIP++;
            }
            else if (bSeekOptimizedFound)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "* File %s has a SOZip index, but is is invalid!",
                            psEntry->pszName);
                ret = false;
            }
        }
    }

    if (ret)
    {
        if (nCountValidSOZIP > 0)
        {
            Output("-----\n");
            Output(CPLSPrintf(
                "%s is a valid .zip file, and contains %d SOZip-enabled "
                "file(s).\n",
                m_zipFilename.c_str(), nCountValidSOZIP));
        }
        else
            Output(
                CPLSPrintf("%s is a valid .zip file, but does not contain any "
                           "SOZip-enabled files.\n",
                           m_zipFilename.c_str()));
    }
    else
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "%s is not a valid SOZip file!", m_zipFilename.c_str());
    }
    return ret;
}

/************************************************************************/
/*               GDALVSISOZIPAlgorithm::GDALVSISOZIPAlgorithm()         */
/************************************************************************/

GDALVSISOZIPAlgorithm::GDALVSISOZIPAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    RegisterSubAlgorithm<GDALVSISOZIPCreateAlgorithm>();
    RegisterSubAlgorithm<GDALVSISOZIPOptimizeAlgorithm>();
    RegisterSubAlgorithm<GDALVSISOZIPListAlgorithm>();
    RegisterSubAlgorithm<GDALVSISOZIPValidateAlgorithm>();
}

//! @endcond
