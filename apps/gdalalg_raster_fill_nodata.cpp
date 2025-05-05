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

#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdal_alg.h"
#include "commonutils.h"

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
    const CPLStringList creationOptions{m_creationOptions};
    const std::string destinationFormat{
        m_format.empty()
            ? GetOutputDriverForRaster(m_outputDataset.GetName().c_str())
            : m_format};
    const bool isGtiff{EQUAL(destinationFormat.c_str(), "GTiff")};
    const bool isMem{EQUAL(destinationFormat.c_str(), "MEM")};
    const bool isCompressed{
        CSLFetchNameValue(creationOptions, "COMPRESS") != nullptr &&
        !EQUAL(CSLFetchNameValue(creationOptions, "COMPRESS"), "NONE")};
    const bool isTiled{
        isGtiff && CSLFetchNameValue(creationOptions, "TILED") != nullptr &&
        CPLTestBool(CSLFetchNameValue(creationOptions, "TILED"))};
    const bool useTemporaryFile{
        !(isMem || (isGtiff && isTiled && !isCompressed))};

    std::string workingFileName{useTemporaryFile
                                    ? CPLGenerateTempFilenameSafe(nullptr)
                                    : m_outputDataset.GetName()};
    std::unique_ptr<GDALDataset> workingDSHandler;

    CPLStringList options;
    options.AddString("-b");
    options.AddString(CPLSPrintf("%d", m_band));

    if (useTemporaryFile)
    {
        // Translate the single required band to uncompressed TILED GTiff
        options.AddString("-of");
        options.AddString("GTiff");
        options.AddString("-co");
        options.AddString("TILED=YES");
    }
    else
    {
        // Translate the single required band to the destination format
        if (!m_format.empty())
        {
            options.AddString("-of");
            options.AddString(m_format.c_str());
        }
        for (const auto &co : m_creationOptions)
        {
            options.AddString("-co");
            options.AddString(co.c_str());
        }
    }

    GDALTranslateOptions *translateOptions =
        GDALTranslateOptionsNew(options.List(), nullptr);

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
    workingDSHandler.reset(GDALDataset::FromHandle(GDALTranslate(
        workingFileName.c_str(), hSrcDS, translateOptions, nullptr)));
    GDALTranslateOptionsFree(translateOptions);

    if (!workingDSHandler)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Failed to create temporary dataset");
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

    // Get the output band
    GDALRasterBand *dstBand{workingDSHandler->GetRasterBand(1)};

    if (!dstBand)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Failed to get band %d from output dataset", m_band);
        return false;
    }

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

    workingDSHandler->FlushCache();

    // If we were using a temporary file we need to translate to the final desired format
    if (useTemporaryFile)
    {
        // Translate the temporary dataset to the final format
        options.Clear();
        options.AddString("-b");
        options.AddString(CPLSPrintf("%d", m_band));
        if (!m_format.empty())
        {
            options.AddString("-of");
            options.AddString(m_format.c_str());
        }
        for (const auto &co : m_creationOptions)
        {
            options.AddString("-co");
            options.AddString(co.c_str());
        }
        GDALTranslateOptions *finalTranslateOptions =
            GDALTranslateOptionsNew(options.List(), nullptr);
        workingDSHandler.reset(GDALDataset::FromHandle(
            GDALTranslate(m_outputDataset.GetName().c_str(),
                          GDALDataset::ToHandle(workingDSHandler.get()),
                          finalTranslateOptions, nullptr)));
        GDALTranslateOptionsFree(finalTranslateOptions);
    }

    m_outputDataset.Set(std::move(workingDSHandler));

    return true;
}

//! @endcond
