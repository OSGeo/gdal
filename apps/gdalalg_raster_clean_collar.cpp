/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster clean-collar" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_clean_collar.h"

#include "cpl_conv.h"
#include "cpl_vsi_virtual.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*   GDALRasterCleanCollarAlgorithm::GDALRasterCleanCollarAlgorithm()   */
/************************************************************************/

GDALRasterCleanCollarAlgorithm::GDALRasterCleanCollarAlgorithm(
    bool standaloneStep)
    : GDALRasterPipelineNonNativelyStreamingAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetAddDefaultArguments(false)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    if (standaloneStep)
    {
        AddProgressArg();
        AddRasterInputArgs(false, false);

        AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER,
                            /* positionalAndRequired = */ false)
            .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT)
            .SetAvailableInPipelineStep(false)
            .SetPositional();
        AddOutputFormatArg(&m_format, /* bStreamAllowed = */ false,
                           /* bGDALGAllowed = */ false)
            .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                             {GDAL_DCAP_CREATE, GDAL_DCAP_RASTER})
            .SetAvailableInPipelineStep(false);
        AddCreationOptionsArg(&m_creationOptions)
            .SetAvailableInPipelineStep(false);
        AddOverwriteArg(&m_overwrite).SetAvailableInPipelineStep(false);
        AddUpdateArg(&m_update).SetAvailableInPipelineStep(false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
    }

    AddArg("color", 0,
           _("Transparent color(s): tuple of integer (like 'r,g,b'), 'black', "
             "'white'"),
           &m_color)
        .SetDefault("black")
        .SetPackedValuesAllowed(false)
        .AddValidationAction(
            [this]()
            {
                for (const auto &c : m_color)
                {
                    if (c != "white" && c != "black")
                    {
                        const CPLStringList aosTokens(
                            CSLTokenizeString2(c.c_str(), ",", 0));
                        for (const char *pszToken : aosTokens)
                        {
                            if (CPLGetValueType(pszToken) != CPL_VALUE_INTEGER)
                            {
                                ReportError(CE_Failure, CPLE_IllegalArg,
                                            "Value for 'color' should be tuple "
                                            "of integer (like 'r,g,b'), "
                                            "'black' or 'white'");
                                return false;
                            }
                        }
                    }
                }
                return true;
            });
    AddArg("color-threshold", 0,
           _("Select how far from specified transparent colors the pixel "
             "values are considered transparent."),
           &m_colorThreshold)
        .SetDefault(m_colorThreshold)
        .SetMinValueIncluded(0);
    AddArg("pixel-distance", 0,
           _("Number of consecutive transparent pixels that can be encountered "
             "before the giving up search inwards."),
           &m_pixelDistance)
        .SetDefault(m_pixelDistance)
        .SetMinValueIncluded(0);
    AddArg("add-alpha", 0, _("Adds an alpha band to the output dataset."),
           &m_addAlpha)
        .SetMutualExclusionGroup("addalpha-addmask");
    AddArg("add-mask", 0, _("Adds a mask band to the output dataset."),
           &m_addMask)
        .SetMutualExclusionGroup("addalpha-addmask");
    AddArg("algorithm", 0, _("Algorithm to apply"), &m_algorithm)
        .SetChoices("floodfill", "twopasses")
        .SetDefault(m_algorithm);
}

/************************************************************************/
/*              GDALRasterCleanCollarAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALRasterCleanCollarAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                             void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

/************************************************************************/
/*              GDALRasterCleanCollarAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterCleanCollarAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poDstDS = m_outputDataset.GetDatasetRef();
    if (!m_standaloneStep)
    {
        m_outputDataset.Set(CPLGenerateTempFilenameSafe("_clean_collar") +
                            ".tif");
        m_creationOptions.push_back("TILED=YES");
    }
    else
    {
        if (poSrcDS == poDstDS && poSrcDS->GetAccess() == GA_ReadOnly)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset should be opened in update mode");
            return false;
        }

        if (!poDstDS && !m_outputDataset.IsNameSet())
        {
            if (m_update)
            {
                m_outputDataset.Set(poSrcDS);
                poDstDS = poSrcDS;
            }
            else
            {
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Output dataset is not specified. If you intend to update "
                    "the input dataset, set the 'update' option");
                return false;
            }
        }
    }

    CPLStringList aosOptions;

    if (!m_format.empty())
    {
        aosOptions.push_back("-of");
        aosOptions.push_back(m_format.c_str());
    }

    for (const auto &co : m_creationOptions)
    {
        aosOptions.push_back("-co");
        aosOptions.push_back(co.c_str());
    }

    for (const auto &color : m_color)
    {
        aosOptions.push_back("-color");
        std::string osColor;
        int nNonAlphaSrcBands = poSrcDS->GetRasterCount();
        if (nNonAlphaSrcBands &&
            poSrcDS->GetRasterBand(nNonAlphaSrcBands)
                    ->GetColorInterpretation() == GCI_AlphaBand)
            --nNonAlphaSrcBands;
        if (color == "white")
        {
            for (int i = 0; i < nNonAlphaSrcBands; ++i)
            {
                if (i > 0)
                    osColor += ',';
                osColor += "255";
            }
        }
        else if (color == "black")
        {
            for (int i = 0; i < nNonAlphaSrcBands; ++i)
            {
                if (i > 0)
                    osColor += ',';
                osColor += "0";
            }
        }
        else
        {
            osColor = color;
        }
        aosOptions.push_back(osColor.c_str());
    }

    aosOptions.push_back("-near");
    aosOptions.push_back(CPLSPrintf("%d", m_colorThreshold));

    aosOptions.push_back("-nb");
    aosOptions.push_back(CPLSPrintf("%d", m_pixelDistance));

    if (m_addAlpha ||
        (!m_addMask && poDstDS == nullptr && poSrcDS->GetRasterCount() > 0 &&
         poSrcDS->GetRasterBand(poSrcDS->GetRasterCount())
                 ->GetColorInterpretation() == GCI_AlphaBand) ||
        (!m_addMask && poDstDS != nullptr && poDstDS->GetRasterCount() > 0 &&
         poDstDS->GetRasterBand(poDstDS->GetRasterCount())
                 ->GetColorInterpretation() == GCI_AlphaBand))
    {
        aosOptions.push_back("-setalpha");
    }

    if (m_addMask ||
        (!m_addAlpha && poDstDS == nullptr && poSrcDS->GetRasterCount() > 0 &&
         poSrcDS->GetRasterBand(1)->GetMaskFlags() == GMF_PER_DATASET) ||
        (!m_addAlpha && poDstDS != nullptr && poDstDS->GetRasterCount() > 0 &&
         poDstDS->GetRasterBand(1)->GetMaskFlags() == GMF_PER_DATASET))
    {
        aosOptions.push_back("-setmask");
    }

    aosOptions.push_back("-alg");
    aosOptions.push_back(m_algorithm.c_str());

    std::unique_ptr<GDALNearblackOptions, decltype(&GDALNearblackOptionsFree)>
        psOptions{GDALNearblackOptionsNew(aosOptions.List(), nullptr),
                  GDALNearblackOptionsFree};
    if (!psOptions)
        return false;

    GDALNearblackOptionsSetProgress(psOptions.get(), ctxt.m_pfnProgress,
                                    ctxt.m_pProgressData);

    auto poRetDS = GDALDataset::FromHandle(GDALNearblack(
        m_outputDataset.GetName().c_str(), GDALDataset::ToHandle(poDstDS),
        GDALDataset::ToHandle(poSrcDS), psOptions.get(), nullptr));
    if (!poRetDS)
    {
        if (!m_standaloneStep)
            VSIUnlink(m_outputDataset.GetName().c_str());
        return false;
    }

    if (poDstDS == nullptr)
    {
        if (!m_standaloneStep)
            poRetDS->MarkSuppressOnClose();
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
    }
    else
    {
        CPLAssert(poRetDS == poDstDS);
    }

    return true;
}

GDALRasterCleanCollarAlgorithmStandalone::
    ~GDALRasterCleanCollarAlgorithmStandalone() = default;

//! @endcond
