/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector simplify-coverage" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_simplify_coverage.h"

#include "cpl_error.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geometry.h"
#include "ogr_geos.h"
#include "ogrsf_frmts.h"

#include <cinttypes>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorSimplifyCoverageAlgorithm::GDALVectorSimplifyCoverageAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddActiveLayerArg(&m_activeLayer);
    AddArg("tolerance", 0, _("Distance tolerance for simplification."),
           &m_opts.tolerance)
        .SetPositional()
        .SetRequired()
        .SetMinValueIncluded(0);
    AddArg("preserve-boundary", 0,
           _("Whether the exterior boundary should be preserved."),
           &m_opts.preserveBoundary);
}

#if defined HAVE_GEOS &&                                                       \
    (GEOS_VERSION_MAJOR > 3 ||                                                 \
     (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 12))

class GDALVectorSimplifyCoverageOutputDataset final
    : public GDALGeosNonStreamingAlgorithmDataset
{
  public:
    GDALVectorSimplifyCoverageOutputDataset(
        const GDALVectorSimplifyCoverageAlgorithm::Options &opts)
        : m_opts(opts)
    {
    }

    ~GDALVectorSimplifyCoverageOutputDataset() override;

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
        // Perform coverage simplification
        GEOSGeometry *coll = GEOSGeom_createCollection_r(
            m_poGeosContext, GEOS_GEOMETRYCOLLECTION, m_apoGeosInputs.data(),
            static_cast<unsigned int>(m_apoGeosInputs.size()));

        if (coll == nullptr)
        {
            return false;
        }

        m_apoGeosInputs.clear();

        m_poGeosResultAsCollection = GEOSCoverageSimplifyVW_r(
            m_poGeosContext, coll, m_opts.tolerance, m_opts.preserveBoundary);
        GEOSGeom_destroy_r(m_poGeosContext, coll);

        return m_poGeosResultAsCollection != nullptr;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSimplifyCoverageOutputDataset)

    const GDALVectorSimplifyCoverageAlgorithm::Options &m_opts;
};

GDALVectorSimplifyCoverageOutputDataset::
    ~GDALVectorSimplifyCoverageOutputDataset() = default;

bool GDALVectorSimplifyCoverageAlgorithm::RunStep(
    GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS =
        std::make_unique<GDALVectorSimplifyCoverageOutputDataset>(m_opts);

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
            if (!poDstDS->AddProcessedLayer(*poSrcLayer, layerProgressFunc,
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

bool GDALVectorSimplifyCoverageAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    ReportError(CE_Failure, CPLE_AppDefined,
                "%s requires GDAL to be built against version 3.12 or later of "
                "the GEOS library.",
                NAME);
    return false;
}
#endif  // HAVE_GEOS

GDALVectorSimplifyCoverageAlgorithmStandalone::
    ~GDALVectorSimplifyCoverageAlgorithmStandalone() = default;

//! @endcond
