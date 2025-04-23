/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRSQLiteSingleFeatureLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_sqlite.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_feature.h"

/************************************************************************/
/*                    OGRSQLiteSingleFeatureLayer()                     */
/************************************************************************/

OGRSQLiteSingleFeatureLayer::OGRSQLiteSingleFeatureLayer(
    const char *pszLayerName, int nValIn)
    : nVal(nValIn), pszVal(nullptr),
      poFeatureDefn(new OGRFeatureDefn("SELECT")), iNextShapeId(0)
{
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    OGRFieldDefn oField(pszLayerName, OFTInteger);
    poFeatureDefn->AddFieldDefn(&oField);
}

/************************************************************************/
/*                    OGRSQLiteSingleFeatureLayer()                     */
/************************************************************************/

OGRSQLiteSingleFeatureLayer::OGRSQLiteSingleFeatureLayer(
    const char *pszLayerName, const char *pszValIn)
    : nVal(0), pszVal(CPLStrdup(pszValIn)),
      poFeatureDefn(new OGRFeatureDefn("SELECT")), iNextShapeId(0)
{
    poFeatureDefn->Reference();
    OGRFieldDefn oField(pszLayerName, OFTString);
    poFeatureDefn->AddFieldDefn(&oField);
}

/************************************************************************/
/*                   ~OGRSQLiteSingleFeatureLayer()                     */
/************************************************************************/

OGRSQLiteSingleFeatureLayer::~OGRSQLiteSingleFeatureLayer()
{
    if (poFeatureDefn != nullptr)
    {
        poFeatureDefn->Release();
        poFeatureDefn = nullptr;
    }
    CPLFree(pszVal);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRSQLiteSingleFeatureLayer::ResetReading()
{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRSQLiteSingleFeatureLayer::GetNextFeature()
{
    if (iNextShapeId != 0)
        return nullptr;

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    if (pszVal)
        poFeature->SetField(0, pszVal);
    else
        poFeature->SetField(0, nVal);
    poFeature->SetFID(iNextShapeId++);
    return poFeature;
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn *OGRSQLiteSingleFeatureLayer::GetLayerDefn()
{
    return poFeatureDefn;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRSQLiteSingleFeatureLayer::TestCapability(const char *)
{
    return FALSE;
}
