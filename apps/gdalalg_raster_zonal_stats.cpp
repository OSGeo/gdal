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
    AddVectorOutputArgs(false, false);

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
        .SetChoices("default", "fractional", "all_touched");
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
           _("Fields from polygon zones to include in ouput"),
           &m_includeFields);
    AddArg("strategy", 0,
           _("For polygon zones, whether to iterate over input features or "
             "raster chunks"),
           &m_strategy)
        .SetChoices("feature", "raster")
        .SetDefault("feature");
    AddArg("memory", 0, _("Number of pixels to use when reading raster chunks"),
           &m_memory)
        .SetDefault("2G");
    AddProgressArg();
}

/************************************************************************/
/*                 GDALRasterZonalStatsAlgorithm::RunStep()             */
/************************************************************************/

bool GDALRasterZonalStatsAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                            void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
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

                auto poDriver =
                    GetGDALDriverManager()->GetDriverByName(m_format.c_str());

                poRetDS.reset(poDriver->Create(
                    m_outputDataset.GetName().c_str(), 0, 0, 0, GDT_Unknown,
                    CPLStringList(m_creationOptions).List()));
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

    GDALZonalStatsOptions options;
    if (m_strategy == "raster")
    {
        options.strategy = GDALZonalStatsOptions::RASTER_SEQUENTIAL;
    }
    if (m_bands.empty())
    {
        int nBands = src->GetRasterCount();
        options.bands.resize(nBands);
        for (int i = 0; i < nBands; i++)
        {
            options.bands[i] = i + 1;
        }
    }
    else
    {
        options.bands = m_bands;
    }
    if (m_pixels == "fractional")
    {
        options.pixels = GDALZonalStatsOptions::FRACTIONAL;
    }
    else if (m_pixels == "all_touched")
    {
        options.pixels = GDALZonalStatsOptions::ALL_TOUCHED;
    }
    else
    {
        options.pixels = GDALZonalStatsOptions::DEFAULT;
    }
    options.stats = m_stats;
    options.include_fields = m_includeFields;
    options.zones_band = m_zonesBand;
    options.zones_layer = m_zonesLayer;
    options.weights_band = m_weightsBand;

    GIntBig maxBytes;
    if (CPLParseMemorySize(m_memory.c_str(), &maxBytes, nullptr) == CE_None)
    {
        options.memory = static_cast<size_t>(maxBytes);
    }
    else
    {
        return false;
    }

    if (poRetDS)
    {
        m_outputDataset.Set(std::move(poRetDS));
    }

    return GDALZonalStats(src, m_weights.GetDatasetRef(), zones, poDstDS,
                          &options, ctxt.m_pfnProgress,
                          ctxt.m_pProgressData) == CE_None;
}

GDALRasterZonalStatsAlgorithmStandalone::
    ~GDALRasterZonalStatsAlgorithmStandalone() = default;

//! @endcond