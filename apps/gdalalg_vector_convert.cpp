/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "vector convert" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_convert.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*       GDALVectorConvertAlgorithm::GDALVectorConvertAlgorithm()       */
/************************************************************************/

GDALVectorConvertAlgorithm::GDALVectorConvertAlgorithm(
    bool /* standaloneStep */)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /*standaloneStep = */ true)
{
}

/************************************************************************/
/*                GDALVectorConvertAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALVectorConvertAlgorithm::RunStep(GDALPipelineStepRunContext &)
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
