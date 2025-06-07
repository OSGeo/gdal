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
#include "vrtdataset.h"

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

    const auto pixelFunctionNames =
        VRTDerivedRasterBand::GetPixelFunctionNames();
    AddArg("pixel-function", 0,
           _("Specify a pixel function to calculate output value from "
             "overlapping inputs"),
           &m_pixelFunction)
        .SetChoices(pixelFunctionNames);

    auto &pixelFunctionArgArg =
        AddArg("pixel-function-arg", 0,
               _("Specify argument(s) to pass to the pixel function"),
               &m_pixelFunctionArgs)
            .SetMetaVar("<NAME>=<VALUE>")
            .SetRepeatedArgAllowed(true);
    pixelFunctionArgArg.AddValidationAction(
        [this, &pixelFunctionArgArg]()
        { return ParseAndValidateKeyValue(pixelFunctionArgArg); });

    pixelFunctionArgArg.SetAutoCompleteFunction(
        [this](const std::string &currentValue)
        {
            std::vector<std::string> ret;

            if (!m_pixelFunction.empty())
            {
                const auto *pair = VRTDerivedRasterBand::GetPixelFunction(
                    m_pixelFunction.c_str());
                if (!pair)
                {
                    ret.push_back("**");
                    // Non printable UTF-8 space, to avoid autocompletion to pickup on 'd'
                    ret.push_back(std::string("\xC2\xA0"
                                              "Invalid pixel function name"));
                }
                else if (pair->second.find("Argument name=") ==
                         std::string::npos)
                {
                    ret.push_back("**");
                    // Non printable UTF-8 space, to avoid autocompletion to pickup on 'd'
                    ret.push_back(
                        std::string(
                            "\xC2\xA0"
                            "No pixel function arguments for pixel function '")
                            .append(m_pixelFunction)
                            .append("'"));
                }
                else
                {
                    AddOptionsSuggestions(pair->second.c_str(), 0, currentValue,
                                          ret);
                }
            }

            return ret;
        });
}

/************************************************************************/
/*                   GDALRasterMosaicAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterMosaicAlgorithm::RunStep(GDALRasterPipelineStepRunContext &ctxt)
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
