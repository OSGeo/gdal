/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  "gdal vector sort" subcommand
 * Author:   Daniel Baston
 *
 ******************************************************************************
 * Copyright (c) 2025-2026, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_sort.h"

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
    AddArg("use-tempfile", 0,
           _("Write features to a temporary file to avoid reading the entire "
             "input dataset into memory"),
           &m_useTempfile);
}

namespace
{

/// Simple container allowing to store features and retrieve each one _a single
/// time_ using a random access pattern. The number of stored features does not
/// need to be known at construction time.
class GDALFeatureStore
{
  public:
    virtual ~GDALFeatureStore() = default;

    virtual std::unique_ptr<OGRFeature> Load(std::size_t i) = 0;

    virtual bool Store(std::unique_ptr<OGRFeature> f) = 0;

    virtual std::size_t Size() const = 0;
};

/// FeatureStore backed by a temporary file on disk.
class GDALFileFeatureStore : public GDALFeatureStore
{
  public:
    GDALFileFeatureStore()
        : m_fileName(CPLGenerateTempFilenameSafe(nullptr)),
          m_file(VSIFOpenL(m_fileName.c_str(), "wb+"))
    {
        if (m_file == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create temporary file");
        }
        else
        {
            // Unlink immediately so that the file is cleaned up if the process is killed
            // (at least on Linux)
            VSIUnlink(m_fileName.c_str());
        }
    }

    ~GDALFileFeatureStore() override
    {
        if (m_defn != nullptr)
        {
            const_cast<OGRFeatureDefn *>(m_defn)->Release();
        }
        if (m_file != nullptr)
        {
            VSIFCloseL(m_file);
        }

        VSIUnlink(m_fileName.c_str());
    }

    std::unique_ptr<OGRFeature> Load(std::size_t i) override
    {
        auto loc = m_locs[i];
        m_buf.resize(loc.size);
        if (VSIFSeekL(m_file, loc.offset, SEEK_SET) == -1)
        {
            return nullptr;
        }
        auto nBytesRead = VSIFReadL(m_buf.data(), 1, loc.size, m_file);
        if (nBytesRead != loc.size)
        {
            return nullptr;
        }

        auto poFeature = std::make_unique<OGRFeature>(m_defn);
        if (!poFeature->DeserializeFromBinary(m_buf.data(), m_buf.size()))
        {
            return nullptr;
        }

        return poFeature;
    }

    size_t Size() const override
    {
        return m_locs.size();
    }

    bool Store(std::unique_ptr<OGRFeature> f) override
    {
        if (m_file == nullptr)
        {
            return false;
        }

        if (m_defn == nullptr)
        {
            m_defn = f->GetDefnRef();
            const_cast<OGRFeatureDefn *>(m_defn)->Reference();
        }

        if (!f->SerializeToBinary(m_buf))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to serialize feature to buffer");
            return false;
        }

        Loc loc;
        loc.offset = m_fileSize;
        loc.size = m_buf.size();
        m_locs.push_back(loc);

        auto nBytesWritten = VSIFWriteL(m_buf.data(), 1, m_buf.size(), m_file);
        if (nBytesWritten != m_buf.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to write feature to temporary file");
            return false;
        }

        m_fileSize += loc.size;

        return true;
    }

  private:
    struct Loc
    {
        vsi_l_offset offset;
        std::size_t size;
    };

    std::string m_fileName{};
    const OGRFeatureDefn *m_defn{nullptr};
    vsi_l_offset m_fileSize{0};
    VSILFILE *m_file{nullptr};
    std::vector<Loc> m_locs{};
    std::vector<GByte> m_buf{};

    CPL_DISALLOW_COPY_ASSIGN(GDALFileFeatureStore)
};

/// FeatureStore backed by a std::vector.
class GDALMemFeatureStore : public GDALFeatureStore
{
  public:
    std::unique_ptr<OGRFeature> Load(std::size_t i) override
    {
        return std::unique_ptr<OGRFeature>(m_features[i]->Clone());
    }

    size_t Size() const override
    {
        return m_features.size();
    }

    bool Store(std::unique_ptr<OGRFeature> f) override
    {
        m_features.push_back(std::move(f));
        return true;
    }

  private:
    std::vector<std::unique_ptr<OGRFeature>> m_features{};
};

/**
 * This base class provides common functionality for layers representing different
 * sorting algorithms. An implementation's Process() method should:
 * - read the input features and transfer them to the feature store
 * - populate the m_sortedIndices vector
 */
class GDALVectorSortedLayer : public GDALVectorNonStreamingAlgorithmLayer
{
  public:
    GDALVectorSortedLayer(OGRLayer &srcLayer, int geomFieldIndex,
                          bool processInMemory)
        : GDALVectorNonStreamingAlgorithmLayer(srcLayer, geomFieldIndex),
          m_store(nullptr), m_processInMemory(processInMemory), m_readPos(0)
    {
    }

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return m_srcLayer.GetLayerDefn();
    }

    int TestCapability(const char *pszCap) const override
    {
        if (EQUAL(pszCap, OLCFastFeatureCount) ||
            EQUAL(pszCap, OLCFastGetExtent) ||
            EQUAL(pszCap, OLCFastGetExtent3D) ||
            EQUAL(pszCap, OLCStringsAsUTF8) || EQUAL(pszCap, OLCIgnoreFields) ||
            EQUAL(pszCap, OLCCurveGeometries) ||
            EQUAL(pszCap, OLCMeasuredGeometries) ||
            EQUAL(pszCap, OLCZGeometries))
        {
            return m_srcLayer.TestCapability(pszCap);
        }

        return false;
    }

    GIntBig GetFeatureCount(int bForce) override
    {
        if (!m_poAttrQuery && !m_poFilterGeom)
        {
            return m_srcLayer.GetFeatureCount(bForce);
        }

        return OGRLayer::GetFeatureCount(bForce);
    }

    std::unique_ptr<OGRFeature> GetNextProcessedFeature() override
    {
        CPLAssert(m_sortedIndices.size() == m_store->Size());

        if (m_readPos < m_store->Size())
        {
            return m_store->Load(m_sortedIndices[m_readPos++]);
        }

        return nullptr;
    }

    void ResetReading() override
    {
        m_readPos = 0;
    }

  protected:
    void Init()
    {
        if (m_processInMemory)
        {
            m_store = std::make_unique<GDALMemFeatureStore>();
        }
        else
        {
            m_store = std::make_unique<GDALFileFeatureStore>();
        }

        m_readPos = 0;
    }

    std::unique_ptr<GDALFeatureStore> m_store;
    std::vector<size_t> m_sortedIndices{};

  private:
    bool m_processInMemory;
    size_t m_readPos;
};

class GDALVectorHilbertSortLayer : public GDALVectorSortedLayer
{
  public:
    using GDALVectorSortedLayer::GDALVectorSortedLayer;

    bool Process(GDALProgressFunc pfnProgress, void *pProgressData) override
    {
        Init();

        const GIntBig nLayerFeatures =
            m_srcLayer.TestCapability(OLCFastFeatureCount)
                ? m_srcLayer.GetFeatureCount(false)
                : -1;
        const double dfInvLayerFeatures =
            1.0 / std::max(1.0, static_cast<double>(nLayerFeatures));
        const double dfFirstPhaseProgressRatio =
            dfInvLayerFeatures * (2.0 / 3.0);

        std::vector<OGREnvelope> envelopes;
        OGREnvelope oLayerExtent;
        for (auto &poFeature : m_srcLayer)
        {
            const OGRGeometry *poGeom =
                poFeature->GetGeomFieldRef(m_geomFieldIndex);

            envelopes.emplace_back();

            if (poGeom != nullptr && !poGeom->IsEmpty())
            {
                poGeom->getEnvelope(&envelopes.back());
                oLayerExtent.Merge(envelopes.back());
            }

            if (!m_store->Store(
                    std::unique_ptr<OGRFeature>(poFeature.release())))
            {
                return false;
            }

            if (pfnProgress && nLayerFeatures > 0 &&
                !pfnProgress(static_cast<double>(envelopes.size()) *
                                 dfFirstPhaseProgressRatio,
                             "", pProgressData))
            {
                CPLError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
                return false;
            }
        }

        std::vector<std::pair<std::size_t, std::uint32_t>> hilbertCodes(
            envelopes.size());
        for (std::size_t i = 0; i < envelopes.size(); i++)
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

        for (const auto &sItem : hilbertCodes)
        {
            m_sortedIndices.push_back(sItem.first);
        }

        if (pfnProgress)
        {
            pfnProgress(1.0, "", pProgressData);
        }

        return true;
    }

  private:
    CPL_DISALLOW_COPY_ASSIGN(GDALVectorHilbertSortLayer)
};

#ifdef HAVE_GEOS
class GDALVectorSTRTreeSortLayer : public GDALVectorSortedLayer
{
  public:
    using GDALVectorSortedLayer::GDALVectorSortedLayer;

    ~GDALVectorSTRTreeSortLayer() override
    {
        if (m_geosContext)
        {
            if (m_poTree)
            {
                GEOSSTRtree_destroy_r(m_geosContext, m_poTree);
            }

            finishGEOS_r(m_geosContext);
        }
    }

    bool Process(GDALProgressFunc pfnProgress, void *pProgressData) override
    {
        Init();

        const GIntBig nLayerFeatures =
            m_srcLayer.TestCapability(OLCFastFeatureCount)
                ? m_srcLayer.GetFeatureCount(false)
                : -1;
        const double dfInvLayerFeatures =
            1.0 / std::max(1.0, static_cast<double>(nLayerFeatures));
        const double dfFirstPhaseProgressRatio =
            dfInvLayerFeatures * (2.0 / 3.0);

        // TODO: variant of this fn returning unique_ptr
        m_geosContext = OGRGeometry::createGEOSContext();
        m_poTree = GEOSSTRtree_create_r(m_geosContext, 10);

        OGREnvelope oGeomExtent;
        std::vector<size_t> nullIndices;
        std::uintptr_t i = 0;
        for (auto &poFeature : m_srcLayer)
        {

            const OGRGeometry *poGeom =
                poFeature->GetGeomFieldRef(m_geomFieldIndex);

            if (poGeom == nullptr || poGeom->IsEmpty())
            {
                nullIndices.push_back(i);
            }
            else
            {
                poGeom->getEnvelope(&oGeomExtent);
                if (!InsertIntoTree(oGeomExtent, i))
                {
                    return false;
                }
            }

            if (!m_store->Store(
                    std::unique_ptr<OGRFeature>(poFeature.release())))
            {
                return false;
            }

            i++;

            if (pfnProgress && nLayerFeatures > 0 &&
                !pfnProgress(static_cast<double>(i) * dfFirstPhaseProgressRatio,
                             "", pProgressData))
            {
                CPLError(CE_Failure, CPLE_UserInterrupt, "Interrupted by user");
                return false;
            }
        }

        BuildTree();

        m_sortedIndices = ReadTreeIndices();
        m_sortedIndices.insert(m_sortedIndices.end(), nullIndices.begin(),
                               nullIndices.end());

        if (pfnProgress)
        {
            pfnProgress(1.0, "", pProgressData);
        }

        return true;
    }

  private:
    bool InsertIntoTree(const OGREnvelope &oGeomExtent, std::uintptr_t i)
    {
        GEOSGeometry *poEnv = CreateGEOSEnvelope(oGeomExtent);
        if (poEnv == nullptr)
        {
            return false;
        }
        GEOSSTRtree_insert_r(m_geosContext, m_poTree, poEnv,
                             reinterpret_cast<void *>(i));
        GEOSGeom_destroy_r(m_geosContext, poEnv);

        return true;
    }

    bool BuildTree()
    {
#if GEOS_VERSION_MAJOR > 3 ||                                                  \
    (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 12)
        GEOSSTRtree_build_r(m_geosContext, m_poTree);
#else
        if (m_store->Size() > 0)
        {
            // Perform a dummy query to force tree construction.
            OGREnvelope oExtent;
            oExtent.MinX = oExtent.MaxX = oExtent.MinY = oExtent.MaxY = 0;
            GEOSGeometry *poEnv = CreateGEOSEnvelope(oExtent);
            if (poEnv == nullptr)
            {
                return false;
            }
            GEOSSTRtree_query_r(
                m_geosContext, m_poTree, poEnv, [](void *, void *) {}, nullptr);
            GEOSGeom_destroy_r(m_geosContext, poEnv);
        }
#endif
        return true;
    }

    std::vector<size_t> ReadTreeIndices()
    {
        std::vector<size_t> sortedIndices;

        GEOSSTRtree_iterate_r(
            m_geosContext, m_poTree,
            [](void *item, void *userData)
            {
                static_cast<std::vector<size_t> *>(userData)->push_back(
                    reinterpret_cast<std::uintptr_t>(item));
            },
            &sortedIndices);

        return sortedIndices;
    }

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
    GEOSSTRtree *m_poTree{nullptr};

    CPL_DISALLOW_COPY_ASSIGN(GDALVectorSTRTreeSortLayer)
};
#endif

}  // namespace

bool GDALVectorSortAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    auto poDstDS = std::make_unique<GDALVectorNonStreamingAlgorithmDataset>();

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

            std::unique_ptr<GDALVectorSortedLayer> layer;

            if (m_sortMethod == "hilbert")
            {
                layer = std::make_unique<GDALVectorHilbertSortLayer>(
                    *poSrcLayer, geomFieldIndex, !m_useTempfile);
            }
            else
            {
                // Already checked for invalid method at arg parsing stage.
                CPLAssert(m_sortMethod == "strtree");
#ifdef HAVE_GEOS
                layer = std::make_unique<GDALVectorSTRTreeSortLayer>(
                    *poSrcLayer, geomFieldIndex, !m_useTempfile);
#else
                CPLError(CE_Failure, CPLE_AppDefined,
                         "--method strtree requires a GDAL build against the "
                         "GEOS library.");
                return false;
#endif
            }

            if (!poDstDS->AddProcessedLayer(std::move(layer), layerProgressFunc,
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

GDALVectorSortAlgorithmStandalone::~GDALVectorSortAlgorithmStandalone() =
    default;

//! @endcond
