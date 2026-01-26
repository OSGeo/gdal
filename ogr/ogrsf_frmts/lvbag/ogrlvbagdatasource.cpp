/******************************************************************************
 *
 * Project:  LV BAG Translator
 * Purpose:  Implements OGRLVBAGDataSource.
 * Author:   Laixer B.V., info at laixer dot com
 *
 ******************************************************************************
 * Copyright (c) 2020, Laixer B.V. <info at laixer dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_lvbag.h"
#include "ogrsf_frmts.h"
#include "ogrunionlayer.h"
#include "ogrlayerpool.h"

#include <algorithm>

/************************************************************************/
/*                         OGRLVBAGDataSource()                         */
/************************************************************************/

OGRLVBAGDataSource::OGRLVBAGDataSource()
    : poPool{new OGRLayerPool{}}, papoLayers{OGRLVBAG::LayerVector{}}
{
    const int nMaxSimultaneouslyOpened =
        std::max(atoi(CPLGetConfigOption("OGR_LVBAG_MAX_OPENED", "100")), 1);
    if (poPool->GetMaxSimultaneouslyOpened() != nMaxSimultaneouslyOpened)
    {
        poPool.reset(new OGRLayerPool(nMaxSimultaneouslyOpened));
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRLVBAGDataSource::Open(const char *pszFilename, char **papszOpenOptionsIn)
{
    auto poLayer = std::unique_ptr<OGRLVBAGLayer>{
        new OGRLVBAGLayer{pszFilename, poPool.get(), papszOpenOptionsIn}};
    if (poLayer && !poLayer->TouchLayer())
    {
        return FALSE;
    }

    papoLayers.push_back({OGRLVBAG::LayerType::LYR_RAW, std::move(poLayer)});

    if ((static_cast<int>(papoLayers.size()) + 1) %
                poPool->GetMaxSimultaneouslyOpened() ==
            0 &&
        poPool->GetSize() > 0)
    {
        TryCoalesceLayers();
    }

    return TRUE;
}

/************************************************************************/
/*                         TryCoalesceLayers()                          */
/************************************************************************/

void OGRLVBAGDataSource::TryCoalesceLayers()
{
    std::vector<int> paGroup = {};
    std::map<int, std::vector<int>> paMergeVector = {};

    // FUTURE: This can be optimized
    // Find similar layers by doing a triangular matrix
    // comparison across all layers currently enlisted.
    for (size_t i = 0; i < papoLayers.size(); ++i)
    {
        std::vector<int> paVector = {};
        for (size_t j = 0; j < papoLayers.size(); ++j)
        {
            if (std::find(paGroup.cbegin(), paGroup.cend(),
                          static_cast<int>(j)) != paGroup.cend())
            {
                continue;
            }

            OGRLayer *poLayerLHS = papoLayers[i].second.get();
            OGRLayer *poLayerRHS = papoLayers[j].second.get();

            if (j > i && EQUAL(poLayerLHS->GetName(), poLayerRHS->GetName()))
            {
                if (poLayerLHS->GetGeomType() == poLayerRHS->GetGeomType() &&
                    poLayerLHS->GetLayerDefn()->IsSame(
                        poLayerRHS->GetLayerDefn()))
                {
                    paVector.push_back(static_cast<int>(j));
                    paGroup.push_back(static_cast<int>(j));
                }
            }
        }
        if (!paVector.empty())
        {
            paMergeVector.insert({static_cast<int>(i), paVector});
        }
    }

    if (paMergeVector.empty())
    {
        return;
    }

    for (const auto &mergeLayer : paMergeVector)
    {
        const int baseLayerIdx = mergeLayer.first;
        const std::vector<int> papoLayersIdx = mergeLayer.second;

        int nSrcLayers = static_cast<int>(papoLayersIdx.size()) + 1;
        OGRLayer **papoSrcLayers = static_cast<OGRLayer **>(
            CPLRealloc(nullptr, sizeof(OGRLayer *) * nSrcLayers));

        CPLAssert(papoLayers[baseLayerIdx].second);

        int idx = 0;
        papoSrcLayers[idx++] = papoLayers[baseLayerIdx].second.release();
        for (const auto &poLayerIdx : papoLayersIdx)
        {
            papoSrcLayers[idx++] = papoLayers[poLayerIdx].second.release();
        }

        OGRLayer *poBaseLayer = papoSrcLayers[0];

        auto poLayer = std::make_unique<OGRUnionLayer>(
            poBaseLayer->GetName(), nSrcLayers, papoSrcLayers, TRUE);

        OGRFeatureDefn *poBaseLayerDefn = poBaseLayer->GetLayerDefn();

        poLayer->SetFields(FIELD_FROM_FIRST_LAYER, poBaseLayerDefn);

        papoLayers.push_back({OGRLVBAG::LayerType::LYR_RAW,
                              OGRLayerUniquePtr{poLayer.release()}});
    }

    // Erase all released pointers
    auto it = papoLayers.begin();
    while (it != papoLayers.end())
    {
        if (!it->second)
        {
            it = papoLayers.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

const OGRLayer *OGRLVBAGDataSource::GetLayer(int iLayer) const
{
    if (iLayer < 0 || iLayer >= GetLayerCount())
    {
        return nullptr;
    }
    return papoLayers[iLayer].second.get();
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRLVBAGDataSource::GetLayerCount() const
{
    const_cast<OGRLVBAGDataSource *>(this)->TryCoalesceLayers();
    return static_cast<int>(papoLayers.size());
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRLVBAGDataSource::TestCapability(const char * /* pszCap */) const
{
    return FALSE;
}
