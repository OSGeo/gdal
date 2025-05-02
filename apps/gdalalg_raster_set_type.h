/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "astype" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALALG_RASTER_SET_TYPE_INCLUDED
#define GDALALG_RASTER_SET_TYPE_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   GDALRasterSetTypeAlgorithm                         */
/************************************************************************/

class GDALRasterSetTypeAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "set-type";
    static constexpr const char *DESCRIPTION =
        "Modify the data type of bands of a raster dataset.";
    static constexpr const char *HELP_URL =
        "/programs/gdal_raster_set_type.html";

    explicit GDALRasterSetTypeAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_type{};
};

/************************************************************************/
/*                GDALRasterSetTypeAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterSetTypeAlgorithmStandalone final
    : public GDALRasterSetTypeAlgorithm
{
  public:
    GDALRasterSetTypeAlgorithmStandalone()
        : GDALRasterSetTypeAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_SET_TYPE_INCLUDED */
