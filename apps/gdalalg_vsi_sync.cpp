/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi sync" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vsi_sync.h"

#include "cpl_vsi.h"
#include "cpl_string.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*              GDALVSISyncAlgorithm::GDALVSISyncAlgorithm()            */
/************************************************************************/

GDALVSISyncAlgorithm::GDALVSISyncAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
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

    AddArg("recursive", 'r', _("Synchronize recursively"), &m_recursive);

    AddArg("strategy", 0, _("Synchronization strategy"), &m_strategy)
        .SetDefault(m_strategy)
        .SetChoices("timestamp", "ETag", "overwrite");

    AddNumThreadsArg(&m_numThreads, &m_numThreadsStr);
}

/************************************************************************/
/*                    GDALVSISyncAlgorithm::RunImpl()                   */
/************************************************************************/

bool GDALVSISyncAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                   void *pProgressData)
{
    CPLStringList aosOptions;
    aosOptions.SetNameValue("RECURSIVE", m_recursive ? "YES" : "NO");
    aosOptions.SetNameValue("STRATEGY", m_strategy.c_str());
    aosOptions.SetNameValue("NUM_THREADS", CPLSPrintf("%d", m_numThreads));

    if (!VSISync(m_source.c_str(), m_destination.c_str(), aosOptions.List(),
                 pfnProgress, pProgressData, nullptr))
    {
        VSIStatBufL sStat;
        if (VSIStatExL(m_source.c_str(), &sStat,
                       VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) != 0)
        {
            ReportError(CE_Failure, CPLE_FileIO, "%s does not exist",
                        m_source.c_str());
        }
        else
        {
            ReportError(CE_Failure, CPLE_FileIO,
                        "%s could not be synchronised with %s",
                        m_source.c_str(), m_destination.c_str());
        }
        return false;
    }
    return true;
}

//! @endcond
