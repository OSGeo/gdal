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

#include "gdalalg_raster_convert.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*       GDALRasterConvertAlgorithm::GDALRasterConvertAlgorithm()       */
/************************************************************************/

GDALRasterConvertAlgorithm::GDALRasterConvertAlgorithm(
    bool /* standalone */, bool openForMixedRasterVector)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(true)
                                          .SetInputDatasetMaxCount(1)
                                          .SetAddDefaultArguments(false))
{
    AddRasterInputArgs(openForMixedRasterVector, false);
    AddProgressArg();
    AddRasterOutputArgs(false);
}

/************************************************************************/
/*                GDALRasterConvertAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALRasterConvertAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    // Do nothing but forwarding the input dataset to the output. Real job
    // is done by GDALVectorWrite.
    CPLAssert(m_inputDataset.size() == 1);
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    m_outputDataset.Set(poSrcDS);

    return true;
}

//! @endcond
