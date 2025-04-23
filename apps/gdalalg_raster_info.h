/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_INFO_INCLUDED
#define GDALALG_RASTER_INFO_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterInfoAlgorithm                        */
/************************************************************************/

class GDALRasterInfoAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_info.html";

    explicit GDALRasterInfoAlgorithm(bool openForMixedRasterVector = false);

    GDALDataset *GetDatasetRef()
    {
        return m_dataset.GetDatasetRef();
    }

    void SetDataset(GDALDataset *poDS)
    {
        auto arg = GetArg(GDAL_ARG_NAME_INPUT);
        if (arg)
        {
            arg->Set(poDS);
            arg->SetSkipIfAlreadySet();
        }
    }

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_format{};
    bool m_minMax = false;
    bool m_stats = false;
    bool m_approxStats = false;
    bool m_hist = false;
    bool m_noGCP = false;
    bool m_noMD = false;
    bool m_noCT = false;
    bool m_noFL = false;
    bool m_noMask = false;
    bool m_noNodata = false;
    bool m_checksum = false;
    bool m_listMDD = false;
    bool m_stdout = false;
    std::string m_mdd{};
    int m_subDS = 0;
    GDALArgDatasetValue m_dataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    std::string m_output{};
};

//! @endcond

#endif
