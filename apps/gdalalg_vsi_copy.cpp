/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi copy" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vsi_copy.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*              GDALVSICopyAlgorithm::GDALVSICopyAlgorithm()            */
/************************************************************************/

GDALVSICopyAlgorithm::GDALVSICopyAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    {
        auto &arg =
            AddArg("source", 0, _("Source file or directory name"), &m_source)
                .SetPositional()
                .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
        arg.AddValidationAction(
            [this]()
            {
                if (m_source.empty())
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Source filename cannot be empty");
                    return false;
                }
                return true;
            });
    }
    {
        auto &arg =
            AddArg("destination", 0, _("Destination file or directory name"),
                   &m_destination)
                .SetPositional()
                .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
        arg.AddValidationAction(
            [this]()
            {
                if (m_destination.empty())
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Destination filename cannot be empty");
                    return false;
                }
                return true;
            });
    }

    AddArg("recursive", 'r', _("Copy subdirectories recursively"),
           &m_recursive);

    AddArg("skip-errors", 0, _("Skip errors"), &m_skip);
    AddProgressArg();
}

/************************************************************************/
/*                    GDALVSICopyAlgorithm::RunImpl()                   */
/************************************************************************/

bool GDALVSICopyAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                   void *pProgressData)
{
    if (m_recursive || cpl::ends_with(m_source, "/*") ||
        cpl::ends_with(m_source, "\\*"))
    {
        // Make sure that copy -r [srcdir/]lastsubdir targetdir' creates
        // targetdir/lastsubdir if targetdir already exists (like cp -r does).
        if (m_source.back() == '/')
            m_source.pop_back();

        if (!cpl::ends_with(m_source, "/*") && !cpl::ends_with(m_source, "\\*"))
        {
            VSIStatBufL statBufSrc;
            bool srcExists =
                VSIStatExL(m_source.c_str(), &statBufSrc,
                           VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0;
            if (!srcExists)
            {
                srcExists =
                    VSIStatExL(
                        std::string(m_source).append("/").c_str(), &statBufSrc,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0;
            }
            VSIStatBufL statBufDst;
            const bool dstExists =
                VSIStatExL(m_destination.c_str(), &statBufDst,
                           VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0;
            if (srcExists && VSI_ISDIR(statBufSrc.st_mode) && dstExists &&
                VSI_ISDIR(statBufDst.st_mode))
            {
                if (m_destination.back() == '/')
                    m_destination.pop_back();
                const auto srcLastSlashPos = m_source.rfind('/');
                if (srcLastSlashPos != std::string::npos)
                    m_destination += m_source.substr(srcLastSlashPos);
                else
                    m_destination = CPLFormFilenameSafe(
                        m_destination.c_str(), m_source.c_str(), nullptr);
            }
        }
        else
        {
            m_source.resize(m_source.size() - 2);
        }

        uint64_t curAmount = 0;
        return CopyRecursive(m_source, m_destination, 0, m_recursive ? -1 : 0,
                             curAmount, 0, pfnProgress, pProgressData);
    }
    else
    {
        VSIStatBufL statBufSrc;
        bool srcExists =
            VSIStatExL(m_source.c_str(), &statBufSrc,
                       VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0;
        if (!srcExists)
        {
            ReportError(CE_Failure, CPLE_FileIO, "%s does not exist",
                        m_source.c_str());
            return false;
        }
        if (VSI_ISDIR(statBufSrc.st_mode))
        {
            ReportError(CE_Failure, CPLE_FileIO,
                        "%s is a directory. Use -r/--recursive option",
                        m_source.c_str());
            return false;
        }

        return CopySingle(m_source, m_destination, ~(static_cast<uint64_t>(0)),
                          pfnProgress, pProgressData);
    }
}

/************************************************************************/
/*                 GDALVSICopyAlgorithm::CopySingle()                   */
/************************************************************************/

bool GDALVSICopyAlgorithm::CopySingle(const std::string &src,
                                      const std::string &dstIn, uint64_t size,
                                      GDALProgressFunc pfnProgress,
                                      void *pProgressData) const
{
    CPLDebug("gdal_vsi_copy", "Copying file %s...", src.c_str());
    VSIStatBufL sStat;
    std::string dst = dstIn;
    const bool bExists =
        VSIStatExL(dst.back() == '/' ? dst.c_str()
                                     : std::string(dst).append("/").c_str(),
                   &sStat, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0;
    if ((!bExists && dst.back() == '/') ||
        (bExists && VSI_ISDIR(sStat.st_mode)))
    {
        const std::string filename = CPLGetFilename(src.c_str());
        dst = CPLFormFilenameSafe(dst.c_str(), filename.c_str(), nullptr);
    }
    return VSICopyFile(src.c_str(), dst.c_str(), nullptr, size, nullptr,
                       pfnProgress, pProgressData) == 0 ||
           m_skip;
}

/************************************************************************/
/*                 GDALVSICopyAlgorithm::CopyRecursive()                */
/************************************************************************/

bool GDALVSICopyAlgorithm::CopyRecursive(const std::string &srcIn,
                                         const std::string &dst, int depth,
                                         int maxdepth, uint64_t &curAmount,
                                         uint64_t totalAmount,
                                         GDALProgressFunc pfnProgress,
                                         void *pProgressData) const
{
    std::string src(srcIn);
    if (src.back() == '/')
        src.pop_back();

    if (pfnProgress && depth == 0)
    {
        CPLDebug("gdal_vsi_copy", "Listing source files...");
        std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> dir(
            VSIOpenDir(src.c_str(), maxdepth, nullptr), VSICloseDir);
        if (dir)
        {
            while (const auto entry = VSIGetNextDirEntry(dir.get()))
            {
                if (!(entry->pszName[0] == '.' &&
                      (entry->pszName[1] == '.' || entry->pszName[1] == 0)))
                {
                    totalAmount += entry->nSize + 1;
                    if (!pfnProgress(0.0, "", pProgressData))
                        return false;
                }
            }
        }
    }
    totalAmount = std::max<uint64_t>(1, totalAmount);

    CPLDebug("gdal_vsi_copy", "Copying directory %s...", src.c_str());
    std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> dir(
        VSIOpenDir(src.c_str(), 0, nullptr), VSICloseDir);
    if (dir)
    {
        VSIStatBufL sStat;
        if (VSIStatL(dst.c_str(), &sStat) != 0)
        {
            if (VSIMkdir(dst.c_str(), 0755) != 0)
            {
                ReportError(m_skip ? CE_Warning : CE_Failure, CPLE_FileIO,
                            "Cannot create directory %s", dst.c_str());
                return m_skip;
            }
        }

        while (const auto entry = VSIGetNextDirEntry(dir.get()))
        {
            if (!(entry->pszName[0] == '.' &&
                  (entry->pszName[1] == '.' || entry->pszName[1] == 0)))
            {
                const std::string subsrc =
                    CPLFormFilenameSafe(src.c_str(), entry->pszName, nullptr);
                if (VSI_ISDIR(entry->nMode))
                {
                    const std::string subdest = CPLFormFilenameSafe(
                        dst.c_str(), entry->pszName, nullptr);
                    if (maxdepth < 0 || depth < maxdepth)
                    {
                        if (!CopyRecursive(subsrc, subdest, depth + 1, maxdepth,
                                           curAmount, totalAmount, pfnProgress,
                                           pProgressData) &&
                            !m_skip)
                        {
                            return false;
                        }
                    }
                    else
                    {
                        if (VSIStatL(subdest.c_str(), &sStat) != 0)
                        {
                            if (VSIMkdir(subdest.c_str(), 0755) != 0)
                            {
                                ReportError(m_skip ? CE_Warning : CE_Failure,
                                            CPLE_FileIO,
                                            "Cannot create directory %s",
                                            subdest.c_str());
                                if (!m_skip)
                                    return false;
                            }
                        }
                    }
                    curAmount += 1;

                    if (pfnProgress &&
                        !pfnProgress(
                            std::min(1.0, static_cast<double>(curAmount) /
                                              static_cast<double>(totalAmount)),
                            "", pProgressData))
                    {
                        return false;
                    }
                }
                else
                {
                    void *pScaledProgressData = GDALCreateScaledProgress(
                        static_cast<double>(curAmount) /
                            static_cast<double>(totalAmount),
                        std::min(1.0, static_cast<double>(curAmount +
                                                          entry->nSize + 1) /
                                          static_cast<double>(totalAmount)),
                        pfnProgress, pProgressData);
                    const bool bRet = CopySingle(
                        subsrc, dst, entry->nSize,
                        pScaledProgressData ? GDALScaledProgress : nullptr,
                        pScaledProgressData);
                    GDALDestroyScaledProgress(pScaledProgressData);

                    curAmount += entry->nSize + 1;

                    if (!bRet)
                        return false;
                }
            }
        }
    }
    else
    {
        ReportError(m_skip ? CE_Warning : CE_Failure, CPLE_AppDefined,
                    "%s is not a directory or cannot be opened", src.c_str());
        if (!m_skip)
            return false;
    }
    return true;
}

//! @endcond
