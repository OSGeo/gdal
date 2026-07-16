/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster stack" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_stack.h"
#include "gdalalg_raster_write.h"

#include "cpl_conv.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*         GDALRasterStackAlgorithm::GDALRasterStackAlgorithm()         */
/************************************************************************/

GDALRasterStackAlgorithm::GDALRasterStackAlgorithm(bool bStandalone)
    : GDALRasterMosaicStackCommonAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                           bStandalone)
{
}

/************************************************************************/
/*                 GDALRasterStackAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterStackAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    CPLAssert(!m_outputDataset.GetDatasetRef());

    std::vector<GDALDatasetH> ahInputDatasets;
    CPLStringList aosInputDatasetNames;
    bool foundByName = false;
    if (!GetInputDatasetNames(ctxt, ahInputDatasets, aosInputDatasetNames,
                              foundByName))
    {
        // Error message emitted by GetInputDatasetNames()
        return false;
    }

    CPLStringList aosOptions;

    aosOptions.push_back("-strict");

    aosOptions.push_back("-program_name");
    aosOptions.push_back("gdal raster stack");

    aosOptions.push_back("-separate");

    SetBuildVRTOptions(aosOptions);

    bool bOK = false;
    GDALBuildVRTOptions *psOptions =
        GDALBuildVRTOptionsNew(aosOptions.List(), nullptr);
    if (psOptions)
    {
        auto poOutDS =
            std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALBuildVRT(
                "",
                foundByName ? aosInputDatasetNames.size()
                            : static_cast<int>(m_inputDataset.size()),
                ahInputDatasets.empty() ? nullptr : ahInputDatasets.data(),
                aosInputDatasetNames.List(), psOptions, nullptr)));
        GDALBuildVRTOptionsFree(psOptions);
        bOK = poOutDS != nullptr;
        if (bOK)
        {
            m_outputDataset.Set(std::move(poOutDS));
        }
    }
    return bOK;
}

GDALRasterStackAlgorithmStandalone::~GDALRasterStackAlgorithmStandalone() =
    default;

//! @endcond
