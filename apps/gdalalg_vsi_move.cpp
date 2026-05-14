/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi move" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vsi_move.h"

#include "cpl_vsi.h"
#include "cpl_vsi_error.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*             GDALVSIMoveAlgorithm::GDALVSIMoveAlgorithm()             */
/************************************************************************/

GDALVSIMoveAlgorithm::GDALVSIMoveAlgorithm()
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
}

/************************************************************************/
/*                   GDALVSIMoveAlgorithm::RunImpl()                    */
/************************************************************************/

bool GDALVSIMoveAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                   void *pProgressData)
{
    if (VSIMove(m_source.c_str(), m_destination.c_str(), nullptr, pfnProgress,
                pProgressData) != 0)
    {
        VSIStatBufL statBufSrc;
        VSIErrorReset();
        const auto nOldErrorNum = VSIGetLastErrorNo();
        const bool srcExists = VSIStatL(m_source.c_str(), &statBufSrc) == 0;
        if (!srcExists)
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
            ReportError(CE_Failure, CPLE_FileIO, "%s could not be moved to %s",
                        m_source.c_str(), m_destination.c_str());
        }
        return false;
    }
    return true;
}

//! @endcond
