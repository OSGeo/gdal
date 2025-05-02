/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vsi move" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VSI_MOVE_INCLUDED
#define GDALALG_VSI_MOVE_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                          GDALVSIMoveAlgorithm                        */
/************************************************************************/

class GDALVSIMoveAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "move";
    static constexpr const char *DESCRIPTION =
        "Move/rename a file/directory located on GDAL Virtual System Interface "
        "(VSI).";
    static constexpr const char *HELP_URL = "/programs/gdal_vsi_move.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"mv", "ren", "rename"};
    }

    GDALVSIMoveAlgorithm();

  private:
    std::string m_source{};
    std::string m_destination{};

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
