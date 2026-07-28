/******************************************************************************
 *
 * Project:  S-101 driver
 * Purpose:  Implements OGRS101Layer
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_s101.h"

/************************************************************************/
/*                     OGRS101Layer::OGRS101Layer()                     */
/************************************************************************/

OGRS101Layer::OGRS101Layer(OGRS101Dataset &oDS, const DDFRecordIndex &oIndex,
                           OGRFeatureDefnRefCountedPtr poFeatureDefn)
    : m_oDS(oDS), m_oIndex(oIndex), m_poFeatureDefn(std::move(poFeatureDefn))
{
    SetDescription(m_poFeatureDefn->GetName());
}

/************************************************************************/
/*                    OGRS101Layer::~OGRS101Layer()                     */
/************************************************************************/

OGRS101Layer::~OGRS101Layer() = default;

/************************************************************************/
/*                     OGRS101Layer::ResetReading()                     */
/************************************************************************/

void OGRS101Layer::ResetReading()
{
    m_nRecordIdx = 0;
}

/************************************************************************/
/*                    OGRS101Layer::TestCapability()                    */
/************************************************************************/

int OGRS101Layer::TestCapability(const char *pszCap) const
{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poAttrQuery == nullptr && m_poFilterGeom == nullptr;

    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return true;

    if (EQUAL(pszCap, OLCZGeometries))
    {
        const auto poSRS = GetSpatialRef();
        if (poSRS)
            return poSRS->IsCompound();
    }

    return false;
}

/************************************************************************/
/*                   OGRS101Layer::GetFeatureCount()                    */
/************************************************************************/

GIntBig OGRS101Layer::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery || m_poFilterGeom)
        return OGRLayer::GetFeatureCount(bForce);
    return m_oIndex.GetCount();
}
