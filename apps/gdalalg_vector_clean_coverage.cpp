/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector clean-coverage" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_clean_coverage.h"

#include "cpl_error.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geos.h"
#include "ogrsf_frmts.h"

#include <cinttypes>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorCleanCoverageAlgorithm::GDALVectorCleanCoverageAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);
    AddArg("snapping-distance", 0, _("Distance tolerance for snapping nodes"),
           &m_opts.snappingTolerance)
        .SetMinValueIncluded(0);

    AddArg("merge-strategy", 0,
           _("Algorithm to assign overlaps to neighboring polygons"),
           &m_opts.mergeStrategy)
        .SetChoices({"longest-border", "max-area", "min-area", "min-index"});

    AddArg("maximum-gap-width", 0, _("Maximum width of a gap to be closed"),
           &m_opts.maximumGapWidth)
        .SetMinValueIncluded(0);
}

#if defined HAVE_GEOS &&                                                       \
    (GEOS_VERSION_MAJOR > 3 ||                                                 \
     (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 14))

class GDALVectorCleanCoverageOutputLayer final
    : public GDALGeosNonStreamingAlgorithmLayer
{
  public:
    GDALVectorCleanCoverageOutputLayer(
        OGRLayer &srcLayer, int geomFieldIndex,
        const GDALVectorCleanCoverageAlgorithm::Options &opts)
        : GDALGeosNonStreamingAlgorithmLayer(srcLayer, geomFieldIndex),
          m_opts(opts), m_cleanParams(GetCoverageCleanParams())
    {
    }

    ~GDALVectorCleanCoverageOutputLayer() override;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_srcLayer.GetLayerDefn();
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCFastFeatureCount))
        {
            return m_srcLayer.TestCapability(pszCap);
        }

        return false;
    }

    GEOSCoverageCleanParams *GetCoverageCleanParams() const
    {
        GEOSCoverageCleanParams *params =
            GEOSCoverageCleanParams_create_r(m_poGeosContext);

        if (!params)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create coverage clean parameters");
            return nullptr;
        }

        if (!GEOSCoverageCleanParams_setSnappingDistance_r(
                m_poGeosContext, params, m_opts.snappingTolerance))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to set snapping tolerance");
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return nullptr;
        }

        if (!GEOSCoverageCleanParams_setGapMaximumWidth_r(
                m_poGeosContext, params, m_opts.maximumGapWidth))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to set maximum gap width");
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return nullptr;
        }

        int mergeStrategy;
        if (m_opts.mergeStrategy == "longest-border")
        {
            mergeStrategy = GEOS_MERGE_LONGEST_BORDER;
        }
        else if (m_opts.mergeStrategy == "max-area")
        {
            mergeStrategy = GEOS_MERGE_MAX_AREA;
        }
        else if (m_opts.mergeStrategy == "min-area")
        {
            mergeStrategy = GEOS_MERGE_MIN_AREA;
        }
        else if (m_opts.mergeStrategy == "min-index")
        {
            mergeStrategy = GEOS_MERGE_MIN_INDEX;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown overlap merge strategy: %s",
                     m_opts.mergeStrategy.c_str());
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return nullptr;
        }

        if (!GEOSCoverageCleanParams_setOverlapMergeStrategy_r(
                m_poGeosContext, params, mergeStrategy))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to set overlap merge strategy");
            GEOSCoverageCleanParams_destroy_r(m_poGeosContext, params);
            return nullptr;
        }

        return params;
    }

    bool PolygonsOnly() const override
    {
        return true;
    }

    bool SkipEmpty() const override
    {
        return false;
    }

    bool ProcessGeos() override
    {
        // Perform coverage cleaning
        GEOSGeometry *coll = GEOSGeom_createCollection_r(
            m_poGeosContext, GEOS_GEOMETRYCOLLECTION, m_apoGeosInputs.data(),
            static_cast<unsigned int>(m_apoGeosInputs.size()));

        if (coll == nullptr)
        {
            return false;
        }

        m_apoGeosInputs.clear();

        m_poGeosResultAsCollection =
            GEOSCoverageCleanWithParams_r(m_poGeosContext, coll, m_cleanParams);
        GEOSGeom_destroy_r(m_poGeosContext, coll);

        return m_poGeosResultAsCollection != nullptr;
    }

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorCleanCoverageOutputLayer)

  private:
    const GDALVectorCleanCoverageAlgorithm::Options &m_opts;
    GEOSCoverageCleanParams *m_cleanParams;
};

GDALVectorCleanCoverageOutputLayer::~GDALVectorCleanCoverageOutputLayer()
{
    if (m_poGeosContext != nullptr)
    {
        GEOSCoverageCleanParams_destroy_r(m_poGeosContext, m_cleanParams);
    }
}

bool GDALVectorCleanCoverageAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS = std::make_unique<GDALVectorNonStreamingAlgorithmDataset>();

    GDALVectorAlgorithmLayerProgressHelper progressHelper(ctxt);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_activeLayer.empty() ||
            m_activeLayer == poSrcLayer->GetDescription())
        {
            progressHelper.AddProcessedLayer(*poSrcLayer);
        }
        else
        {
            progressHelper.AddPassThroughLayer(*poSrcLayer);
        }
    }

    if (!progressHelper.HasProcessedLayers())
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Specified layer '%s' was not found",
                    m_activeLayer.c_str());
        return false;
    }

    for (auto [poSrcLayer, bProcessed, layerProgressFunc, layerProgressData] :
         progressHelper)
    {
        if (bProcessed)
        {
            constexpr int geomFieldIndex = 0;  // TODO parametrize

            auto poLayer = std::make_unique<GDALVectorCleanCoverageOutputLayer>(
                *poSrcLayer, geomFieldIndex, m_opts);

            if (!poDstDS->AddProcessedLayer(std::move(poLayer),
                                            layerProgressFunc,
                                            layerProgressData.get()))
            {
                return false;
            }
        }
        else
        {
            poDstDS->AddPassThroughLayer(*poSrcLayer);
        }
    }

    m_outputDataset.Set(std::move(poDstDS));

    return true;
}

#else

bool GDALVectorCleanCoverageAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    ReportError(CE_Failure, CPLE_AppDefined,
                "%s requires GDAL to be built against version 3.14 or later of "
                "the GEOS library.",
                NAME);
    return false;
}
#endif  // HAVE_GEOS

GDALVectorCleanCoverageAlgorithmStandalone::
    ~GDALVectorCleanCoverageAlgorithmStandalone() = default;

//! @endcond
