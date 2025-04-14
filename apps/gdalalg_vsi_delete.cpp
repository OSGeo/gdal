/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi delete" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Deleteright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vsi_delete.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*            GDALVSIDeleteAlgorithm::GDALVSIDeleteAlgorithm()          */
/************************************************************************/

GDALVSIDeleteAlgorithm::GDALVSIDeleteAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    {
        auto &arg = AddArg("filename", 0, _("File or directory name to delete"),
                           &m_filename)
                        .SetPositional()
                        .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
        arg.AddValidationAction(
            [this]()
            {
                if (m_filename.empty())
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Filename cannot be empty");
                    return false;
                }
                return true;
            });
    }

    AddArg("recursive", 'r', _("Delete directories recursively"), &m_recursive)
        .AddShortNameAlias('R');
}

/************************************************************************/
/*                    GDALVSIDeleteAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALVSIDeleteAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    bool ret = false;
    VSIStatBufL sStat;
    if (VSIStatL(m_filename.c_str(), &sStat) != 0)
    {
        ReportError(CE_Failure, CPLE_FileIO, "%s does not exist",
                    m_filename.c_str());
    }
    else
    {
        if (m_recursive)
        {
            ret = VSIRmdirRecursive(m_filename.c_str()) == 0;
        }
        else
        {
            ret = VSI_ISDIR(sStat.st_mode) ? VSIRmdir(m_filename.c_str()) == 0
                                           : VSIUnlink(m_filename.c_str()) == 0;
        }
        if (!ret)
        {
            ReportError(CE_Failure, CPLE_FileIO, "Cannot delete %s",
                        m_filename.c_str());
        }
    }
    return ret;
}

//! @endcond
