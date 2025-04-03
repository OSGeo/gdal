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

#ifndef GDALALG_RASTER_ASTYPE_INCLUDED
#define GDALALG_RASTER_ASTYPE_INCLUDED

#include "gdalalg_raster_pipeline.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                    GDALRasterAsTypeAlgorithm                         */
/************************************************************************/

class GDALRasterAsTypeAlgorithm /* non final */
    : public GDALRasterPipelineStepAlgorithm
{
  public:
    static constexpr const char *NAME = "astype";
    static constexpr const char *DESCRIPTION =
        "Modify the data type of bands of a raster dataset.";
    static constexpr const char *HELP_URL = "/programs/gdal_raster_astype.html";

    explicit GDALRasterAsTypeAlgorithm(bool standaloneStep = false);

  private:
    bool RunStep(GDALProgressFunc pfnProgress, void *pProgressData) override;

    std::string m_type{};
};

/************************************************************************/
/*                 GDALRasterAsTypeAlgorithmStandalone                  */
/************************************************************************/

class GDALRasterAsTypeAlgorithmStandalone final
    : public GDALRasterAsTypeAlgorithm
{
  public:
    GDALRasterAsTypeAlgorithmStandalone()
        : GDALRasterAsTypeAlgorithm(/* standaloneStep = */ true)
    {
    }
};

//! @endcond

#endif /* GDALALG_RASTER_ASTYPE_INCLUDED */
