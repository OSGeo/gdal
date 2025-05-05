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

GDALRasterCleanCollarAlgorithm::GDALRasterCleanCollarAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();

    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER,
                        /* positionalAndRequired = */ false)
        .SetPositional()
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddOutputFormatArg(&m_format, /* bStreamAllowed = */ false,
                       /* bGDALGAllowed = */ false)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_CREATE, GDAL_DCAP_RASTER});
    AddCreationOptionsArg(&m_creationOptions);
    AddOverwriteArg(&m_overwrite);
    AddUpdateArg(&m_update);

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
/*               GDALRasterCleanCollarAlgorithm::RunImpl()              */
/************************************************************************/

bool GDALRasterCleanCollarAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                             void *pProgressData)
{
    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poDstDS = m_outputDataset.GetDatasetRef();
    if (poSrcDS == poDstDS && poSrcDS->GetAccess() == GA_ReadOnly)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Dataset should be opened in update mode");
        return false;
    }

    const bool dstDSWasNull = poDstDS == nullptr;

    if (dstDSWasNull && !m_outputDataset.IsNameSet() && !m_update)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Output dataset is not specified. If you intend to update "
                    "the input dataset, set the 'update' option");
        return false;
    }

    if (!poDstDS && !m_outputDataset.GetName().empty() && poDstDS != poSrcDS)
    {
        VSIStatBufL sStat;
        bool fileExists{VSIStatL(m_outputDataset.GetName().c_str(), &sStat) ==
                        0};

        {
            CPLErrorStateBackuper oCPLErrorHandlerPusher(CPLQuietErrorHandler);
            poDstDS = GDALDataset::FromHandle(GDALOpenEx(
                m_outputDataset.GetName().c_str(),
                GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR | GDAL_OF_UPDATE,
                nullptr, nullptr, nullptr));
            CPLErrorReset();
        }

        if ((poDstDS || fileExists) && !m_overwrite && !m_update)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Dataset '%s' already exists. Specify the --overwrite "
                     "option to overwrite it or the --update option to "
                     "update it.",
                     m_outputDataset.GetName().c_str());
            delete poDstDS;
            return false;
        }

        if (poDstDS && fileExists && m_overwrite)
        {
            // Delete the existing file
            delete poDstDS;
            poDstDS = nullptr;
            if (VSIUnlink(m_outputDataset.GetName().c_str()) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to delete existing dataset '%s'.",
                         m_outputDataset.GetName().c_str());
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

    GDALNearblackOptionsSetProgress(psOptions.get(), pfnProgress,
                                    pProgressData);

    auto poRetDS = GDALDataset::FromHandle(GDALNearblack(
        m_outputDataset.GetName().c_str(), GDALDataset::ToHandle(poDstDS),
        GDALDataset::ToHandle(poSrcDS), psOptions.get(), nullptr));
    if (!poRetDS)
        return false;

    if (poDstDS == nullptr)
    {
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
    }
    else if (dstDSWasNull)
    {
        const bool bCloseOK = poDstDS->Close() == CE_None;
        delete poDstDS;
        if (!bCloseOK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to close output dataset");
            return false;
        }
    }

    return true;
}

//! @endcond
