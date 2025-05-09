/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "manage-dataset copy" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MANAGE_DATASET_COPY_INCLUDED
#define GDALALG_MANAGE_DATASET_COPY_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*            GDALManageDatasetCopyRenameCommonAlgorithm                */
/************************************************************************/

class GDALManageDatasetCopyRenameCommonAlgorithm /* non-final */
    : public GDALAlgorithm
{
  protected:
    GDALManageDatasetCopyRenameCommonAlgorithm(const std::string &name,
                                               const std::string &description,
                                               const std::string &helpURL);

  private:
    std::string m_source{};
    std::string m_destination{};
    std::string m_format{};
    bool m_overwrite = false;

    bool RunImpl(GDALProgressFunc, void *) override;
};

/************************************************************************/
/*                  GDALManageDatasetCopyAlgorithm                      */
/************************************************************************/

class GDALManageDatasetCopyAlgorithm final
    : public GDALManageDatasetCopyRenameCommonAlgorithm
{
  public:
    static constexpr const char *NAME = "copy";
    static constexpr const char *DESCRIPTION = "Copy files of a dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_manage_dataset_copy.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"cp"};
    }

    GDALManageDatasetCopyAlgorithm();
};

//! @endcond

#endif
