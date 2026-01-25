/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster sieve" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_sieve.h"

#include "cpl_conv.h"
#include "cpl_vsi_virtual.h"

#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "commonutils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*         GDALRasterSieveAlgorithm::GDALRasterSieveAlgorithm()         */
/************************************************************************/

GDALRasterSieveAlgorithm::GDALRasterSieveAlgorithm(bool standaloneStep)
    : GDALRasterPipelineNonNativelyStreamingAlgorithm(NAME, DESCRIPTION,
                                                      HELP_URL, standaloneStep)
{
    auto &mask{
        AddArg("mask", 0,
               _("Use the first band of the specified file as a "
                 "validity mask (all pixels with a value other than zero "
                 "will be considered suitable for inclusion in polygons)"),
               &m_maskDataset, GDAL_OF_RASTER)};

    SetAutoCompleteFunctionForFilename(mask, GDAL_OF_RASTER);

    AddBandArg(&m_band);
    AddArg("size-threshold", 's', _("Minimum size of polygons to keep"),
           &m_sizeThreshold)
        .SetDefault(m_sizeThreshold);

    AddArg("connect-diagonal-pixels", 'c',
           _("Consider diagonal pixels as connected"), &m_connectDiagonalPixels)
        .SetDefault(m_connectDiagonalPixels);
}

/************************************************************************/
/*                 GDALRasterSieveAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterSieveAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
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
    GDALRasterBand *dstBand = poTmpDS->GetRasterBand(1);
    CPLAssert(dstBand);

    pScaledData.reset(
        GDALCreateScaledProgress(0.5, 1.0, pfnProgress, pProgressData));
    const CPLErr err = GDALSieveFilter(
        dstBand, maskBand, dstBand, m_sizeThreshold,
        m_connectDiagonalPixels ? 8 : 4, nullptr,
        pScaledData ? GDALScaledProgress : nullptr, pScaledData.get());
    if (err == CE_None)
    {
        if (pfnProgress)
            pfnProgress(1.0, "", pProgressData);
        m_outputDataset.Set(std::move(poTmpDS));
    }

    return err == CE_None;
}

GDALRasterSieveAlgorithmStandalone::~GDALRasterSieveAlgorithmStandalone() =
    default;

//! @endcond
