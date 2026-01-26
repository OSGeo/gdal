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

#include "gdal_alg.h"
#include "gdal_priv.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*     GDALRasterProximityAlgorithm::GDALRasterProximityAlgorithm()     */
/************************************************************************/

GDALRasterProximityAlgorithm::GDALRasterProximityAlgorithm(bool standaloneStep)
    : GDALRasterPipelineNonNativelyStreamingAlgorithm(NAME, DESCRIPTION,
                                                      HELP_URL, standaloneStep)
{
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
/*               GDALRasterProximityAlgorithm::RunStep()                */
/************************************************************************/

bool GDALRasterProximityAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;

    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    GDALDataType outputType = GDT_Float32;
    if (!m_outputDataType.empty())
    {
        outputType = GDALGetDataTypeByName(m_outputDataType.c_str());
    }

    auto poTmpDS = CreateTemporaryDataset(
        poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(), 1, outputType,
        /* bTiledIfPossible = */ true, poSrcDS, /* bCopyMetadata = */ false);
    if (!poTmpDS)
        return false;

    const auto srcBand = poSrcDS->GetRasterBand(m_inputBand);
    CPLAssert(srcBand);

    const auto dstBand = poTmpDS->GetRasterBand(1);
    CPLAssert(dstBand);

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
    if (error == CE_None)
    {
        if (pfnProgress)
            pfnProgress(1.0, "", pProgressData);
        m_outputDataset.Set(std::move(poTmpDS));
    }

    return error == CE_None;
}

GDALRasterProximityAlgorithmStandalone::
    ~GDALRasterProximityAlgorithmStandalone() = default;

//! @endcond
