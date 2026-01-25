/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "gdal raster fillnodata" standalone command
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_fill_nodata.h"

#include "cpl_progress.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "commonutils.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*    GDALRasterFillNodataAlgorithm::GDALRasterFillNodataAlgorithm()    */
/************************************************************************/

GDALRasterFillNodataAlgorithm::GDALRasterFillNodataAlgorithm(
    bool standalone) noexcept
    : GDALRasterPipelineNonNativelyStreamingAlgorithm(NAME, DESCRIPTION,
                                                      HELP_URL, standalone)
{
    AddBandArg(&m_band).SetDefault(m_band);

    AddArg("max-distance", 'd',
           _("The maximum distance (in pixels) that the algorithm will search "
             "out for values to interpolate."),
           &m_maxDistance)
        .SetDefault(m_maxDistance)
        .SetMetaVar("MAX_DISTANCE");

    AddArg("smoothing-iterations", 's',
           _("The number of 3x3 average filter smoothing iterations to run "
             "after the interpolation to dampen artifacts. The default is zero "
             "smoothing iterations."),
           &m_smoothingIterations)
        .SetDefault(m_smoothingIterations)
        .SetMetaVar("SMOOTHING_ITERATIONS");

    auto &mask{AddArg("mask", 0,
                      _("Use the first band of the specified file as a "
                        "validity mask (zero is invalid, non-zero is valid)."),
                      &m_maskDataset)};

    SetAutoCompleteFunctionForFilename(mask, GDAL_OF_RASTER);

    AddArg("strategy", 0,
           _("By default, pixels are interpolated using an inverse distance "
             "weighting (invdist). It is also possible to choose a nearest "
             "neighbour (nearest) strategy."),
           &m_strategy)
        .SetDefault(m_strategy)
        .SetChoices("invdist", "nearest");
}

/************************************************************************/
/*               GDALRasterFillNodataAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterFillNodataAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;

    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    std::unique_ptr<void, decltype(&GDALDestroyScaledProgress)> pScaledData(
        GDALCreateScaledProgress(0.0, 0.5, pfnProgress, pProgressData),
        GDALDestroyScaledProgress);
    auto poTmpDS = CreateTemporaryCopy(
        this, poSrcDS, m_band, true, pScaledData ? GDALScaledProgress : nullptr,
        pScaledData.get());
    if (!poTmpDS)
        return false;

    GDALRasterBand *maskBand{nullptr};
    if (m_maskDataset.GetDatasetRef())
    {
        maskBand = m_maskDataset.GetDatasetRef()->GetRasterBand(1);
        if (!maskBand)
        {
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot get mask band.");
            return false;
        }
    }

    // Get the output band
    GDALRasterBand *dstBand{poTmpDS->GetRasterBand(1)};
    CPLAssert(dstBand);

    // Prepare options to pass to GDALFillNodata
    CPLStringList aosFillOptions;

    if (EQUAL(m_strategy.c_str(), "nearest"))
        aosFillOptions.AddNameValue("INTERPOLATION", "NEAREST");
    else
        aosFillOptions.AddNameValue("INTERPOLATION",
                                    "INV_DIST");  // default strategy

    pScaledData.reset(
        GDALCreateScaledProgress(0.5, 1.0, pfnProgress, pProgressData));
    const auto retVal = GDALFillNodata(
        dstBand, maskBand, m_maxDistance, 0, m_smoothingIterations,
        aosFillOptions.List(), pScaledData ? GDALScaledProgress : nullptr,
        pScaledData.get());

    if (retVal == CE_None)
    {
        if (pfnProgress)
            pfnProgress(1.0, "", pProgressData);
        m_outputDataset.Set(std::move(poTmpDS));
    }

    return retVal == CE_None;
}

GDALRasterFillNodataAlgorithmStandalone::
    ~GDALRasterFillNodataAlgorithmStandalone() = default;

//! @endcond
