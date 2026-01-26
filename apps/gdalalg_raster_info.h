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

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterInfoAlgorithm                        */
/************************************************************************/

class GDALRasterInfoAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "info";
    static constexpr const char *DESCRIPTION =
        "Return information on a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_info.html";

    explicit GDALRasterInfoAlgorithm(bool standaloneStep = false,
                                     bool openForMixedRasterVector = false);

    bool CanBeLastStep() const override
    {
        return true;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

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
    std::string m_mdd{};
    int m_subDS = 0;
};

/************************************************************************/
/*                  GDALRasterInfoAlgorithmStandalone                   */
/************************************************************************/

class GDALRasterInfoAlgorithmStandalone final : public GDALRasterInfoAlgorithm
{
  public:
    GDALRasterInfoAlgorithmStandalone()
        : GDALRasterInfoAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterInfoAlgorithmStandalone() override;
};

//! @endcond

#endif
