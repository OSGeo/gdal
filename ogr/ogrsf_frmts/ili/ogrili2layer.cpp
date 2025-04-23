/******************************************************************************
 *
 * Project:  Interlis 2 Translator
 * Purpose:  Implements OGRILI2Layer class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_ili2.h"

/************************************************************************/
/*                           OGRILI2Layer()                              */
/************************************************************************/

OGRILI2Layer::OGRILI2Layer(OGRFeatureDefn *poFeatureDefnIn,
                           const GeomFieldInfos &oGeomFieldInfosIn,
                           OGRILI2DataSource *poDSIn)
    : poFeatureDefn(poFeatureDefnIn), oGeomFieldInfos(oGeomFieldInfosIn),
      poDS(poDSIn)
{
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();

    listFeatureIt = listFeature.begin();
}

/************************************************************************/
/*                           ~OGRILI2Layer()                           */
/************************************************************************/

OGRILI2Layer::~OGRILI2Layer()
{
    if (poFeatureDefn)
        poFeatureDefn->Release();

    listFeatureIt = listFeature.begin();
    while (listFeatureIt != listFeature.end())
    {
        OGRFeature *poFeature = *(listFeatureIt++);
        delete poFeature;
    }
}

/************************************************************************/
/*                             AddFeature()                             */
/************************************************************************/

void OGRILI2Layer::AddFeature(OGRFeature *poFeature)
{
    poFeature->SetFID(static_cast<GIntBig>(1 + listFeature.size()));
    listFeature.push_back(poFeature);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRILI2Layer::ResetReading()
{
    listFeatureIt = listFeature.begin();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRILI2Layer::GetNextFeature()
{
    while (listFeatureIt != listFeature.end())
    {
        OGRFeature *poFeature = *(listFeatureIt++);
        // apply filters
        if ((m_poFilterGeom == nullptr ||
             FilterGeometry(poFeature->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(poFeature)))
            return poFeature->Clone();
    }
    return nullptr;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRILI2Layer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom == nullptr && m_poAttrQuery == nullptr)
    {
        return listFeature.size();
    }
    else
    {
        return OGRLayer::GetFeatureCount(bForce);
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRILI2Layer::TestCapability(CPL_UNUSED const char *pszCap)
{
    if (EQUAL(pszCap, OLCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRILI2Layer::GetDataset()
{
    return poDS;
}
