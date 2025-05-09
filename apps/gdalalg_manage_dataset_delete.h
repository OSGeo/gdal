/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "manage-dataset delete" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MANAGE_DATASET_DELETE_INCLUDED
#define GDALALG_MANAGE_DATASET_DELETE_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                GDALManageDatasetDeleteAlgorithm                      */
/************************************************************************/

class GDALManageDatasetDeleteAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "delete";
    static constexpr const char *DESCRIPTION = "Delete dataset(s).";
    static constexpr const char *HELP_URL =
        "/programs/gdal_manage_dataset_delete.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"rm", "remove"};
    }

    GDALManageDatasetDeleteAlgorithm();

  private:
    std::vector<std::string> m_filename{};
    std::string m_format{};

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
