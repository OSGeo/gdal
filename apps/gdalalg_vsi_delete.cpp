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
#include "cpl_vsi_error.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*           GDALVSIDeleteAlgorithm::GDALVSIDeleteAlgorithm()           */
/************************************************************************/

GDALVSIDeleteAlgorithm::GDALVSIDeleteAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    {
        auto &arg = AddArg("filename", 0, _("File or directory name to delete"),
                           &m_filename)
                        .SetPositional()
                        .SetMinCharCount(1)
                        .SetRequired();
        SetAutoCompleteFunctionForFilename(arg, 0);
    }

    AddArg("recursive", 'r', _("Delete directories recursively"), &m_recursive)
        .AddShortNameAlias('R');
}

/************************************************************************/
/*                  GDALVSIDeleteAlgorithm::RunImpl()                   */
/************************************************************************/

bool GDALVSIDeleteAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    bool ret = false;
    VSIStatBufL sStat;
    VSIErrorReset();
    const auto nOldErrorNum = VSIGetLastErrorNo();
    if (VSIStatL(m_filename.c_str(), &sStat) != 0)
    {
        if (nOldErrorNum != VSIGetLastErrorNo())
        {
            ReportError(CE_Failure, CPLE_FileIO,
                        "'%s' cannot be accessed. %s: %s", m_filename.c_str(),
                        VSIErrorNumToString(VSIGetLastErrorNo()),
                        VSIGetLastErrorMsg());
        }
        else
        {
            ReportError(CE_Failure, CPLE_FileIO,
                        "'%s' does not exist or cannot be accessed",
                        m_filename.c_str());
        }
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
