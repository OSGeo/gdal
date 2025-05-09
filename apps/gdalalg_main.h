/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "main" command
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MAIN_INCLUDED
#define GDALALG_MAIN_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         GDALMainAlgorithm                            */
/************************************************************************/

class GDALMainAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME =
        GDALGlobalAlgorithmRegistry::ROOT_ALG_NAME;
    static constexpr const char *DESCRIPTION = "Main gdal entry point.";
    static constexpr const char *HELP_URL = "/programs/index.html";

    GDALMainAlgorithm();

    bool
    ParseCommandLineArguments(const std::vector<std::string> &args) override;

    std::string GetUsageForCLI(bool shortUsage,
                               const UsageOptions &usageOptions) const override;

  private:
    std::unique_ptr<GDALAlgorithm> m_subAlg{};
    std::string m_output{};
    bool m_showUsage = true;
    bool m_drivers = false;
    bool m_version = false;

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif  // GDALALG_MAIN_INCLUDED
