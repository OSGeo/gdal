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
#include "commonutils.h"

#include <array>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                    GDALRasterRGBToPaletteAlgorithm()                 */
/************************************************************************/

GDALRasterRGBToPaletteAlgorithm::GDALRasterRGBToPaletteAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();

    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER);
    AddOutputFormatArg(&m_format, /* bStreamAllowed = */ false,
                       /* bGDALGAllowed = */ false)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_RASTER, GDAL_DCAP_CREATE});
    AddCreationOptionsArg(&m_creationOptions);
    AddOverwriteArg(&m_overwrite);

    AddArg("color-count", 0,
           _("Select the number of colors in the generated color table"),
           &m_colorCount)
        .SetDefault(m_colorCount)
        .SetMinValueIncluded(2)
        .SetMaxValueIncluded(256);
    AddArg("color-map", 0, _("Color map filename"), &m_colorMap);
}

/************************************************************************/
/*                GDALRasterRGBToPaletteAlgorithm::RunImpl()            */
/************************************************************************/

bool GDALRasterRGBToPaletteAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);

    if (m_format.empty())
    {
        m_format = GetOutputDriverForRaster(m_outputDataset.GetName().c_str());
        if (m_format.empty())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot guess output driver from output filename");
            return false;
        }
    }
    if (EQUAL(m_format.c_str(), "JPEG") || EQUAL(m_format.c_str(), "WEBP"))
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Format %s does not support color tables",
                    CPLString(m_format).toupper().c_str());
        return false;
    }

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

    bool bNeedTmpFile =
        !EQUAL(m_format.c_str(), "MEM") && !EQUAL(m_format.c_str(), "GTiff");
    if (EQUAL(m_format.c_str(), "GTiff"))
    {
        bool bIsCompressed = false;
        bool bIsTiled = false;
        for (const auto &co : m_creationOptions)
        {
            if (STARTS_WITH_CI(co.c_str(), "COMPRESS=") &&
                !STARTS_WITH_CI(co.c_str(), "COMPRESS=NONE"))
            {
                bIsCompressed = true;
            }
            else if (STARTS_WITH_CI(co.c_str(), "TILED=") &&
                     CPLTestBool(co.c_str() + strlen("TILED=")))
            {
                bIsTiled = true;
            }
        }
        if (bIsCompressed && bIsTiled)
            bNeedTmpFile = true;
    }

    const char *pszTmpDriverName =
        EQUAL(m_format.c_str(), "MEM") ? "MEM" : "GTiff";
    auto poTmpDriver =
        GetGDALDriverManager()->GetDriverByName(pszTmpDriverName);
    if (!poTmpDriver)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "%s driver not available",
                    pszTmpDriverName);
        return false;
    }

    const double oneOverStep =
        1.0 / ((m_colorMap.empty() ? 1 : 0) + (bNeedTmpFile ? 2 : 1));

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

    std::unique_ptr<GDALDataset> poTmpDS;
    if (bOK)
    {
        const std::string osTmpFilename =
            bNeedTmpFile ? m_outputDataset.GetName() + ".tmp.tif"
                         : m_outputDataset.GetName();
        CPLStringList aosTmpCO;
        if (!bNeedTmpFile)
            aosTmpCO = CPLStringList(m_creationOptions);
        poTmpDS.reset(poTmpDriver->Create(
            osTmpFilename.c_str(), poSrcDS->GetRasterXSize(),
            poSrcDS->GetRasterYSize(), 1, GDT_Byte, aosTmpCO.List()));
        bOK = poTmpDS != nullptr;
        if (bOK && bNeedTmpFile)
        {
            // In file systems that allow it (all but Windows...), we want to
            // delete the temporary file as soon as soon as possible after
            // having open it, so that if someone kills the process there are
            // no temp files left over. If that unlink() doesn't succeed
            // (on Windows), then the file will eventually be deleted when
            // poTmpDS is cleaned due to MarkSuppressOnClose().
            VSIUnlink(osTmpFilename.c_str());
            poTmpDS->MarkSuppressOnClose();
        }
    }
    if (bOK)
    {
        poTmpDS->GetRasterBand(1)->SetColorTable(&oCT);
        poTmpDS->SetSpatialRef(poSrcDS->GetSpatialRef());
        std::array<double, 6> adfGT{};
        if (poSrcDS->GetGeoTransform(adfGT.data()) == CE_None)
            poTmpDS->SetGeoTransform(adfGT.data());
        if (const int nGCPCount = poSrcDS->GetGCPCount())
        {
            const auto apsGCPs = poSrcDS->GetGCPs();
            if (apsGCPs)
            {
                poTmpDS->SetGCPs(nGCPCount, apsGCPs,
                                 poSrcDS->GetGCPSpatialRef());
            }
        }
        poTmpDS->SetMetadata(poSrcDS->GetMetadata());

        pScaledData.reset(GDALCreateScaledProgress(dfLastProgress,
                                                   dfLastProgress + oneOverStep,
                                                   pfnProgress, pProgressData));
        dfLastProgress += oneOverStep;
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
        if (!bNeedTmpFile)
        {
            m_outputDataset.Set(std::move(poTmpDS));
        }
        else
        {
            auto poOutDriver =
                GetGDALDriverManager()->GetDriverByName(m_format.c_str());
            CPLAssert(poOutDriver);
            const CPLStringList aosCO(m_creationOptions);
            pScaledData.reset(GDALCreateScaledProgress(
                dfLastProgress, 1.0, pfnProgress, pProgressData));
            auto poOutDS = std::unique_ptr<GDALDataset>(poOutDriver->CreateCopy(
                m_outputDataset.GetName().c_str(), poTmpDS.get(),
                /* bStrict = */ false, aosCO.List(),
                pScaledData ? GDALScaledProgress : nullptr, pScaledData.get()));
            bOK = poOutDS != nullptr;
            m_outputDataset.Set(std::move(poOutDS));
        }
    }
    if (bOK && pfnProgress)
        pfnProgress(1.0, "", pProgressData);

    return bOK;
}

//! @endcond
