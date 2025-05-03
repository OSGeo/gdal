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

#include "gdalalgorithm.h"

#include "gdalalg_mdim_info.h"
#include "gdalalg_mdim_convert.h"

#include "gdal_priv.h"

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                         GDALMdimAlgorithm                            */
/************************************************************************/

class GDALMdimAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "mdim";
    static constexpr const char *DESCRIPTION = "Multidimensional commands.";
    static constexpr const char *HELP_URL = "/programs/gdal_mdim.html";

    GDALMdimAlgorithm() : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
    {
        AddArg("drivers", 0,
               _("Display multidimensional driver list as JSON document"),
               &m_drivers);

        AddOutputStringArg(&m_output);

        RegisterSubAlgorithm<GDALMdimInfoAlgorithm>();
        RegisterSubAlgorithm<GDALMdimConvertAlgorithm>();
    }

  private:
    std::string m_output{};
    bool m_drivers = false;

    bool RunImpl(GDALProgressFunc, void *) override
    {
        if (m_drivers)
        {
            m_output = GDALPrintDriverList(GDAL_OF_MULTIDIM_RASTER, true);
            return true;
        }
        else
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "The Run() method should not be called directly on the \"gdal "
                "mdim\" program.");
            return false;
        }
    }
};

GDAL_STATIC_REGISTER_ALG(GDALMdimAlgorithm);
