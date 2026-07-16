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
#include "gdal_alg_priv.h"
#include "gdal_priv.h"

#include <algorithm>
#include <limits>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                  GDALRasterRGBToPaletteAlgorithm()                   */
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
    AddArg("dst-nodata", 0, _("Destination nodata value"), &m_dstNoData)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("no-dither", 0, _("Disable Floyd-Steinberg dithering"), &m_noDither);
    AddArg("bit-depth", 0,
           _("Bit depth of color palette component (8 bit causes longer "
             "computation time)"),
           &m_bitDepth)
        .SetDefault(m_bitDepth)
        .SetChoices("5", "8");
}

/************************************************************************/
/*              GDALRasterRGBToPaletteAlgorithm::RunStep()              */
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
    else if (nSrcBandCount == 4 &&
             poSrcDS->GetRasterBand(4)->GetColorInterpretation() ==
                 GCI_AlphaBand)
    {
        // nothing to do
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
        if (poSrcBand->GetRasterDataType() != GDT_UInt8)
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
        poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), 1, GDT_UInt8,
        /* bTiledIfPossible = */ true, poSrcDS, /* bCopyMetadata = */ true);
    if (!poTmpDS)
        return false;

    const double oneOverStep = 1.0 / ((m_colorMap.empty() ? 1 : 0) + 1);

    if (m_colorMap.empty() && m_dstNoData < 0)
    {
        int bSrcHasNoDataR = FALSE;
        const double dfSrcNoDataR =
            GDALGetRasterNoDataValue(mapBands[GCI_RedBand], &bSrcHasNoDataR);
        int bSrcHasNoDataG = FALSE;
        const double dfSrcNoDataG =
            GDALGetRasterNoDataValue(mapBands[GCI_GreenBand], &bSrcHasNoDataG);
        int bSrcHasNoDataB = FALSE;
        const double dfSrcNoDataB =
            GDALGetRasterNoDataValue(mapBands[GCI_BlueBand], &bSrcHasNoDataB);
        if (bSrcHasNoDataR && bSrcHasNoDataG && bSrcHasNoDataB &&
            dfSrcNoDataR == dfSrcNoDataG && dfSrcNoDataR == dfSrcNoDataB &&
            dfSrcNoDataR >= 0 && dfSrcNoDataR <= 255 &&
            std::round(dfSrcNoDataR) == dfSrcNoDataR)
        {
            m_dstNoData = 0;
        }
        else
        {
            const int nMaskFlags = GDALGetMaskFlags(mapBands[GCI_RedBand]);
            if ((nMaskFlags & GMF_PER_DATASET))
                m_dstNoData = 0;
        }
    }

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

        const int nXSize = poSrcDS->GetRasterXSize();
        const int nYSize = poSrcDS->GetRasterYSize();

        if (m_dstNoData >= 0 && m_colorCount == 256)
            --m_colorCount;
        if (nYSize == 0)
        {
            bOK = false;
        }
        else if (static_cast<GUInt32>(nXSize) <
                 std::numeric_limits<GUInt32>::max() /
                     static_cast<GUInt32>(nYSize))
        {
            bOK =
                GDALComputeMedianCutPCTInternal(
                    mapBands[GCI_RedBand], mapBands[GCI_GreenBand],
                    mapBands[GCI_BlueBand], nullptr, nullptr, nullptr, nullptr,
                    m_colorCount, m_bitDepth, static_cast<GUInt32 *>(nullptr),
                    GDALColorTable::ToHandle(&oCT),
                    pScaledData ? GDALScaledProgress : nullptr,
                    pScaledData.get()) == CE_None;
        }
        else
        {
            bOK =
                GDALComputeMedianCutPCTInternal(
                    mapBands[GCI_RedBand], mapBands[GCI_GreenBand],
                    mapBands[GCI_BlueBand], nullptr, nullptr, nullptr, nullptr,
                    m_colorCount, m_bitDepth, static_cast<GUIntBig *>(nullptr),
                    GDALColorTable::ToHandle(&oCT),
                    pScaledData ? GDALScaledProgress : nullptr,
                    pScaledData.get()) == CE_None;
        }
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

        m_colorCount = oCT.GetColorEntryCount();
    }

    if (m_dstNoData >= 0)
    {
        for (int i = std::min(255, m_colorCount); i > m_dstNoData; --i)
        {
            oCT.SetColorEntry(i, oCT.GetColorEntry(i - 1));
        }

        poTmpDS->GetRasterBand(1)->SetNoDataValue(m_dstNoData);
        GDALColorEntry sEntry = {0, 0, 0, 0};
        oCT.SetColorEntry(m_dstNoData, &sEntry);
    }

    if (bOK)
    {
        poTmpDS->GetRasterBand(1)->SetColorTable(&oCT);

        pScaledData.reset(GDALCreateScaledProgress(dfLastProgress, 1.0,
                                                   pfnProgress, pProgressData));

        bOK = GDALDitherRGB2PCTInternal(
                  mapBands[GCI_RedBand], mapBands[GCI_GreenBand],
                  mapBands[GCI_BlueBand],
                  GDALRasterBand::ToHandle(poTmpDS->GetRasterBand(1)),
                  GDALColorTable::ToHandle(&oCT), m_bitDepth,
                  /* pasDynamicColorMap = */ nullptr, !m_noDither,
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
