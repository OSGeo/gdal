/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101LayerInformationType
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_s101.h"

/************************************************************************/
/*                    OGRS101LayerInformationType()                     */
/************************************************************************/

OGRS101LayerInformationType::OGRS101LayerInformationType(
    OGRS101Dataset &oDS, const DDFRecordIndex &oIndex,
    OGRFeatureDefnRefCountedPtr poFeatureDefn)
    : OGRS101Layer(oDS, oIndex, std::move(poFeatureDefn))
{
}

/************************************************************************/
/*           OGRS101LayerInformationType::GetNextRawFeature()           */
/************************************************************************/

OGRFeature *OGRS101LayerInformationType::GetNextRawFeature()
{
    if (m_nRecordIdx >= m_oIndex.GetCount())
        return nullptr;
    ++m_nRecordIdx;
    return GetFeature(m_nRecordIdx);
}

/************************************************************************/
/*              OGRS101LayerInformationType::GetFeature()               */
/************************************************************************/

OGRFeature *OGRS101LayerInformationType::GetFeature(GIntBig nFID)
{
    if (nFID < 1 || nFID > m_oIndex.GetCount())
        return nullptr;
    auto poFeature = std::make_unique<OGRFeature>(m_poFeatureDefn.get());
    if (!m_oDS.GetReader().FillFeatureInformationType(
            m_oIndex, static_cast<int>(nFID) - 1, *poFeature))
    {
        return nullptr;
    }

    poFeature->SetFID(nFID);

    return poFeature.release();
}
