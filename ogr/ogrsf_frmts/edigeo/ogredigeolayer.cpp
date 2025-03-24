/******************************************************************************
 *
 * Project:  EDIGEO Translator
 * Purpose:  Implements OGREDIGEOLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_edigeo.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

/************************************************************************/
/*                          OGREDIGEOLayer()                            */
/************************************************************************/

OGREDIGEOLayer::OGREDIGEOLayer(OGREDIGEODataSource *poDSIn, const char *pszName,
                               OGRwkbGeometryType eType,
                               OGRSpatialReference *poSRSIn)
    : poDS(poDSIn), poFeatureDefn(new OGRFeatureDefn(pszName)), poSRS(poSRSIn),
      nNextFID(0)
{
    if (poSRS)
        poSRS->Reference();

    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(eType);
    if (poFeatureDefn->GetGeomFieldCount() != 0)
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    SetDescription(poFeatureDefn->GetName());
}

/************************************************************************/
/*                          ~OGREDIGEOLayer()                           */
/************************************************************************/

OGREDIGEOLayer::~OGREDIGEOLayer()

{
    for (int i = 0; i < (int)aosFeatures.size(); i++)
        delete aosFeatures[i];

    poFeatureDefn->Release();

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGREDIGEOLayer::ResetReading()

{
    nNextFID = 0;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGREDIGEOLayer::GetNextRawFeature()
{
    if (nNextFID < (int)aosFeatures.size())
    {
        OGRFeature *poFeature = aosFeatures[nNextFID]->Clone();
        nNextFID++;
        return poFeature;
    }
    else
        return nullptr;
}

/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature *OGREDIGEOLayer::GetFeature(GIntBig nFID)
{
    if (nFID >= 0 && nFID < (int)aosFeatures.size())
        return aosFeatures[(int)nFID]->Clone();
    else
        return nullptr;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGREDIGEOLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;

    else if (EQUAL(pszCap, OLCRandomRead))
        return TRUE;

    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return poDS->HasUTF8ContentOnly();

    return FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGREDIGEOLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom != nullptr || m_poAttrQuery != nullptr)
        return OGRLayer::GetFeatureCount(bForce);

    return (int)aosFeatures.size();
}

/************************************************************************/
/*                             AddFeature()                             */
/************************************************************************/

void OGREDIGEOLayer::AddFeature(OGRFeature *poFeature)
{
    poFeature->SetFID(aosFeatures.size());
    aosFeatures.push_back(poFeature);
}

/************************************************************************/
/*                         GetAttributeIndex()                          */
/************************************************************************/

int OGREDIGEOLayer::GetAttributeIndex(const CPLString &osRID)
{
    std::map<CPLString, int>::iterator itAttrIndex =
        mapAttributeToIndex.find(osRID);
    if (itAttrIndex != mapAttributeToIndex.end())
        return itAttrIndex->second;
    else
        return -1;
}

/************************************************************************/
/*                           AddFieldDefn()                             */
/************************************************************************/

void OGREDIGEOLayer::AddFieldDefn(const CPLString &osName, OGRFieldType eType,
                                  const CPLString &osRID)
{
    if (!osRID.empty())
        mapAttributeToIndex[osRID] = poFeatureDefn->GetFieldCount();

    OGRFieldDefn oFieldDefn(osName, eType);
    poFeatureDefn->AddFieldDefn(&oFieldDefn);
}
