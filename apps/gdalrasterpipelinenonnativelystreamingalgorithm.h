/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster/vector pipeline" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALRASTERPIPELINENGNNATIVELYSTREAMINGALGORITHM_INCLUDED
#define GDALRASTERPIPELINENGNNATIVELYSTREAMINGALGORITHM_INCLUDED

//! @cond Doxygen_Suppress

#include "gdalrasterpipelinestepalgorithm.h"

/************************************************************************/
/*           GDALRasterPipelineNonNativelyStreamingAlgorithm            */
/************************************************************************/

class GDALRasterPipelineNonNativelyStreamingAlgorithm /* non-final */
    : public GDALRasterPipelineStepAlgorithm
{
  protected:
    GDALRasterPipelineNonNativelyStreamingAlgorithm(
        const std::string &name, const std::string &description,
        const std::string &helpURL, bool standaloneStep);

    bool IsNativelyStreamingCompatible() const override;

    static std::unique_ptr<GDALDataset>
    CreateTemporaryDataset(int nWidth, int nHeight, int nBands,
                           GDALDataType eDT, bool bTiledIfPossible,
                           GDALDataset *poSrcDSForMetadata,
                           bool bCopyMetadata = true);
    static std::unique_ptr<GDALDataset>
    CreateTemporaryCopy(GDALAlgorithm *poAlg, GDALDataset *poSrcDS,
                        int nSingleBand, bool bTiledIfPossible,
                        GDALProgressFunc pfnProgress, void *pProgressData);
};

//! @endcond

#endif
