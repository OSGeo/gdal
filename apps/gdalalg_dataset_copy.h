/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset copy" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_DATASET_COPY_INCLUDED
#define GDALALG_DATASET_COPY_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALDatasetCopyRenameCommonAlgorithm                 */
/************************************************************************/

class GDALDatasetCopyRenameCommonAlgorithm /* non-final */
    : public GDALAlgorithm
{
  protected:
    GDALDatasetCopyRenameCommonAlgorithm(const std::string &name,
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
/*                       GDALDatasetCopyAlgorithm                       */
/************************************************************************/

class GDALDatasetCopyAlgorithm final
    : public GDALDatasetCopyRenameCommonAlgorithm
{
  public:
    static constexpr const char *NAME = "copy";
    static constexpr const char *DESCRIPTION = "Copy files of a dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_dataset_copy.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"cp"};
    }

    GDALDatasetCopyAlgorithm();
    ~GDALDatasetCopyAlgorithm() override;
};

//! @endcond

#endif
