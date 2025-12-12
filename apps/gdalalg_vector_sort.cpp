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

// Subtract 1 from the theoretical max, reserving that number for empty or
// null geometries.
constexpr uint32_t CPL_HILBERT_MAX = (1 << 16) - 2;

static std::uint32_t CPLHilbertCode(std::uint32_t x, std::uint32_t y)
{
    // Based on public domain code at
    // https://github.com/rawrunprotected/hilbert_curves

    uint32_t a = x ^ y;
    uint32_t b = 0xFFFF ^ a;
    uint32_t c = 0xFFFF ^ (x | y);
    uint32_t d = x & (y ^ 0xFFFF);

    uint32_t A = a | (b >> 1);
    uint32_t B = (a >> 1) ^ a;
    uint32_t C = ((c >> 1) ^ (b & (d >> 1))) ^ c;
    uint32_t D = ((a & (c >> 1)) ^ (d >> 1)) ^ d;

    a = A;
    b = B;
    c = C;
    d = D;
    A = ((a & (a >> 2)) ^ (b & (b >> 2)));
    B = ((a & (b >> 2)) ^ (b & ((a ^ b) >> 2)));
    C ^= ((a & (c >> 2)) ^ (b & (d >> 2)));
    D ^= ((b & (c >> 2)) ^ ((a ^ b) & (d >> 2)));

    a = A;
    b = B;
    c = C;
    d = D;
    A = ((a & (a >> 4)) ^ (b & (b >> 4)));
    B = ((a & (b >> 4)) ^ (b & ((a ^ b) >> 4)));
    C ^= ((a & (c >> 4)) ^ (b & (d >> 4)));
    D ^= ((b & (c >> 4)) ^ ((a ^ b) & (d >> 4)));

    a = A;
    b = B;
    c = C;
    d = D;
    C ^= ((a & (c >> 8)) ^ (b & (d >> 8)));
    D ^= ((b & (c >> 8)) ^ ((a ^ b) & (d >> 8)));

    a = C ^ (C >> 1);
    b = D ^ (D >> 1);

    uint32_t i0 = x ^ y;
    uint32_t i1 = b | (0xFFFF ^ (i0 | a));

    i0 = (i0 | (i0 << 8)) & 0x00FF00FF;
    i0 = (i0 | (i0 << 4)) & 0x0F0F0F0F;
    i0 = (i0 | (i0 << 2)) & 0x33333333;
    i0 = (i0 | (i0 << 1)) & 0x55555555;

    i1 = (i1 | (i1 << 8)) & 0x00FF00FF;
    i1 = (i1 | (i1 << 4)) & 0x0F0F0F0F;
    i1 = (i1 | (i1 << 2)) & 0x33333333;
    i1 = (i1 | (i1 << 1)) & 0x55555555;

    uint32_t value = ((i1 << 1) | i0);

    return value;
}

static std::uint32_t CPLHilbertCode(const OGREnvelope &oDomain, double dfX,
                                    double dfY)
{
    uint32_t x = 0;
    uint32_t y = 0;
    if (oDomain.Width() != 0.0)
        x = static_cast<uint32_t>(
            floor(CPL_HILBERT_MAX * (dfX - oDomain.MinX) / oDomain.Width()));
    if (oDomain.Height() != 0.0)
        y = static_cast<uint32_t>(
            floor(CPL_HILBERT_MAX * (dfY - oDomain.MinY) / oDomain.Height()));
    return CPLHilbertCode(x, y);
}

namespace
{

bool CreateDstFeatures(
    const std::vector<std::unique_ptr<OGRFeature>> &srcFeatures,
    const std::vector<size_t> &sortedIndices, OGRLayer &dstLayer)
{
    for (size_t iSrcFeature : sortedIndices)
    {
        OGRFeature *poSrcFeature = srcFeatures[iSrcFeature].get();
        poSrcFeature->SetFDefnUnsafe(dstLayer.GetLayerDefn());
        poSrcFeature->SetFID(OGRNullFID);

        if (dstLayer.CreateFeature(poSrcFeature) != OGRERR_NONE)
        {
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

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer,
                 int geomFieldIndex) override
    {
        std::vector<std::unique_ptr<OGRFeature>> features;

        for (auto &feature : srcLayer)
        {
            features.emplace_back(feature.release());
        }
        OGREnvelope oLayerExtent;

        std::vector<OGREnvelope> envelopes(features.size());
        for (size_t i = 0; i < features.size(); i++)
        {
            const OGRFeature *poFeature = features[i].get();
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
                hilbertCodes[i].second = CPLHilbertCode(oLayerExtent, dfX, dfY);
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

        return CreateDstFeatures(features, sortedIndices, dstLayer);
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

    bool Process(OGRLayer &srcLayer, OGRLayer &dstLayer,
                 int geomFieldIndex) override
    {
        std::vector<std::unique_ptr<OGRFeature>> features;
        std::vector<size_t> sortedIndices;

        for (auto &feature : srcLayer)
        {
            features.emplace_back(feature.release());
        }
        // TODO: variant of this fn returning unique_ptr
        m_geosContext = OGRGeometry::createGEOSContext();

        auto TreeDeleter = [this](GEOSSTRtree *tree)
        { GEOSSTRtree_destroy_r(m_geosContext, tree); };

        std::unique_ptr<GEOSSTRtree, decltype(TreeDeleter)> poTree(
            GEOSSTRtree_create_r(m_geosContext, 10), TreeDeleter);

        OGREnvelope oGeomExtent;
        std::vector<size_t> nullIndices;
        for (size_t i = 0; i < features.size(); i++)
        {
            const OGRFeature *poFeature = features[i].get();
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

        GEOSSTRtree_build_r(m_geosContext, poTree.get());

        GEOSSTRtree_iterate_r(
            m_geosContext, poTree.get(),
            [](void *item, void *userData)
            {
                static_cast<std::vector<size_t> *>(userData)->push_back(
                    reinterpret_cast<size_t>(item));
            },
            &sortedIndices);

        for (size_t nullInd : nullIndices)
        {
            sortedIndices.push_back(nullInd);
        }

        return CreateDstFeatures(features, sortedIndices, dstLayer);
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

bool GDALVectorSortAlgorithm::RunStep(GDALPipelineStepRunContext &)
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
#endif
    }

    for (auto &&poSrcLayer : poSrcDS->GetLayers())
    {
        if (m_inputLayerNames.empty() ||
            std::find(m_inputLayerNames.begin(), m_inputLayerNames.end(),
                      poSrcLayer->GetDescription()) != m_inputLayerNames.end())
        {
            const auto poSrcLayerDefn = poSrcLayer->GetLayerDefn();
            if (poSrcLayerDefn->GetGeomFieldCount() > 0)
            {
                const int geomFieldIndex =
                    m_geomField.empty() ? 0
                                        : poSrcLayerDefn->GetGeomFieldIndex(
                                              m_geomField.c_str());

                if (geomFieldIndex == -1)
                {
                    ReportError(
                        CE_Failure, CPLE_AppDefined,
                        "Specified geometry field '%s' does not exist in "
                        "layer '%s'",
                        m_geomField.c_str(), poSrcLayer->GetDescription());
                    return false;
                }

                if (!poDstDS->AddProcessedLayer(*poSrcLayer,
                                                *poSrcLayer->GetLayerDefn(),
                                                geomFieldIndex))
                {
                    return false;
                }
            }
            else if (m_inputLayerNames.empty())
            {
                poDstDS->AddPassThroughLayer(*poSrcLayer);
            }
        }
    }

    m_outputDataset.Set(std::move(poDstDS));

    return true;
}

GDALVectorSortAlgorithmStandalone::~GDALVectorSortAlgorithmStandalone() =
    default;

//! @endcond
