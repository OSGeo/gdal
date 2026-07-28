/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "read" step of "mdim pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_read.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*            GDALMdimReadAlgorithm::GDALMdimReadAlgorithm()            */
/************************************************************************/

GDALMdimReadAlgorithm::GDALMdimReadAlgorithm(bool openForMixedMdimVector)
    : GDALMdimPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions().SetAddDefaultArguments(false))
{
    AddMdimInputArgs(openForMixedMdimVector,
                     /* hiddenForCLI = */ false, /* acceptRaster = */ false);
}

/************************************************************************/
/*                   GDALMdimReadAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALMdimReadAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    const auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    m_outputDataset.Set(poSrcDS);

    return true;
}

//! @endcond
