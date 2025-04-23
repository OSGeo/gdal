/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi list" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vsi_list.h"

#include "cpl_string.h"
#include "cpl_time.h"
#include "cpl_vsi.h"

#include <cinttypes>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*              GDALVSIListAlgorithm::GDALVSIListAlgorithm()            */
/************************************************************************/

GDALVSIListAlgorithm::GDALVSIListAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL), m_oWriter(JSONPrint, this)
{
    auto &arg = AddArg("filename", 0, _("File or directory name"), &m_filename)
                    .SetPositional()
                    .SetRequired();
    SetAutoCompleteFunctionForFilename(arg, 0);

    AddOutputFormatArg(&m_format).SetDefault("json").SetChoices("json", "text");

    AddArg("long-listing", 'l', _("Use a long listing format"), &m_longListing)
        .AddAlias("long");
    AddArg("recursive", 'R', _("List subdirectories recursively"),
           &m_recursive);
    AddArg("depth", 0, _("Maximum depth in recursive mode"), &m_depth)
        .SetMinValueIncluded(1);
    AddArg("absolute-path", 0, _("Display absolute path"), &m_absolutePath)
        .AddAlias("abs");
    AddArg("tree", 0, _("Use a hierarchical presentation for JSON output"),
           &m_JSONAsTree);

    AddOutputStringArg(&m_output);
    AddArg(
        "stdout", 0,
        _("Directly output on stdout. If enabled, output-string will be empty"),
        &m_stdout)
        .SetHiddenForCLI();
}

/************************************************************************/
/*                   GDALVSIListAlgorithm::Print()                      */
/************************************************************************/

void GDALVSIListAlgorithm::Print(const char *str)
{
    if (m_stdout)
        fwrite(str, 1, strlen(str), stdout);
    else
        m_output += str;
}

/************************************************************************/
/*                  GDALVSIListAlgorithm::JSONPrint()                   */
/************************************************************************/

/* static */ void GDALVSIListAlgorithm::JSONPrint(const char *pszTxt,
                                                  void *pUserData)
{
    static_cast<GDALVSIListAlgorithm *>(pUserData)->Print(pszTxt);
}

/************************************************************************/
/*                            GetDepth()                                */
/************************************************************************/

static int GetDepth(const std::string &filename)
{
    int depth = 0;
    const char sep = VSIGetDirectorySeparator(filename.c_str())[0];
    for (size_t i = 0; i < filename.size(); ++i)
    {
        if ((filename[i] == sep || filename[i] == '/') &&
            i != filename.size() - 1)
            ++depth;
    }
    return depth;
}

/************************************************************************/
/*                 GDALVSIListAlgorithm::PrintEntry()                   */
/************************************************************************/

void GDALVSIListAlgorithm::PrintEntry(const VSIDIREntry *entry)
{
    std::string filename;
    if (m_format == "json" && m_JSONAsTree)
    {
        filename = CPLGetFilename(entry->pszName);
    }
    else if (m_absolutePath)
    {
        if (CPLIsFilenameRelative(m_filename.c_str()))
        {
            char *pszCurDir = CPLGetCurrentDir();
            if (!pszCurDir)
                pszCurDir = CPLStrdup(".");
            if (m_filename == ".")
                filename = pszCurDir;
            else
                filename =
                    CPLFormFilenameSafe(pszCurDir, m_filename.c_str(), nullptr);
            CPLFree(pszCurDir);
        }
        else
        {
            filename = m_filename;
        }
        filename =
            CPLFormFilenameSafe(filename.c_str(), entry->pszName, nullptr);
    }
    else
    {
        filename = entry->pszName;
    }

    char permissions[1 + 3 + 3 + 3 + 1] = "----------";
    struct tm bdt;
    memset(&bdt, 0, sizeof(bdt));

    if (m_longListing)
    {
        if (entry->bModeKnown)
        {
            if (VSI_ISDIR(entry->nMode))
                permissions[0] = 'd';
            for (int i = 0; i < 9; ++i)
            {
                if (entry->nMode & (1 << i))
                    permissions[9 - i] = (i % 3) == 0   ? 'x'
                                         : (i % 3) == 1 ? 'w'
                                                        : 'r';
            }
        }
        else if (VSI_ISDIR(entry->nMode))
        {
            strcpy(permissions, "dr-xr-xr-x");
        }
        else
        {
            strcpy(permissions, "-r--r--r--");
        }

        CPLUnixTimeToYMDHMS(entry->nMTime, &bdt);
    }

    if (m_format == "json")
    {
        if (m_JSONAsTree)
        {
            while (!m_stackNames.empty() &&
                   GetDepth(m_stackNames.back()) >= GetDepth(entry->pszName))
            {
                m_oWriter.EndArray();
                m_oWriter.EndObj();
                m_stackNames.pop_back();
            }
        }

        if (m_longListing)
        {
            m_oWriter.StartObj();
            m_oWriter.AddObjKey("name");
            m_oWriter.Add(filename);
            m_oWriter.AddObjKey("type");
            m_oWriter.Add(VSI_ISDIR(entry->nMode) ? "directory" : "file");
            m_oWriter.AddObjKey("size");
            m_oWriter.Add(static_cast<uint64_t>(entry->nSize));
            if (entry->bMTimeKnown)
            {
                m_oWriter.AddObjKey("last_modification_date");
                m_oWriter.Add(CPLSPrintf("%04d-%02d-%02d %02d:%02d:%02dZ",
                                         bdt.tm_year + 1900, bdt.tm_mon + 1,
                                         bdt.tm_mday, bdt.tm_hour, bdt.tm_min,
                                         bdt.tm_sec));
            }
            if (entry->bModeKnown)
            {
                m_oWriter.AddObjKey("permissions");
                m_oWriter.Add(permissions);
            }
            if (m_JSONAsTree && VSI_ISDIR(entry->nMode))
            {
                m_stackNames.push_back(entry->pszName);
                m_oWriter.AddObjKey("entries");
                m_oWriter.StartArray();
            }
            else
            {
                m_oWriter.EndObj();
            }
        }
        else
        {
            if (m_JSONAsTree && VSI_ISDIR(entry->nMode))
            {
                m_oWriter.StartObj();
                m_oWriter.AddObjKey("name");
                m_oWriter.Add(filename);

                m_stackNames.push_back(entry->pszName);
                m_oWriter.AddObjKey("entries");
                m_oWriter.StartArray();
            }
            else
            {
                m_oWriter.Add(filename);
            }
        }
    }
    else if (m_longListing)
    {
        Print(CPLSPrintf("%s 1 unknown unknown %12" PRIu64
                         " %04d-%02d-%02d %02d:%02d %s\n",
                         permissions, static_cast<uint64_t>(entry->nSize),
                         bdt.tm_year + 1900, bdt.tm_mon + 1, bdt.tm_mday,
                         bdt.tm_hour, bdt.tm_min, filename.c_str()));
    }
    else
    {
        Print(filename.c_str());
        Print("\n");
    }
}

/************************************************************************/
/*                    GDALVSIListAlgorithm::RunImpl()                   */
/************************************************************************/

bool GDALVSIListAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    VSIStatBufL sStat;
    if (VSIStatL(m_filename.c_str(), &sStat) != 0)
    {
        ReportError(CE_Failure, CPLE_FileIO, "'%s' does not exist",
                    m_filename.c_str());
        return false;
    }

    bool ret = false;
    if (VSI_ISDIR(sStat.st_mode))
    {
        std::unique_ptr<VSIDIR, decltype(&VSICloseDir)> dir(
            VSIOpenDir(m_filename.c_str(),
                       m_recursive ? (m_depth == 0  ? 0
                                      : m_depth > 0 ? m_depth - 1
                                                    : -1)
                                   : 0,
                       nullptr),
            VSICloseDir);
        if (dir)
        {
            ret = true;
            if (m_format == "json")
                m_oWriter.StartArray();
            while (const auto entry = VSIGetNextDirEntry(dir.get()))
            {
                if (!(entry->pszName[0] == '.' &&
                      (entry->pszName[1] == '.' || entry->pszName[1] == 0)))
                {
                    PrintEntry(entry);
                }
            }
            while (!m_stackNames.empty())
            {
                m_stackNames.pop_back();
                m_oWriter.EndArray();
                m_oWriter.EndObj();
            }
            if (m_format == "json")
                m_oWriter.EndArray();
        }
    }
    else
    {
        ret = true;
        VSIDIREntry sEntry;
        sEntry.pszName = CPLStrdup(m_filename.c_str());
        sEntry.bModeKnown = true;
        sEntry.nMode = sStat.st_mode;
        sEntry.nSize = sStat.st_size;
        PrintEntry(&sEntry);
    }

    return ret;
}

//! @endcond
