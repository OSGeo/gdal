/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "dataset check" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_DATASET_CHECK_INCLUDED
#define GDALALG_DATASET_CHECK_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

class GDALGroup;

/************************************************************************/
/*                     GDALDatasetCheckAlgorithm                        */
/************************************************************************/

class GDALDatasetCheckAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "check";
    static constexpr const char *DESCRIPTION =
        "Check whether there are errors when reading the content of a dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_dataset_check.html";

    GDALDatasetCheckAlgorithm();

  private:
    GDALArgDatasetValue m_input{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    int m_retCode = 0;

    bool CheckDataset(GDALDataset *poDS, bool bRasterOnly, GDALProgressFunc,
                      void *);
    bool CheckGroup(GDALGroup *poGroup, GIntBig &nProgress,
                    GIntBig nTotalContent, GDALProgressFunc, void *);
    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
