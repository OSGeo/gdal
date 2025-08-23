/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "edit" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_EDIT_INCLUDED
#define GDALALG_RASTER_EDIT_INCLUDED

#include "gdalalg_raster_pipeline.h"

namespace gdal
{
class GCP;
}

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       GDALRasterEditAlgorithm                        */
/************************************************************************/

class GDALRasterEditAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "edit";
    static constexpr const char *DESCRIPTION = "Edit a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_edit.html";

    explicit GDALRasterEditAlgorithm(bool standaloneStep = false);
    ~GDALRasterEditAlgorithm() override;

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    std::vector<gdal::GCP> ParseGCPs() const;

    GDALArgDatasetValue m_dataset{};  // standalone mode only
    bool m_readOnly = false;          // standalone mode only
    std::string m_overrideCrs{};
    std::vector<double> m_bbox{};
    std::vector<std::string> m_metadata{};
    std::vector<std::string> m_unsetMetadata{};
    std::vector<std::string> m_unsetMetadataDomain{};
    std::string m_nodata{};
    std::vector<std::string> m_gcps{};
    bool m_stats = false;        // standalone mode only
    bool m_approxStats = false;  // standalone mode only
    bool m_hist = false;         // standalone mode only
};

/************************************************************************/
/*                     GDALRasterEditAlgorithmStandalone                */
/************************************************************************/

class GDALRasterEditAlgorithmStandalone final : public GDALRasterEditAlgorithm
{
  public:
    GDALRasterEditAlgorithmStandalone()
        : GDALRasterEditAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterEditAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_EDIT_INCLUDED */
