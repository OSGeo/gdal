/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_CONVERT_INCLUDED
#define GDALALG_RASTER_CONVERT_INCLUDED

#include "gdalalgorithm.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterConvertAlgorithm                       */
/************************************************************************/

class GDALRasterConvertAlgorithm final : public GDALAlgorithm
{
  public:
    static constexpr const char *NAME = "convert";
    static constexpr const char *DESCRIPTION = "Convert a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_convert.html";

    explicit GDALRasterConvertAlgorithm(bool openForMixedRasterVector = false);

    GDALDataset *GetDatasetRef()
    {
        return m_inputDataset.GetDatasetRef();
    }

    void SetDataset(GDALDataset *poDS)
    {
        auto arg = GetArg(GDAL_ARG_NAME_INPUT);
        arg->Set(poDS);
        arg->SetSkipIfAlreadySet();
    }

  private:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_outputFormat{};
    GDALArgDatasetValue m_inputDataset{};
    std::vector<std::string> m_openOptions{};
    std::vector<std::string> m_inputFormats{};
    GDALArgDatasetValue m_outputDataset{};
    std::vector<std::string> m_creationOptions{};
    bool m_overwrite = false;
    bool m_append = false;
};

//! @endcond

#endif
