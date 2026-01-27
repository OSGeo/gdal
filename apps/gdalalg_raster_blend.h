/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "blend" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_BLEND_INCLUDED
#define GDALALG_RASTER_BLEND_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                           CompositionMode                            */
/************************************************************************/

//! Blend composition modes (aka: operators)
enum class CompositionMode : unsigned
{
    SRC_OVER = 0,
    HSV_VALUE,
    MULTIPLY,
    SCREEN,
    OVERLAY,
    HARD_LIGHT,
    DARKEN,
    LIGHTEN,
    COLOR_DODGE,
    COLOR_BURN,
};

//! Returns a map of all composition modes to their string identifiers
std::map<CompositionMode, std::string> CompositionModes();

//! Returns the text identifier of the composition mode
std::string CompositionModeToString(CompositionMode mode);

//! Returns a list of all modes string identifiers
std::vector<std::string> CompositionModesIdentifiers();

//! Parses a composition mode from its string identifier
CompositionMode CompositionModeFromString(const std::string &str);

//! Returns the minimum number of bands required for the given composition mode
int MinBandCountForCompositionMode(CompositionMode mode);

/**
 *  Returns the maximum number of bands allowed for the given composition mode
 *  (-1 means no limit)
 */
int MaxBandCountForCompositionMode(CompositionMode mode);

//! Checks whether the number of bands is compatible with the given composition mode
bool BandCountIsCompatibleWithCompositionMode(int bandCount,
                                              CompositionMode mode);

/************************************************************************/
/*                       GDALRasterBlendAlgorithm                       */
/************************************************************************/

class GDALRasterBlendAlgorithm /* non final*/
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    explicit GDALRasterBlendAlgorithm(bool standaloneStep = false);

    static constexpr const char *NAME = "blend";
    static constexpr const char *DESCRIPTION =
        "Blend/compose two raster datasets";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_blend.html";

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool ValidateGlobal();

    GDALArgDatasetValue m_overlayDataset{};
    CompositionMode m_operator{};
    std::string m_operatorIdentifier{};
    static constexpr int OPACITY_INPUT_RANGE = 100;
    int m_opacity = OPACITY_INPUT_RANGE;
    std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> m_poTmpSrcDS{};
    std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser>
        m_poTmpOverlayDS{};
};

/************************************************************************/
/*                  GDALRasterBlendAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterBlendAlgorithmStandalone final : public GDALRasterBlendAlgorithm
{
  public:
    GDALRasterBlendAlgorithmStandalone()
        : GDALRasterBlendAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterBlendAlgorithmStandalone() override;
};

//! @endcond

#endif /* GDALALG_RASTER_COLOR_MERGE_INCLUDED */
