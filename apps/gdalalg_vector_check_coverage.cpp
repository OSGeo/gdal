/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector check-coverage" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_check_coverage.h"

#include "cpl_error.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geometry.h"
#include "ogr_geos.h"

#include <cinttypes>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorCheckCoverageAlgorithm::GDALVectorCheckCoverageAlgorithm(
    bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("include-valid", 0,
           _("Include valid inputs in output, with empty geometry"),
           &m_includeValid);

    AddArg("geometry-field", 0, _("Name of geometry field to check"),
           &m_geomField);

    AddArg("maximum-gap-width", 0, _("Maximum width of a gap to be flagged"),
           &m_maximumGapWidth)
        .SetMinValueIncluded(0);
}

#if defined HAVE_GEOS &&                                                       \
    (GEOS_VERSION_MAJOR > 3 ||                                                 \
     (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 12))

class GDALVectorCheckCoverageOutputDataset final
    : public GDALGeosNonStreamingAlgorithmDataset
{
  public:
    explicit GDALVectorCheckCoverageOutputDataset(double maximumGapWidth,
                                                  bool includeValid)
        : m_maximumGapWidth(maximumGapWidth), m_includeValid(includeValid)
    {
    }

    ~GDALVectorCheckCoverageOutputDataset() override;

    bool PolygonsOnly() const override
    {
        return true;
    }

    bool SkipEmpty() const override
    {
        return !m_includeValid;
    }

    bool ProcessGeos() override
    {
        // Perform coverage checking
        GEOSGeometry *coll = GEOSGeom_createCollection_r(
            m_poGeosContext, GEOS_GEOMETRYCOLLECTION, m_apoGeosInputs.data(),
            static_cast<unsigned int>(m_apoGeosInputs.size()));

        if (coll == nullptr)
        {
            return false;
        }

        m_apoGeosInputs.clear();

        int geos_result =
            GEOSCoverageIsValid_r(m_poGeosContext, coll, m_maximumGapWidth,
                                  &m_poGeosResultAsCollection);
        GEOSGeom_destroy_r(m_poGeosContext, coll);

        CPLDebug("CoverageIsValid", "%d", geos_result);

        return geos_result != 2;
    }

  private:
    const double m_maximumGapWidth;
    const bool m_includeValid;
};

GDALVectorCheckCoverageOutputDataset::~GDALVectorCheckCoverageOutputDataset() =
    default;

bool GDALVectorCheckCoverageAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS = std::make_unique<GDALVectorCheckCoverageOutputDataset>(
        m_maximumGapWidth, m_includeValid);

    const bool bSingleLayerOutput = m_inputLayerNames.empty()
                                        ? poSrcDS->GetLayerCount() == 1
                                        : m_inputLayerNames.size() == 1;

    GDALVectorAlgorithmLayerProgressHelper progressHelper(ctxt);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_inputLayerNames.empty() ||
            std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                      poSrcLayer->GetDescription()) != m_inputLayerNames.end())
        {
            const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
            if (poSrcLayerDefn->GetGeomFieldCount() == 0)
            {
                if (m_inputLayerNames.empty())
                    continue;
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Specified layer '%s' has no geometry field",
                            poSrcLayer->GetDescription());
                return false;
            }

            progressHelper.AddProcessedLayer(*poSrcLayer);
        }
    }

    for ([[maybe_unused]] auto [poSrcLayer, bProcessed, layerProgressFunc,
                                layerProgressData] : progressHelper)
    {
        const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
        const int geomFieldIndex =
            m_geomField.empty()
                ? 0
                : poSrcLayerDefn->GetGeomFieldIndex(m_geomField.c_str());

        if (geomFieldIndex == -1)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Specified geometry field '%s' does not exist in "
                        "layer '%s'",
                        m_geomField.c_str(), poSrcLayer->GetDescription());
            return false;
        }

        OGRFeatureDefn defn(bSingleLayerOutput
                                ? "invalid_edge"
                                : std::string("invalid_edge_")
                                      .append(poSrcLayer->GetDescription())
                                      .c_str());
        defn.SetGeomType(wkbMultiLineString);
        defn.GetGeomFieldDefn(0)->SetSpatialRef(
            poSrcLayerDefn->GetGeomFieldDefn(geomFieldIndex)->GetSpatialRef());

        if (!poDstDS->AddProcessedLayer(*poSrcLayer, defn, geomFieldIndex,
                                        layerProgressFunc,
                                        layerProgressData.get()))
        {
            return false;
        }
    }

    m_outputDataset.Set(std::move(poDstDS));

    return true;
}

#else

bool GDALVectorCheckCoverageAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    ReportError(CE_Failure, CPLE_AppDefined,
                "%s requires GDAL to be built against version 3.12 or later of "
                "the GEOS library.",
                NAME);
    return false;
}
#endif  // HAVE_GEOS

GDALVectorCheckCoverageAlgorithmStandalone::
    ~GDALVectorCheckCoverageAlgorithmStandalone() = default;

//! @endcond
