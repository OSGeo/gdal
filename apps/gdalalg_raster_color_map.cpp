/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "color-map" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_color_map.h"
#include "gdalalg_raster_write.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*      GDALRasterColorMapAlgorithm::GDALRasterColorMapAlgorithm()      */
/************************************************************************/

GDALRasterColorMapAlgorithm::GDALRasterColorMapAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("color-map", 0, _("Color map filename"), &m_colorMap);
    AddArg("add-alpha", 0, _("Adds an alpha mask band to the destination."),
           &m_addAlpha);
    AddArg("color-selection", 0,
           _("How to compute output colors from input values"),
           &m_colorSelection)
        .SetChoices("interpolate", "exact", "nearest")
        .SetDefault(m_colorSelection);
}

/************************************************************************/
/*           GDALRasterColorMapAlgorithm::CanHandleNextStep()           */
/************************************************************************/

bool GDALRasterColorMapAlgorithm::CanHandleNextStep(
    GDALPipelineStepAlgorithm *poNextStep) const
{
    return poNextStep->GetName() == GDALRasterWriteAlgorithm::NAME &&
           poNextStep->GetOutputFormat() != "stream";
}

/************************************************************************/
/*                GDALRasterColorMapAlgorithm::RunStep()                */
/************************************************************************/

bool GDALRasterColorMapAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    std::string outputFilename;
    if (ctxt.m_poNextUsableStep)
    {
        CPLAssert(CanHandleNextStep(ctxt.m_poNextUsableStep));
        outputFilename = ctxt.m_poNextUsableStep->GetOutputDataset().GetName();
        const auto &format = ctxt.m_poNextUsableStep->GetOutputFormat();
        if (!format.empty())
        {
            aosOptions.AddString("-of");
            aosOptions.AddString(format.c_str());
        }

        for (const std::string &co :
             ctxt.m_poNextUsableStep->GetCreationOptions())
        {
            aosOptions.AddString("-co");
            aosOptions.AddString(co.c_str());
        }
    }
    else
    {
        aosOptions.AddString("-of");
        aosOptions.AddString("VRT");
    }

    aosOptions.AddString("-b");
    aosOptions.AddString(CPLSPrintf("%d", m_band));

    if (m_colorMap.empty())
    {
        if (poSrcDS->GetRasterBand(m_band)->GetColorTable() == nullptr)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Input dataset has no color table and 'color-map' "
                        "option was not specified.");
            return false;
        }

        if (GetArg("color-selection")->IsExplicitlySet() &&
            m_colorSelection != "exact")
        {
            ReportError(
                CE_Warning, CPLE_NotSupported,
                "When using band color table, 'color-selection' is ignored");
        }

        aosOptions.AddString("-expand");
        aosOptions.AddString(m_addAlpha ? "rgba" : "rgb");

        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);
        if (ctxt.m_poNextUsableStep)
        {
            GDALTranslateOptionsSetProgress(psOptions, ctxt.m_pfnProgress,
                                            ctxt.m_pProgressData);
        }

        // Backup error state since GDALTranslate() resets it multiple times
        const auto nLastErrorNum = CPLGetLastErrorNo();
        const auto nLastErrorType = CPLGetLastErrorType();
        const std::string osLastErrorMsg = CPLGetLastErrorMsg();
        const auto nLastErrorCounter = CPLGetErrorCounter();

        auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
            GDALTranslate(outputFilename.c_str(),
                          GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));

        if (nLastErrorCounter > 0 && CPLGetErrorCounter() == 0)
        {
            CPLErrorSetState(nLastErrorType, nLastErrorNum,
                             osLastErrorMsg.c_str(), &nLastErrorCounter);
        }

        GDALTranslateOptionsFree(psOptions);
        const bool bRet = poOutDS != nullptr;
        if (poOutDS)
        {
            m_outputDataset.Set(std::move(poOutDS));
        }

        return bRet;
    }
    else
    {
        if (m_addAlpha)
            aosOptions.AddString("-alpha");
        if (m_colorSelection == "exact")
            aosOptions.AddString("-exact_color_entry");
        else if (m_colorSelection == "nearest")
            aosOptions.AddString("-nearest_color_entry");

        GDALDEMProcessingOptions *psOptions =
            GDALDEMProcessingOptionsNew(aosOptions.List(), nullptr);
        bool bOK = false;
        if (psOptions)
        {
            if (ctxt.m_poNextUsableStep)
            {
                GDALDEMProcessingOptionsSetProgress(
                    psOptions, ctxt.m_pfnProgress, ctxt.m_pProgressData);
            }
            auto poOutDS = std::unique_ptr<GDALDataset>(
                GDALDataset::FromHandle(GDALDEMProcessing(
                    outputFilename.c_str(), GDALDataset::ToHandle(poSrcDS),
                    "color-relief", m_colorMap.c_str(), psOptions, nullptr)));
            GDALDEMProcessingOptionsFree(psOptions);
            bOK = poOutDS != nullptr;
            if (poOutDS)
            {
                m_outputDataset.Set(std::move(poOutDS));
            }
        }
        return bOK;
    }
}

GDALRasterColorMapAlgorithmStandalone::
    ~GDALRasterColorMapAlgorithmStandalone() = default;

//! @endcond
