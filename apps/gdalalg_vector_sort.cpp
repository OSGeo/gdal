/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector sort" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_sort.h"

#include "cpl_enumerate.h"
#include "cpl_error.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdalalg_vector_geom.h"
#include "ogr_geometry.h"

#include "ogr_geos.h"

#include <algorithm>
#include <cinttypes>
#include <limits>

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

GDALVectorSortAlgorithm::GDALVectorSortAlgorithm(bool standaloneStep)
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("geometry-field", 0, _("Name of geometry field to use in sort"),
           &m_geomField);
    AddArg("method", 0, _("Geometry sorting algorithm"), &m_sortMethod)
        .SetChoices("hilbert", "strtree")
        .SetDefault(m_sortMethod);
}

namespace
{

bool CreateDstFeatures(std::vector<std::unique_ptr<OGRFeature>> &srcFeatures,
                       const std::vector<size_t> &sortedIndices,
                       OGRLayer &dstLayer, GDALProgressFunc pfnProgress,
                       void *pProgressData, double dfProgressStart,
                       double dfProgressRatio)
{
    uint64_t nCounter = 0;
    for (size_t iSrcFeature : sortedIndices)
    {
        auto &poSrcFeature = srcFeatures[iSrcFeature];
        poSrcFeature->SetFDefnUnsafe(dstLayer.GetLayerDefn());
        poSrcFeature->SetFID(OGRNullFID);

        if (dstLayer.CreateFeature(std::move(poSrcFeature)) != OGRERR_NONE)
        {
            return false;
        }
        if (pfnProgress &&
            !pfnProgress(dfProgressStart +
                             static_cast<double>(++nCounter) * dfProgressRatio,
                         "", pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
            return false;
        }
    }

    return true;
}

class GDALVectorHilbertSortDataset
    : public GDALVectorNonStreamingAlgorithmDataset
{
  public:
    using GDALVectorNonStreamingAlgorithmDataset::
        GDALVectorNonStreamingAlgorithmDataset;

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer, int geomFieldIndex,
                 GDALProgressFunc pfnProgress, void *pProgressData) override
    {
        std::vector<std::unique_ptr<OGRFeature>> features;

        const GIntBig nLayerFeatures =
            srcLayer.TestCapability(OLCFastFeatureCount)
                ? srcLayer.GetFeatureCount(false)
                : -1;
        const double dfInvLayerFeatures =
            1.0 / std::max(1.0, static_cast<double>(nLayerFeatures));
        const double dfFirstPhaseProgressRatio =
            dfInvLayerFeatures * (2.0 / 3.0);
        for (auto &feature : srcLayer)
        {
            features.emplace_back(feature.release());
            if (pfnProgress && nLayerFeatures > 0 &&
                !pfnProgress(static_cast<double>(features.size()) *
                                 dfFirstPhaseProgressRatio,
                             "", pProgressData))
            {
                ReportError(CE_Failure, CPLE_UserInterrupt,
                            "Interrupted by user");
                return false;
            }
        }
        OGREnvelope oLayerExtent;

        std::vector<OGREnvelope> envelopes(features.size());
        for (const auto &[i, poFeature] : cpl::enumerate(features))
        {
            const OGRGeometry *poGeom =
                poFeature->GetGeomFieldRef(geomFieldIndex);

            if (poGeom != nullptr && !poGeom->IsEmpty())
            {
                poGeom->getEnvelope(&envelopes[i]);
                oLayerExtent.Merge(envelopes[i]);
            }
        }

        std::vector<std::pair<std::size_t, std::uint32_t>> hilbertCodes(
            features.size());
        for (std::size_t i = 0; i < features.size(); i++)
        {
            hilbertCodes[i].first = i;

            if (envelopes[i].IsInit())
            {
                double dfX, dfY;
                envelopes[i].Center(dfX, dfY);
                hilbertCodes[i].second =
                    GDALHilbertCode(&oLayerExtent, dfX, dfY);
            }
            else
            {
                hilbertCodes[i].second =
                    std::numeric_limits<std::uint32_t>::max();
            }
        }

        std::sort(hilbertCodes.begin(), hilbertCodes.end(),
                  [](const auto &a, const auto &b)
                  { return a.second < b.second; });

        std::vector<size_t> sortedIndices;
        for (const auto &sItem : hilbertCodes)
        {
            sortedIndices.push_back(sItem.first);
        }

        const double dfProgressStart = nLayerFeatures > 0 ? 2.0 / 3.0 : 0.0;
        const double dfProgressRatio =
            (nLayerFeatures > 0 ? 1.0 / 3.0 : 1.0) /
            std::max(1.0, static_cast<double>(features.size()));
        return CreateDstFeatures(features, sortedIndices, dstLayer, pfnProgress,
                                 pProgressData, dfProgressStart,
                                 dfProgressRatio);
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorHilbertSortDataset)
};

#ifdef HAVE_GEOS
class GDALVectorSTRTreeSortDataset
    : public GDALVectorNonStreamingAlgorithmDataset
{
  public:
    using GDALVectorNonStreamingAlgorithmDataset::
        GDALVectorNonStreamingAlgorithmDataset;

    ~GDALVectorSTRTreeSortDataset() override
    {
        if (m_geosContext)
        {
            finishGEOS_r(m_geosContext);
        }
    }

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer, int geomFieldIndex,
                 GDALProgressFunc pfnProgress, void *pProgressData) override
    {
        std::vector<std::unique_ptr<OGRFeature>> features;
        std::vector<size_t> sortedIndices;

        const GIntBig nLayerFeatures =
            srcLayer.TestCapability(OLCFastFeatureCount)
                ? srcLayer.GetFeatureCount(false)
                : -1;
        const double dfInvLayerFeatures =
            1.0 / std::max(1.0, static_cast<double>(nLayerFeatures));
        const double dfFirstPhaseProgressRatio =
            dfInvLayerFeatures * (2.0 / 3.0);
        for (auto &feature : srcLayer)
        {
            features.emplace_back(feature.release());
            if (pfnProgress && nLayerFeatures > 0 &&
                !pfnProgress(static_cast<double>(features.size()) *
                                 dfFirstPhaseProgressRatio,
                             "", pProgressData))
            {
                ReportError(CE_Failure, CPLE_UserInterrupt,
                            "Interrupted by user");
                return false;
            }
        }
        // TODO: variant of this fn returning unique_ptr
        m_geosContext = OGRGeometry::createGEOSContext();

        auto TreeDeleter = [this](GEOSSTRtree *tree)
        { GEOSSTRtree_destroy_r(m_geosContext, tree); };

        std::unique_ptr<GEOSSTRtree, decltype(TreeDeleter)> poTree(
            GEOSSTRtree_create_r(m_geosContext, 10), TreeDeleter);

        OGREnvelope oGeomExtent;
        std::vector<size_t> nullIndices;
        for (const auto &[i, poFeature] : cpl::enumerate(features))
        {
            const OGRGeometry *poGeom =
                poFeature->GetGeomFieldRef(geomFieldIndex);

            if (poGeom == nullptr || poGeom->IsEmpty())
            {
                nullIndices.push_back(i);
                continue;
            }

            poGeom->getEnvelope(&oGeomExtent);
            GEOSGeometry *poEnv = CreateGEOSEnvelope(oGeomExtent);
            if (poEnv == nullptr)
            {
                return false;
            }

            GEOSSTRtree_insert_r(m_geosContext, poTree.get(), poEnv,
                                 reinterpret_cast<void *>(i));
            GEOSGeom_destroy_r(m_geosContext, poEnv);
        }

#if GEOS_VERSION_MAJOR > 3 ||                                                  \
    (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 12)
        GEOSSTRtree_build_r(m_geosContext, poTree.get());
#else
        if (!features.empty())
        {
            // Perform a dummy query to force tree construction.
            GEOSGeometry *poEnv = CreateGEOSEnvelope(oGeomExtent);
            GEOSSTRtree_query_r(
                m_geosContext, poTree.get(), poEnv, [](void *, void *) {},
                nullptr);
            GEOSGeom_destroy_r(m_geosContext, poEnv);
        }
#endif

        GEOSSTRtree_iterate_r(
            m_geosContext, poTree.get(),
            [](void *item, void *userData)
            {
                static_cast<std::vector<size_t> *>(userData)->push_back(
                    reinterpret_cast<size_t>(item));
            },
            &sortedIndices);

        sortedIndices.insert(sortedIndices.end(), nullIndices.begin(),
                             nullIndices.end());
        nullIndices.clear();

        const double dfProgressStart = nLayerFeatures > 0 ? 2.0 / 3.0 : 0.0;
        const double dfProgressRatio =
            (nLayerFeatures > 0 ? 1.0 / 3.0 : 1.0) /
            std::max(1.0, static_cast<double>(features.size()));
        return CreateDstFeatures(features, sortedIndices, dstLayer, pfnProgress,
                                 pProgressData, dfProgressStart,
                                 dfProgressRatio);
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSTRTreeSortDataset)

    // FIXME: Duplicated from alg/zonal.cpp.
    // Put into OGRGeometryFactory?
    GEOSGeometry *CreateGEOSEnvelope(const OGREnvelope &oEnv) const
    {
        GEOSCoordSequence *seq = GEOSCoordSeq_create_r(m_geosContext, 2, 2);
        if (seq == nullptr)
        {
            return nullptr;
        }
        GEOSCoordSeq_setXY_r(m_geosContext, seq, 0, oEnv.MinX, oEnv.MinY);
        GEOSCoordSeq_setXY_r(m_geosContext, seq, 1, oEnv.MaxX, oEnv.MaxY);
        return GEOSGeom_createLineString_r(m_geosContext, seq);
    }

    GEOSContextHandle_t m_geosContext{nullptr};
};
#endif

}  // namespace

bool GDALVectorSortAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    std::unique_ptr<GDALVectorNonStreamingAlgorithmDataset> poDstDS;

    if (m_sortMethod == "hilbert")
    {
        poDstDS = std::make_unique<GDALVectorHilbertSortDataset>();
    }
    else
    {
        // Already checked for invalid method at arg parsing stage.
        CPLAssert(m_sortMethod == "strtree");
#ifdef HAVE_GEOS
        poDstDS = std::make_unique<GDALVectorSTRTreeSortDataset>();
#else
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "--method strtree requires a GDAL build against the GEOS library.");
        return false;
#endif
    }

    GDALVectorAlgorithmLayerProgressHelper progressHelper(ctxt);

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_inputLayerNames.empty() ||
            std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                      poSrcLayer->GetDescription()) != m_inputLayerNames.end())
        {
            const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
            if (poSrcLayerDefn->GetGeomFieldCount() > 0)
            {
                progressHelper.AddProcessedLayer(*poSrcLayer);
            }
            else
            {
                progressHelper.AddPassThroughLayer(*poSrcLayer);
            }
        }
    }

    for (auto [poSrcLayer, bProcessed, layerProgressFunc, layerProgressData] :
         progressHelper)
    {
        if (bProcessed)
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

            if (!poDstDS->AddProcessedLayer(
                    *poSrcLayer, *poSrcLayer->GetLayerDefn(), geomFieldIndex,
                    layerProgressFunc, layerProgressData.get()))
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

GDALVectorSortAlgorithmStandalone::~GDALVectorSortAlgorithmStandalone() =
    default;

//! @endcond
