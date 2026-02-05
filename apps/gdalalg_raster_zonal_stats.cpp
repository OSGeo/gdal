/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster zonal-stats" subcommand
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_zonal_stats.h"

#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*    GDALRasterZonalStatsAlgorithm::GDALRasterZonalStatsAlgorithm()    */
/************************************************************************/

GDALRasterZonalStatsAlgorithm::GDALRasterZonalStatsAlgorithm(bool bStandalone)
    : GDALPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(bStandalone)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    AddRasterInputArgs(false, false);
    if (bStandalone)
    {
        AddVectorOutputArgs(false, false);
    }

    constexpr const char *ZONES_BAND_OR_LAYER = "BAND_OR_LAYER";

    AddBandArg(&m_bands);
    AddArg("zones", 0, _("Dataset containing zone definitions"), &m_zones)
        .SetRequired();
    AddArg("zones-band", 0, _("Band from which zones should be read"),
           &m_zonesBand)
        .SetMutualExclusionGroup(ZONES_BAND_OR_LAYER);
    AddArg("zones-layer", 0, _("Layer from which zones should be read"),
           &m_zonesLayer)
        .SetMutualExclusionGroup(ZONES_BAND_OR_LAYER);
    AddArg("weights", 0, _("Weighting raster dataset"), &m_weights);
    AddArg("weights-band", 0, _("Band from which weights should be read"),
           &m_weightsBand)
        .SetDefault(1);
    AddArg(
        "pixels", 0,
        _("Method to determine which pixels are included in stat calculation."),
        &m_pixels)
        .SetChoices("default", "fractional", "all-touched");
    AddArg("stat", 0, _("Statistic(s) to compute for each zone"), &m_stats)
        .SetRequired()
        .SetMinValueIncluded(1)
        .SetChoices("center_x", "center_y", "count", "coverage", "frac", "max",
                    "max_center_x", "max_center_y", "mean", "median", "min",
                    "minority", "min_center_x", "min_center_y", "mode", "stdev",
                    "sum", "unique", "values", "variance", "variety",
                    "weighted_mean", "weighted_stdev", "weighted_sum",
                    "weighted_variance", "weights");
    AddArg("include-field", 0,
           _("Fields from polygon zones to include in output"),
           &m_includeFields);
    AddArg("strategy", 0,
           _("For polygon zones, whether to iterate over input features or "
             "raster chunks"),
           &m_strategy)
        .SetChoices("feature", "raster")
        .SetDefault("feature");
    AddMemorySizeArg(&m_memoryBytes, &m_memoryStr, "chunk-size",
                     _("Maximum size of raster chunks read into memory"));
    AddProgressArg();
}

/************************************************************************/
/*               GDALRasterZonalStatsAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterZonalStatsAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                            void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

template <typename T>
static std::string Join(const T &vec, const std::string &separator)
{
    std::stringstream ss;
    bool first = true;
    for (const auto &val : vec)
    {
        static_assert(!std::is_floating_point_v<decltype(val)>,
                      "Precision would be lost.");

        if (first)
        {
            first = false;
        }
        else
        {
            ss << separator;
        }
        ss << val;
    }
    return ss.str();
}

bool GDALRasterZonalStatsAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    GDALDataset *zones = m_zones.GetDatasetRef();

    /// Prepare the output dataset.
    /// This section copy-pasted from gdal raster as-features.
    /// Avoid duplication.
    GDALDataset *poDstDS = m_outputDataset.GetDatasetRef();
    std::unique_ptr<GDALDataset> poRetDS;
    if (!poDstDS)
    {
        std::string outputFilename = m_outputDataset.GetName();
        if (m_standaloneStep)
        {
            if (m_format.empty())
            {
                const auto aosFormats =
                    CPLStringList(GDALGetOutputDriversForDatasetName(
                        m_outputDataset.GetName().c_str(), GDAL_OF_VECTOR,
                        /* bSingleMatch = */ true,
                        /* bWarn = */ true));
                if (aosFormats.size() != 1)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Cannot guess driver for %s",
                                m_outputDataset.GetName().c_str());
                    return false;
                }
                m_format = aosFormats[0];
            }
        }
        else
        {
            m_format = "MEM";
        }

        auto poDriver =
            GetGDALDriverManager()->GetDriverByName(m_format.c_str());
        if (!poDriver)
        {
            // shouldn't happen given checks done in GDALAlgorithm
            ReportError(CE_Failure, CPLE_AppDefined, "Cannot find driver %s",
                        m_format.c_str());
            return false;
        }

        poRetDS.reset(
            poDriver->Create(outputFilename.c_str(), 0, 0, 0, GDT_Unknown,
                             CPLStringList(m_creationOptions).List()));
        if (!poRetDS)
            return false;

        poDstDS = poRetDS.get();
    }
    /// End prep of output dataset.

    GDALDataset *src = m_inputDataset.front().GetDatasetRef();

    CPLStringList aosOptions;
    if (!m_bands.empty())
    {
        aosOptions.AddNameValue("BANDS", Join(m_bands, ",").c_str());
    }
    if (!m_includeFields.empty())
    {
        aosOptions.AddNameValue("INCLUDE_FIELDS",
                                Join(m_includeFields, ",").c_str());
    }
    aosOptions.AddNameValue("PIXEL_INTERSECTION", m_pixels.c_str());
    if (m_memoryBytes != 0)
    {
        aosOptions.AddNameValue("RASTER_CHUNK_SIZE_BYTES",
                                std::to_string(m_memoryBytes).c_str());
    }
    aosOptions.AddNameValue("STATS", Join(m_stats, ",").c_str());
    aosOptions.AddNameValue("STRATEGY", (m_strategy + "_SEQUENTIAL").c_str());
    if (m_weightsBand != 0)
    {
        aosOptions.AddNameValue("WEIGHTS_BAND",
                                std::to_string(m_weightsBand).c_str());
    }
    if (m_zonesBand != 0)
    {
        aosOptions.AddNameValue("ZONES_BAND",
                                std::to_string(m_zonesBand).c_str());
    }
    if (!m_zonesLayer.empty())
    {
        aosOptions.AddNameValue("ZONES_LAYER", m_zonesLayer.c_str());
    }
    for (const std::string &lco : m_layerCreationOptions)
    {
        aosOptions.AddString(std::string("LCO_").append(lco));
    }

    if (poRetDS)
    {
        m_outputDataset.Set(std::move(poRetDS));
    }

    return GDALZonalStats(src, m_weights.GetDatasetRef(), zones, poDstDS,
                          aosOptions.List(), ctxt.m_pfnProgress,
                          ctxt.m_pProgressData) == CE_None;
}

GDALRasterZonalStatsAlgorithmStandalone::
    ~GDALRasterZonalStatsAlgorithmStandalone() = default;

//! @endcond
