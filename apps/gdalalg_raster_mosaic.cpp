/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster mosaic" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_mosaic.h"

#include "cpl_conv.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterMosaicAlgorithm::GDALRasterMosaicAlgorithm()        */
/************************************************************************/

GDALRasterMosaicAlgorithm::GDALRasterMosaicAlgorithm(bool bStandalone)
    : GDALRasterMosaicStackCommonAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                           bStandalone)
{
    AddArg("add-alpha", 0,
           _("Adds an alpha mask band to the destination when the source "
             "raster have "
             "none."),
           &m_addAlpha);
    AddPixelFunctionNameArg(&m_pixelFunction);
    AddPixelFunctionArgsArg(&m_pixelFunctionArgs);
}

/************************************************************************/
/*                 GDALRasterMosaicAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterMosaicAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
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
    aosOptions.push_back("gdal raster mosaic");

    SetBuildVRTOptions(aosOptions);

    if (m_addAlpha)
    {
        aosOptions.push_back("-addalpha");
    }
    if (!m_pixelFunction.empty())
    {
        aosOptions.push_back("-pixel-function");
        aosOptions.push_back(m_pixelFunction);
    }

    for (const auto &arg : m_pixelFunctionArgs)
    {
        aosOptions.push_back("-pixel-function-arg");
        aosOptions.push_back(arg);
    }

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

GDALRasterMosaicAlgorithmStandalone::~GDALRasterMosaicAlgorithmStandalone() =
    default;

//! @endcond
