/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_MDIM_INCLUDED
#define GDALALG_MDIM_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"

/************************************************************************/
/*                         GDALMdimAlgorithm                            */
/************************************************************************/

class GDALMdimAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "mdim";
    static constexpr const char *DESCRIPTION = "Multidimensional commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim.html";

    GDALMdimAlgorithm();

  private:
    std::string m_output{};
    bool m_drivers = false;

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
