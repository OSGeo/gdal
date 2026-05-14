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

#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_error.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*             GDALVSISyncAlgorithm::GDALVSISyncAlgorithm()             */
/************************************************************************/

GDALVSISyncAlgorithm::GDALVSISyncAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    {
        auto &arg =
            AddArg("source", 0, _("Source file or directory name"), &m_source)
                .SetPositional()
                .SetMinCharCount(1)
                .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
    }
    {
        auto &arg =
            AddArg("destination", 0, _("Destination file or directory name"),
                   &m_destination)
                .SetPositional()
                .SetMinCharCount(1)
                .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
    }

    AddArg("recursive", 'r', _("Synchronize recursively"), &m_recursive);

    AddArg("strategy", 0, _("Synchronization strategy"), &m_strategy)
        .SetDefault(m_strategy)
        .SetChoices("timestamp", "ETag", "overwrite");

    AddNumThreadsArg(&m_numThreads, &m_numThreadsStr);
}

/************************************************************************/
/*                   GDALVSISyncAlgorithm::RunImpl()                    */
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
        VSIErrorReset();
        const auto nOldErrorNum = VSIGetLastErrorNo();
        if (VSIStatL(m_source.c_str(), &sStat) != 0)
        {
            if (nOldErrorNum != VSIGetLastErrorNo())
            {
                ReportError(CE_Failure, CPLE_FileIO,
                            "'%s' cannot be accessed. %s: %s", m_source.c_str(),
                            VSIErrorNumToString(VSIGetLastErrorNo()),
                            VSIGetLastErrorMsg());
            }
            else
            {
                ReportError(CE_Failure, CPLE_FileIO,
                            "'%s' does not exist or cannot be accessed",
                            m_source.c_str());
            }
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
