/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Common code of "raster mosaic" and "raster stack"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_MOSAIC_STACK_COMMON_INCLUDED
#define GDALALG_RASTER_MOSAIC_STACK_COMMON_INCLUDED

#include "gdalalg_raster_pipeline.h"
#include "cpl_string.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                 GDALRasterMosaicStackCommonAlgorithm                 */
/************************************************************************/

class GDALRasterMosaicStackCommonAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    GDALRasterMosaicStackCommonAlgorithm(const std::string &name,
                                         const std::string &description,
                                         const std::string &helpURL,
                                         bool bStandalone);

    static ConstructorOptions GetConstructorOptions(bool standaloneStep);

  protected:
    bool RunImpl(GDALProgressFunc pfnProgress, void *pProgressData) override;

    bool GetInputDatasetNames(GDALPipelineStepRunContext &ctxt,
                              std::vector<GDALDatasetH> &ahInputDatasets,
                              CPLStringList &aosInputDatasetNames,
                              bool &foundByName);

    void SetBuildVRTOptions(CPLStringList &aosOptions);

    std::string m_resolution{};
    std::vector<double> m_bbox{};
    bool m_targetAlignedPixels = false;
    std::vector<double> m_srcNoData{};
    std::vector<double> m_dstNoData{};
    std::vector<int> m_bands{};
    bool m_hideNoData = false;
    bool m_writeAbsolutePaths = false;
};

//! @endcond

#endif
