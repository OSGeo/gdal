/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "read" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_read.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALVectorReadAlgorithm::GDALVectorReadAlgorithm()          */
/************************************************************************/

GDALVectorReadAlgorithm::GDALVectorReadAlgorithm()
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep =*/false)
{
    AddInputArgs(/* hiddenForCLI = */ false);
}

/************************************************************************/
/*                  GDALVectorReadAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALVectorReadAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_inputLayerNames.empty())
    {
        m_outputDataset.Set(m_inputDataset.GetDatasetRef());
    }
    else
    {
        auto poSrcDS = m_inputDataset.GetDatasetRef();
        auto poOutDS = std::make_unique<GDALVectorPipelineOutputDataset>();
        poOutDS->SetDescription(poSrcDS->GetDescription());
        for (const auto &srcLayerName : m_inputLayerNames)
        {
            auto poSrcLayer = poSrcDS->GetLayerByName(srcLayerName.c_str());
            if (!poSrcLayer)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot find source layer '%s'",
                            srcLayerName.c_str());
                return false;
            }
            poOutDS->AddLayer(poSrcLayer);
        }
        m_outputDataset.Set(std::move(poOutDS));
    }
    return true;
}

//! @endcond
