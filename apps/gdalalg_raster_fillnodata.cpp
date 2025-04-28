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

#include "gdalalg_raster_fillnodata.h"

#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdal_alg.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*   GDALRasterFillNodataAlgorithm::GDALRasterFillNodataAlgorithm()     */
/************************************************************************/

GDALRasterFillNodataAlgorithm::GDALRasterFillNodataAlgorithm() noexcept
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{

    AddProgressArg();

    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER);
    AddOutputFormatArg(&m_format, /* bStreamAllowed = */ false,
                       /* bGDALGAllowed = */ false)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_CREATE, GDAL_DCAP_RASTER});

    AddCreationOptionsArg(&m_creationOptions);
    AddOverwriteArg(&m_overwrite);

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

    mask.AddValidationAction(
        [&mask]()
        {
            // Check if the mask dataset is a valid raster dataset descriptor
            // Try to open the dataset as a raster
            std::unique_ptr<GDALDataset> poDS(
                GDALDataset::Open(mask.Get<std::string>().c_str(),
                                  GDAL_OF_RASTER, nullptr, nullptr, nullptr));
            return static_cast<bool>(poDS);
        });

    AddArg("strategy", 0,
           _("By default, pixels are interpolated using an inverse distance "
             "weighting (invdist). It is also possible to choose a nearest "
             "neighbour (nearest) strategy."),
           &m_strategy)
        .SetDefault(m_strategy)
        .SetChoices("invdist", "nearest");
}

/************************************************************************/
/*                 GDALRasterFillNodataAlgorithm::RunImpl()             */
/************************************************************************/

bool GDALRasterFillNodataAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                            void *pProgressData)
{

    VSIStatBufL sStat;
    if (!m_overwrite && !m_outputDataset.GetName().empty() &&
        (VSIStatL(m_outputDataset.GetName().c_str(), &sStat) == 0 ||
         std::unique_ptr<GDALDataset>(
             GDALDataset::Open(m_outputDataset.GetName().c_str()))))
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "File '%s' already exists. Specify the --overwrite "
                    "option to overwrite it.",
                    m_outputDataset.GetName().c_str());
        return false;
    }

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString(m_format.c_str());
    aosOptions.AddString("-b");
    aosOptions.AddString(CPLSPrintf("%d", m_band));

    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(co.c_str());
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
    auto poRetDS =
        std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(GDALTranslate(
            m_outputDataset.GetName().c_str(), hSrcDS, psOptions, nullptr)));
    GDALTranslateOptionsFree(psOptions);

    if (!poRetDS)
    {
        return false;
    }

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

    GDALRasterBand *dstBand{poRetDS->GetRasterBand(1)};
    // Prepare options to pass to GDALFillNodata
    CPLStringList aosFillOptions;

    if (EQUAL(m_strategy.c_str(), "nearest"))
        aosFillOptions.AddNameValue("INTERPOLATION", "NEAREST");
    else
        aosFillOptions.AddNameValue("INTERPOLATION",
                                    "INV_DIST");  // default strategy

    auto retVal{GDALFillNodata(
        dstBand, maskBand, m_maxDistance, 0, m_smoothingIterations,
        aosFillOptions.List(), m_progressBarRequested ? pfnProgress : nullptr,
        m_progressBarRequested ? pProgressData : nullptr)};
    if (retVal != CE_None)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot run fillNodata.");
        return false;
    }

    poRetDS->FlushCache();

    m_outputDataset.Set(std::move(poRetDS));

    return true;
}

//! @endcond
