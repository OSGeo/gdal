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

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*              GDALVSIMoveAlgorithm::GDALVSIMoveAlgorithm()            */
/************************************************************************/

GDALVSIMoveAlgorithm::GDALVSIMoveAlgorithm()
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
}

/************************************************************************/
/*                    GDALVSIMoveAlgorithm::RunImpl()                   */
/************************************************************************/

bool GDALVSIMoveAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                   void *pProgressData)
{
    if (VSIMove(m_source.c_str(), m_destination.c_str(), nullptr, pfnProgress,
                pProgressData) != 0)
    {
        VSIStatBufL statBufSrc;
        const bool srcExists =
            VSIStatExL(m_source.c_str(), &statBufSrc,
                       VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0;
        if (!srcExists)
        {
            ReportError(CE_Failure, CPLE_FileIO, "%s does not exist",
                        m_source.c_str());
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
