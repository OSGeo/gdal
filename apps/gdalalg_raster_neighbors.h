/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "neighbors" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_NEIGHBORS_INCLUDED
#define GDALALG_RASTER_NEIGHBORS_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterNeighborsAlgorithm                     */
/************************************************************************/

class GDALRasterNeighborsAlgorithm : public GDALRasterPipelineStepAlgorithm
{
  public:
    explicit GDALRasterNeighborsAlgorithm(bool standaloneStep = false) noexcept;

    static constexpr const char *NAME = "neighbors";
    static constexpr const char *DESCRIPTION =
        "Compute the value of each pixel from its neighbors (focal statistics)";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_neighbors.html";

    static std::vector<std::string> GetAliasesStatic()
    {
        return {"neighbours"};
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;

    int m_band = 0;
    std::vector<std::string> m_method{};
    int m_size = 0;
    std::vector<std::string> m_kernel{};
    std::string m_type{};
    std::string m_nodata{};
};

/************************************************************************/
/*                GDALRasterNeighborsAlgorithmStandalone                */
/************************************************************************/

class GDALRasterNeighborsAlgorithmStandalone final
    : public GDALRasterNeighborsAlgorithm
{
  public:
    GDALRasterNeighborsAlgorithmStandalone()
        : GDALRasterNeighborsAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterNeighborsAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_CALC_INCLUDED */
