/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster proximity" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_proximity.h"

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
/*      GDALRasterProximityAlgorithm::GDALRasterProximityAlgorithm()            */
/************************************************************************/

GDALRasterProximityAlgorithm::GDALRasterProximityAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{

    AddProgressArg();

    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER);
    AddOutputFormatArg(&m_outputFormat, /* bStreamAllowed = */ false,
                       /* bGDALGAllowed = */ false)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_RASTER, GDAL_DCAP_CREATE});
    AddCreationOptionsArg(&m_creationOptions);
    AddOverwriteArg(&m_overwrite);
    AddOutputDataTypeArg(&m_outputDataType)
        .SetChoices("Byte", "UInt16", "Int16", "UInt32", "Int32", "Float32",
                    "Float64")
        .SetDefault(m_outputDataType);
    AddBandArg(&m_inputBand);
    AddArg("target-values", 0, _("Target pixel values"), &m_targetPixelValues);
    AddArg("distance-units", 0, _("Distance units"), &m_distanceUnits)
        .SetChoices("pixel", "geo")
        .SetDefault(m_distanceUnits);
    AddArg("max-distance", 0,
           _("Maximum distance. The nodata value will be used for pixels "
             "beyond this distance"),
           &m_maxDistance)
        .SetDefault(m_maxDistance);
    AddArg("fixed-value", 0,
           _("Fixed value for the pixels that are beyond the "
             "maximum distance (instead of the actual distance)"),
           &m_fixedBufferValue)
        .SetMinValueIncluded(0)
        .SetDefault(m_fixedBufferValue);
    AddArg("nodata", 0,
           _("Specify a nodata value to use for pixels that are beyond the "
             "maximum distance"),
           &m_noDataValue);
}

/************************************************************************/
/*                 GDALRasterProximityAlgorithm::RunImpl()              */
/************************************************************************/

bool GDALRasterProximityAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{

    auto srcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(srcDS);

    const auto srcBand = srcDS->GetRasterBand(m_inputBand);
    // Check done by GDALAlgorithm::AddBandArg()
    CPLAssert(srcBand);

    const std::string outputFormat{
        m_outputFormat.empty()
            ? GetOutputDriverForRaster(m_outputDataset.GetName().c_str())
            : m_outputFormat};

    const auto drv =
        GDALDriver::FromHandle(GDALGetDriverByName(outputFormat.c_str()));
    if (!drv)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Output driver '%s' not found.", outputFormat.c_str());
        return false;
    }
    // Create the output dataset with the specified type
    CPLStringList creationOptions{m_creationOptions};

    GDALDataType outputType = GDT_Float32;
    if (!m_outputDataType.empty())
    {
        outputType = GDALGetDataTypeByName(m_outputDataType.c_str());
    }

    std::unique_ptr<GDALDataset> dstDs{GDALDataset::FromHandle(drv->Create(
        m_outputDataset.GetName().c_str(), srcBand->GetXSize(),
        srcBand->GetYSize(), 1, outputType, creationOptions.List()))};

    if (!dstDs)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Failed to create output dataset '%s'.",
                    m_outputDataset.GetName().c_str());
        return false;
    }

    double adfGeoTransform[6];
    if (srcDS->GetGeoTransform(adfGeoTransform) == CE_None)
    {
        dstDs->SetGeoTransform(adfGeoTransform);
    }

    dstDs->SetSpatialRef(srcDS->GetSpatialRef());

    const auto dstBand = dstDs->GetRasterBand(1);

    // Build options for GDALComputeProximity
    CPLStringList proximityOptions;

    if (GetArg("max-distance")->IsExplicitlySet())
    {
        proximityOptions.AddString(CPLSPrintf("MAXDIST=%.17g", m_maxDistance));
    }

    if (GetArg("distance-units")->IsExplicitlySet())
    {
        proximityOptions.AddString(
            CPLSPrintf("DISTUNITS=%s", m_distanceUnits.c_str()));
    }

    if (GetArg("fixed-value")->IsExplicitlySet())
    {
        proximityOptions.AddString(
            CPLSPrintf("FIXED_BUF_VAL=%.17g", m_fixedBufferValue));
    }

    if (GetArg("nodata")->IsExplicitlySet())
    {
        proximityOptions.AddString(CPLSPrintf("NODATA=%.17g", m_noDataValue));
        dstBand->SetNoDataValue(m_noDataValue);
    }

    // Always set this to YES. Note that this was NOT the
    // default behavior in the python implementation of the utility.
    proximityOptions.AddString("USE_INPUT_NODATA=YES");

    if (GetArg("target-values")->IsExplicitlySet())
    {
        std::string targetPixelValues;
        for (const auto &value : m_targetPixelValues)
        {
            if (!targetPixelValues.empty())
                targetPixelValues += ",";
            targetPixelValues += CPLSPrintf("%.17g", value);
        }
        proximityOptions.AddString(
            CPLSPrintf("VALUES=%s", targetPixelValues.c_str()));
    }

    const auto error = GDALComputeProximity(srcBand, dstBand, proximityOptions,
                                            pfnProgress, pProgressData);

    if (error != CE_None)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Failed to compute proximity map.");
        return false;
    }

    m_outputDataset.Set(std::move(dstDs));

    return true;
}

//! @endcond
