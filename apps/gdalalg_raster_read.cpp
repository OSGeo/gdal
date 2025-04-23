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

GDALRasterReadAlgorithm::GDALRasterReadAlgorithm()
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep =*/false)
{
    AddInputArgs(/* openForMixedRasterVector = */ false,
                 /* hiddenForCLI = */ false);
}

/************************************************************************/
/*                  GDALRasterReadAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterReadAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    m_outputDataset.Set(m_inputDataset.GetDatasetRef());

    return true;
}

//! @endcond
