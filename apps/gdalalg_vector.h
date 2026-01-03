/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_VECTOR_INCLUDED
#define GDALALG_VECTOR_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalalgorithm.h"

/************************************************************************/
/*                         GDALVectorAlgorithm                          */
/************************************************************************/

class GDALVectorAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "vector";
    static constexpr const char *DESCRIPTION = "Vector commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_vector.html";

    GDALVectorAlgorithm();

  private:
    std::string m_output{};
    bool m_drivers = false;

    bool RunImpl(GDALProgressFunc, void *) override;
};

//! @endcond

#endif
