/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster shift-longitude" subcommand
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_shift_longitude.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
#include "../frmts/vrt/gdal_vrt.h"
#include "../frmts/vrt/vrtdataset.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*GDALRasterShiftLongitudeAlgorithm::GDALRasterShiftLongitudeAlgorithm()*/
/************************************************************************/

GDALRasterShiftLongitudeAlgorithm::GDALRasterShiftLongitudeAlgorithm(
    bool bStandalone)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL, bStandalone)
{
    AddArg("min-x", 0, _("Sets the minimum longitude value"), &m_minX)
        .SetRequired();
    AddArg("max-x", 0, _("Sets the maximum longitude value"), &m_maxX)
        .SetRequired();
    AddNodataArg(&m_nodata, /* noneAllowed = */ true, "output-nodata");
}

/************************************************************************/
/*             GDALRasterShiftLongitudeAlgorithm::RunStep()             */
/************************************************************************/

static double NormalizeLongitude(double lon)
{
    while (lon < -180.0)
        lon += 360.0;
    while (lon > 180.0)
        lon -= 360.0;
    return lon;
}

bool GDALRasterShiftLongitudeAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    CPLAssert(!m_outputDataset.GetDatasetRef());

    const auto poSrcDS = m_inputDataset[0].GetDatasetRef();

    GDALGeoTransform srcGT;
    if (poSrcDS->GetGeoTransform(srcGT) != CE_None)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Input dataset does not have a geotransform");
        return false;
    }

    if (!srcGT.IsAxisAligned())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Input dataset geotransform cannot have a rotation term");
        return false;
    }

    const auto nSrcXSize = poSrcDS->GetRasterXSize();

    const double dfSrcMinX = srcGT.xorig;
    const double dfSrcMaxX = srcGT.xorig + srcGT.xscale * nSrcXSize;

    // Snap m_minX and m_maxX to pixel boundaries of the input raster
    {
        const double dfXOff =
            std::ceil(std::abs(dfSrcMinX - m_minX) / srcGT.xscale) *
            (m_minX < dfSrcMinX ? -1 : 1);
        if (!GDALIsValueInRange<int>(dfXOff))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to compute pixel offset between minimum x "
                     "coordinate of input and output.");
            return false;
        }
        const int nXOff = static_cast<int>(dfXOff);
        m_minX = dfSrcMinX + nXOff * srcGT.xscale;
    }
    const double dfDstXSize = std::ceil((m_maxX - m_minX) / srcGT.xscale);
    if (!GDALIsValueInRange<int>(dfDstXSize))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Size of output raster exceeds maximum allowable size.");
        return false;
    }
    const int nDstXSize = static_cast<int>(dfDstXSize);

    if (nDstXSize <= 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "--max-x must be greater than --min-x");
        return false;
    }

    m_maxX = m_minX + nDstXSize * srcGT.xscale;
    const int nYSize = poSrcDS->GetRasterYSize();

    auto poDstDS = std::make_unique<VRTDataset>(nDstXSize, nYSize);
    {
        GDALGeoTransform dstGT = srcGT;
        dstGT.xorig = m_minX;
        poDstDS->SetGeoTransform(dstGT);
    }

    std::vector<GDALRasterWindow> aosSrcWindows;
    for (int nDstXOff = 0; nDstXOff < nDstXSize;)
    {
        double dfDstChunkMinX = m_minX + nDstXOff * srcGT.xscale;

        while (dfDstChunkMinX < dfSrcMinX)
        {
            dfDstChunkMinX += 360;
        }
        while (dfDstChunkMinX >= dfSrcMaxX)
        {
            dfDstChunkMinX -= 360;
        }

        const double dfSrcXOff =
            std::round(dfDstChunkMinX - dfSrcMinX) / srcGT.xscale;
        if (!GDALIsValueInRange<int>(dfSrcXOff))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to calculate source pixels for longitude range "
                     "beginning at %g",
                     dfDstChunkMinX);
            return false;
        }
        const int nSrcXOff = static_cast<int>(dfSrcXOff);

        if (nSrcXOff < 0)
        {
            const double dfMissingRangeMinX =
                NormalizeLongitude(dfDstChunkMinX);
            const int nMissingPx =
                std::min(std::abs(nSrcXOff), nDstXSize - nDstXOff);
            double dfMissingRangeMaxX =
                NormalizeLongitude(dfDstChunkMinX + srcGT.xscale * nMissingPx);
            while (dfMissingRangeMaxX < dfMissingRangeMinX)
            {
                dfMissingRangeMaxX += 360;
            }

            CPLError(
                CE_Warning, CPLE_AppDefined,
                "No source data available for output longitude range %g to %g",
                dfMissingRangeMinX, dfMissingRangeMaxX);
            nDstXOff += nMissingPx;

            const GDALRasterWindow chunk{-1, -1, nMissingPx, nYSize};
            aosSrcWindows.push_back(chunk);
            continue;
        }

        const int nColumnsWanted = nSrcXSize - nSrcXOff;
        const int nColumnsAvailable = nDstXSize - nDstXOff;

        const GDALRasterWindow chunk{
            nSrcXOff, 0, std::min(nColumnsWanted, nColumnsAvailable), nYSize};

        CPLAssert(chunk.nXSize > 0);

        const double dstChunkMaxX = dfDstChunkMinX + chunk.nXSize;
        CPLDebug("ShiftLongitude", "Src %g - %g, Dst %g - %g", dfDstChunkMinX,
                 dstChunkMaxX, m_minX + chunk.nXOff * srcGT.xscale,
                 m_minX + (chunk.nXOff + chunk.nXSize) * srcGT.xscale);

        aosSrcWindows.push_back(chunk);
        nDstXOff += chunk.nXSize;
    }

    for (int iBand = 1; iBand <= poSrcDS->GetRasterCount(); ++iBand)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand);
        poDstDS->AddBand(poSrcBand->GetRasterDataType());
        VRTSourcedRasterBand *poDstBand =
            cpl::down_cast<VRTSourcedRasterBand *>(
                poDstDS->GetRasterBand(iBand));
        poDstBand->CopyCommonInfoFrom(poSrcBand);

        bool bHasNoData;
        double dfSrcNoData{0};

        {
            int bSuccess;
            dfSrcNoData = poSrcBand->GetNoDataValue(&bSuccess);
            if (!bSuccess)
            {
                dfSrcNoData = VRT_NODATA_UNSET;
            }
        }

        if (!m_nodata.empty())
        {
            bool inexact = false;
            if (poDstBand->SetNoDataValueAsString(m_nodata.c_str(), &inexact) !=
                CE_None)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid NoData value: %s", m_nodata.c_str());
                return false;
            }
            if (inexact)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Specified NoData value cannot be represented in the "
                         "output data type.");
                return false;
            }
            bHasNoData = true;
        }
        else
        {
            bHasNoData = GDALCopyNoDataValue(poDstBand, poSrcBand, nullptr);
        }

        int nDstXOff = 0;
        for (const auto &chunk : aosSrcWindows)
        {
            const bool bDataAvailable = chunk.nXOff >= 0;

            if (bDataAvailable)
            {
                CPLDebug("ShiftLongitude", "Adding source chunk %d,%d %d %d",
                         chunk.nXOff, chunk.nYOff, chunk.nXSize, chunk.nYSize);

                // Scale and offset are propagated to the output band, not handled by the ComplexSource
                constexpr double dfSourceScale = 1.0;
                constexpr double dfSourceOffset = 0.0;
                constexpr int nSrcYOff = 0;
                constexpr int nDstYOff = 0;
                if (poDstBand->AddComplexSource(
                        poSrcBand, chunk.nXOff, nSrcYOff, chunk.nXSize,
                        chunk.nYSize, nDstXOff, nDstYOff, chunk.nXSize,
                        chunk.nYSize, dfSourceOffset, dfSourceScale,
                        dfSrcNoData) != CE_None)
                {
                    return false;
                }
            }
            else if (!bHasNoData)
            {
                CPLErrorOnce(
                    CE_Warning, CPLE_AppDefined,
                    "Some regions of the output dataset have no source data "
                    "available, but a NoData value has not been defined. You "
                    "can set a value using --output-nodata");
            }
            nDstXOff += chunk.nXSize;
        }
    }

    m_outputDataset.Set(std::move(poDstDS));

    return true;
}

GDALRasterShiftLongitudeAlgorithmStandalone::
    ~GDALRasterShiftLongitudeAlgorithmStandalone() = default;

//! @endcond
