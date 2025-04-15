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

#ifndef GDALALG_VSI_DELETE_INCLUDED
#define GDALALG_VSI_DELETE_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALVSIDeleteAlgorithm                        */
/************************************************************************/

class GDALVSIDeleteAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "delete";
    static constexpr const char *DESCRIPTION =
        "Delete files located on GDAL Virtual System Interface (VSI).";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_delete.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"rm", "rmdir", "del"};
    }

    GDALVSIDeleteAlgorithm();

  private:
    std::string m_filename{};
    bool m_recursive = false;

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
