/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster pixelinfo" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_PIXEL_INFO_INCLUDED
#define GDALALG_RASTER_PIXEL_INFO_INCLUDED

#include "gdalalg_abstract_pipeline.h"

#include "cpl_vsi_virtual.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                     GDALRasterPixelInfoAlgorithm                     */
/************************************************************************/

class GDALRasterPixelInfoAlgorithm /* non final */
    : public GDALPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "pixel-info";
    static constexpr const char *DESCRIPTION =
        "Return information on a pixel of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_pixel_info.html";

    explicit GDALRasterPixelInfoAlgorithm(bool standaloneStep = false);
    ~GDALRasterPixelInfoAlgorithm() override;

    bool IsNativelyStreamingCompatible() const override
    {
        // It could potentially be made fully streamable in pipeline mode since
        // we read coordinates from an input vector dataset. "Just" needs some
        // code reorganization.
        return false;
    }

    int GetInputType() const override
    {
        return GDAL_OF_RASTER;
    }

    int GetOutputType() const override
    {
        return GDAL_OF_VECTOR;
    }

  private:
    bool RunStep(GDALPipelineStepRunContext &ctxt) override;
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    GDALArgDatasetValue m_vectorDataset{};
    std::vector<std::string> m_includeFields{"ALL"};

    std::vector<int> m_band{};
    int m_overview = -1;
    std::vector<double> m_pos{};
    std::string m_posCrs{};
    std::string m_resampling = "nearest";
    bool m_promotePixelValueToZ = false;

    VSIVirtualHandleUniquePtr m_outputFile{};
    std::string m_osTmpFilename{};
};

/************************************************************************/
/*                GDALRasterPixelInfoAlgorithmStandalone                */
/************************************************************************/

class GDALRasterPixelInfoAlgorithmStandalone final
    : public GDALRasterPixelInfoAlgorithm
{
  public:
    GDALRasterPixelInfoAlgorithmStandalone()
        : GDALRasterPixelInfoAlgorithm(/* standaloneStep = */ true)
    {
    }

    ~GDALRasterPixelInfoAlgorithmStandalone() override;
};

//! @endcond

#endif
