/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster rgb-to-palette" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_rgb_to_palette.h"

#include "cpl_string.h"
#include "gdal_alg.h"
#include "gdal_priv.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALRasterRGBToPaletteAlgorithm()                 */
/************************************************************************/

GDALRasterRGBToPaletteAlgorithm::GDALRasterRGBToPaletteAlgorithm(
    bool standaloneStep)
    : GDALRasterPipelineNonNativelyStreamingAlgorithm(NAME, DESCRIPTION,
                                                      HELP_URL, standaloneStep)
{
    AddArg("color-count", 0,
           _("Select the number of colors in the generated color table"),
           &m_colorCount)
        .SetDefault(m_colorCount)
        .SetMinValueIncluded(2)
        .SetMaxValueIncluded(256);
    AddArg("color-map", 0, _("Color map filename"), &m_colorMap);
}

/************************************************************************/
/*                GDALRasterRGBToPaletteAlgorithm::RunStep()            */
/************************************************************************/

bool GDALRasterRGBToPaletteAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    const int nSrcBandCount = poSrcDS->GetRasterCount();
    if (nSrcBandCount < 3)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Input dataset must have at least 3 bands");
        return false;
    }
    else if (nSrcBandCount >= 4)
    {
        ReportError(
            CE_Warning, CPLE_AppDefined,
            "Only R,G,B bands of input dataset will be taken into account");
    }

    std::map<GDALColorInterp, GDALRasterBandH> mapBands;
    int nFound = 0;
    for (int i = 1; i <= nSrcBandCount; ++i)
    {
        auto poSrcBand = poSrcDS->GetRasterBand(i);
        if (poSrcBand->GetRasterDataType() != GDT_Byte)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Non-byte band found and not supported");
            return false;
        }
        const auto eColorInterp = poSrcBand->GetColorInterpretation();
        for (const auto eInterestColorInterp :
             {GCI_RedBand, GCI_GreenBand, GCI_BlueBand})
        {
            if (eColorInterp == eInterestColorInterp)
            {
                if (mapBands.find(eColorInterp) != mapBands.end())
                {
                    ReportError(CE_Failure, CPLE_NotSupported,
                                "Several %s bands found",
                                GDALGetColorInterpretationName(eColorInterp));
                    return false;
                }
                ++nFound;
                mapBands[eColorInterp] = GDALRasterBand::ToHandle(poSrcBand);
            }
        }
    }

    if (nFound < 3)
    {
        if (nFound > 0)
        {
            ReportError(
                CE_Warning, CPLE_AppDefined,
                "Assuming first band is red, second green and third blue, "
                "despite at least one band with one of those color "
                "interpretation "
                "found");
        }
        mapBands[GCI_RedBand] =
            GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(1));
        mapBands[GCI_GreenBand] =
            GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(2));
        mapBands[GCI_BlueBand] =
            GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(3));
    }

    auto poTmpDS = CreateTemporaryDataset(
        poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), 1, GDT_Byte,
        /* bTiledIfPossible = */ true, poSrcDS, /* bCopyMetadata = */ true);
    if (!poTmpDS)
        return false;

    const double oneOverStep = 1.0 / ((m_colorMap.empty() ? 1 : 0) + 1);

    GDALColorTable oCT;

    bool bOK = true;
    double dfLastProgress = 0;
    std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)> pScaledData(
        nullptr, GDALDestroyScaledProgress);
    if (m_colorMap.empty())
    {
        pScaledData.reset(GDALCreateScaledProgress(0, oneOverStep, pfnProgress,
                                                   pProgressData));
        dfLastProgress = oneOverStep;
        bOK = (GDALComputeMedianCutPCT(
                   mapBands[GCI_RedBand], mapBands[GCI_GreenBand],
                   mapBands[GCI_BlueBand], nullptr, m_colorCount,
                   GDALColorTable::ToHandle(&oCT),
                   pScaledData ? GDALScaledProgress : nullptr,
                   pScaledData.get()) == CE_None);
    }
    else
    {
        GDALDriverH hDriver;
        if ((hDriver = GDALIdentifyDriver(m_colorMap.c_str(), nullptr)) !=
                nullptr &&
            // Palette .txt files may be misidentified by the XYZ driver
            !EQUAL(GDALGetDescription(hDriver), "XYZ"))
        {
            auto poPaletteDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                m_colorMap.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                nullptr, nullptr, nullptr));
            bOK = poPaletteDS != nullptr && poPaletteDS->GetRasterCount() > 0;
            if (bOK)
            {
                const auto poCT =
                    poPaletteDS->GetRasterBand(1)->GetColorTable();
                if (poCT)
                {
                    oCT = *poCT;
                }
                else
                {
                    bOK = false;
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Dataset '%s' does not contain a color table",
                                m_colorMap.c_str());
                }
            }
        }
        else
        {
            auto poCT = GDALColorTable::LoadFromFile(m_colorMap.c_str());
            bOK = poCT != nullptr;
            if (bOK)
            {
                oCT = std::move(*(poCT.get()));
            }
        }
    }

    if (bOK)
    {
        poTmpDS->GetRasterBand(1)->SetColorTable(&oCT);

        pScaledData.reset(GDALCreateScaledProgress(dfLastProgress, 1.0,
                                                   pfnProgress, pProgressData));

        bOK = GDALDitherRGB2PCT(
                  mapBands[GCI_RedBand], mapBands[GCI_GreenBand],
                  mapBands[GCI_BlueBand],
                  GDALRasterBand::ToHandle(poTmpDS->GetRasterBand(1)),
                  GDALColorTable::ToHandle(&oCT),
                  pScaledData ? GDALScaledProgress : nullptr,
                  pScaledData.get()) == CE_None;
    }

    if (bOK)
    {
        m_outputDataset.Set(std::move(poTmpDS));
        if (pfnProgress)
            pfnProgress(1.0, "", pProgressData);
    }

    return bOK;
}

GDALRasterRGBToPaletteAlgorithmStandalone::
    ~GDALRasterRGBToPaletteAlgorithmStandalone() = default;

//! @endcond
