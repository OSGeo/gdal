/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset identify" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MANAGE_IDENTIFY_INCLUDED
#define GDALALG_MANAGE_IDENTIFY_INCLUDED

#include "gdalalgorithm.h"

#include "cpl_json_streaming_writer.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALDatasetIdentifyAlgorithm                       */
/************************************************************************/

class GDALDatasetIdentifyAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "identify";
    static constexpr const char *DESCRIPTION =
        "Identify driver opening dataset(s).";
    static constexpr const char *HELP_URL =
        "/programs/gdal_dataset_identify.html";

    GDALDatasetIdentifyAlgorithm();

  private:
    std::vector<std::string> m_filename{};
    std::string m_format{};
    CPLJSonStreamingWriter m_oWriter;
    std::string m_output{};
    bool m_recursive = false;
    bool m_forceRecursive = false;
    bool m_reportFailures = false;
    bool m_stdout = false;

    bool RunImpl(GDALProgressFunc, void *) override;
    void Print(const char *str);
    bool Process(const char *pszTarget, CSLConstList papszSiblingList,
                 GDALProgressFunc pfnProgress, void *pProgressData);
    static void JSONPrint(const char *pszTxt, void *pUserData);
};

//! @endcond

#endif
