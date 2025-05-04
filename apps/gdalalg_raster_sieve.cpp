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
/*      GDALRasterSieveAlgorithm::GDALRasterSieveAlgorithm()            */
/************************************************************************/

GDALRasterSieveAlgorithm::GDALRasterSieveAlgorithm()
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
/*                 GDALRasterSieveAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALRasterSieveAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
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

    const CPLErr err = GDALSieveFilter(
        dstBand, maskBand, dstBand, m_sizeThreshold,
        m_connectDiagonalPixels ? 8 : 4, nullptr, pfnProgress, pProgressData);

    if (err != CE_None)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Failed to apply sieve filter");
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
