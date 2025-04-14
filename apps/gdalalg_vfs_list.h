/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vfs list" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VFS_LIST_INCLUDED
#define GDALALG_VFS_LIST_INCLUDED

#include "gdalalgorithm.h"

#include "cpl_json_streaming_writer.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                          GDALVFSListAlgorithm                        */
/************************************************************************/

struct VSIDIREntry;

class GDALVFSListAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "list";
    static constexpr const char *DESCRIPTION =
        "List files of one of the GDAL Virtual file systems (VSI).";
    static constexpr const char *HELP_URL = "/programs/gdal_vfs_list.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"ls"};
    }

    GDALVFSListAlgorithm();

  private:
    CPLJSonStreamingWriter m_oWriter;
    std::string m_filename{};
    std::string m_format{};
    std::string m_output{};
    int m_depth = -1;
    bool m_stdout = false;
    bool m_longListing = false;
    bool m_recursive = false;
    bool m_JSONAsTree = false;
    bool m_absolutePath = false;

    std::vector<std::string> m_stackNames{};

    bool RunImpl(GDALProgressFunc, void *) override;
    void Print(const char *str);
    void PrintEntry(const VSIDIREntry *entry);
    static void JSONPrint(const char *pszTxt, void *pUserData);
};

//! @endcond

#endif
