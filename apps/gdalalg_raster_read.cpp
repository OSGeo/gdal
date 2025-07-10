/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "read" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_read.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*          GDALRasterReadAlgorithm::GDALRasterReadAlgorithm()          */
/************************************************************************/

GDALRasterReadAlgorithm::GDALRasterReadAlgorithm(bool openForMixedRasterVector)
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions().SetAddDefaultArguments(false))
{
    AddRasterInputArgs(openForMixedRasterVector,
                       /* hiddenForCLI = */ false);
}

/************************************************************************/
/*                  GDALRasterReadAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterReadAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    const auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    m_outputDataset.Set(poSrcDS);

    return true;
}

//! @endcond
