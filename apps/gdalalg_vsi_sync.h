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

#ifndef GDALALG_VSI_SYNC_INCLUDED
#define GDALALG_VSI_SYNC_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                          GDALVSISyncAlgorithm                        */
/************************************************************************/

class GDALVSISyncAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "sync";
    static constexpr const char *DESCRIPTION =
        "Synchronize source and target file/directory located on GDAL Virtual "
        "System Interface (VSI).";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_sync.html";

    GDALVSISyncAlgorithm();

  private:
    std::string m_source{};
    std::string m_destination{};
    bool m_recursive = false;
    std::string m_strategy = "timestamp";
    int m_numThreads = 0;

    // Work variables
    std::string m_numThreadsStr{};

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
