/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101LayerPoint
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_s101.h"

/************************************************************************/
/*                         OGRS101LayerPoint()                          */
/************************************************************************/

OGRS101LayerPoint::OGRS101LayerPoint(OGRS101Dataset &oDS,
                                     const DDFRecordIndex &oIndex,
                                     const std::vector<int> &anRecordIndices,
                                     OGRFeatureDefnRefCountedPtr poFeatureDefn)
    : OGRS101Layer(oDS, oIndex, std::move(poFeatureDefn)),
      m_anRecordIndices(anRecordIndices)
{
}

/************************************************************************/
/*                OGRS101LayerPoint::GetNextRawFeature()                */
/************************************************************************/

OGRFeature *OGRS101LayerPoint::GetNextRawFeature()
{
    if (m_nRecordIdx >= static_cast<int>(m_anRecordIndices.size()))
        return nullptr;
    ++m_nRecordIdx;
    return GetFeature(m_nRecordIdx);
}

/************************************************************************/
/*                 OGRS101LayerPoint::GetFeatureCount()                 */
/************************************************************************/

GIntBig OGRS101LayerPoint::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery || m_poFilterGeom)
        return OGRLayer::GetFeatureCount(bForce);
    return static_cast<GIntBig>(m_anRecordIndices.size());
}

/************************************************************************/
/*                   OGRS101LayerPoint::GetFeature()                    */
/************************************************************************/

OGRFeature *OGRS101LayerPoint::GetFeature(GIntBig nFID)
{
    if (nFID < 1 || nFID > static_cast<GIntBig>(m_anRecordIndices.size()))
        return nullptr;
    auto poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn.get());
    if (!m_oDS.GetReader().FillFeaturePoint(
            m_oIndex, m_anRecordIndices[static_cast<int>(nFID) - 1],
            *poFeature))
    {
        return nullptr;
    }
    poFeature->SetFID(nFID);

    return poFeature.release();
}
