/******************************************************************************
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM layer class.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/
#include "gnm.h"
#include "gnm_priv.h"

CPL_CVSID("$Id$")

/**
 * GNMGenericLayer
 */
GNMGenericLayer::GNMGenericLayer(OGRLayer* poLayer,
                                 GNMGenericNetwork* poNetwork) :
    OGRLayer(),
    m_soLayerName( poLayer->GetName() ),
    m_poLayer( poLayer ),
    m_poNetwork( poNetwork )
{
}

/**
 * ~GNMGenericLayer
 */
GNMGenericLayer::~GNMGenericLayer() {}

const char *GNMGenericLayer::GetFIDColumn()
{
    return GNM_SYSFIELD_GFID;
}

const char *GNMGenericLayer::GetGeometryColumn()
{
    return m_poLayer->GetGeometryColumn();
}

OGRErr GNMGenericLayer::SetIgnoredFields(const char **papszFields)
{
    return m_poLayer->SetIgnoredFields(papszFields);
}

OGRErr GNMGenericLayer::Intersection(OGRLayer *pLayerMethod,
                                     OGRLayer *pLayerResult,
                                     char **papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressArg)
{
    return m_poLayer->Intersection(pLayerMethod, pLayerResult, papszOptions,
                                   pfnProgress, pProgressArg);
}

OGRErr GNMGenericLayer::Union(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                              char **papszOptions, GDALProgressFunc pfnProgress,
                              void *pProgressArg)
{
    return m_poLayer->Union(pLayerMethod, pLayerResult, papszOptions,
                                   pfnProgress, pProgressArg);
}

OGRErr GNMGenericLayer::SymDifference(OGRLayer *pLayerMethod,
                                      OGRLayer *pLayerResult, char **papszOptions,
                                      GDALProgressFunc pfnProgress, void *pProgressArg)
{
    return m_poLayer->Union(pLayerMethod, pLayerResult, papszOptions,
                                   pfnProgress, pProgressArg);
}

OGRErr GNMGenericLayer::Identity(OGRLayer *pLayerMethod,
                                 OGRLayer *pLayerResult, char **papszOptions,
                                 GDALProgressFunc pfnProgress, void *pProgressArg)
{
    return m_poLayer->Union(pLayerMethod, pLayerResult, papszOptions,
                                   pfnProgress, pProgressArg);
}

OGRErr GNMGenericLayer::Update(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                               char **papszOptions, GDALProgressFunc pfnProgress,
                               void *pProgressArg)
{
    return m_poLayer->Update(pLayerMethod, pLayerResult, papszOptions,
                                   pfnProgress, pProgressArg);
}

OGRErr GNMGenericLayer::Clip(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                             char **papszOptions, GDALProgressFunc pfnProgress,
                             void *pProgressArg)
{
    return m_poLayer->Clip(pLayerMethod, pLayerResult, papszOptions,
                                   pfnProgress, pProgressArg);
}

OGRErr GNMGenericLayer::Erase(OGRLayer *pLayerMethod, OGRLayer *pLayerResult,
                              char **papszOptions, GDALProgressFunc pfnProgress,
                              void *pProgressArg)
{
    return m_poLayer->Erase(pLayerMethod, pLayerResult, papszOptions,
                                   pfnProgress, pProgressArg);
}

GIntBig GNMGenericLayer::GetFeaturesRead()
{
    return m_poLayer->GetFeaturesRead();
}

int GNMGenericLayer::AttributeFilterEvaluationNeedsGeometry()
{
    return m_poLayer->AttributeFilterEvaluationNeedsGeometry();
}

//! @cond Doxygen_Suppress
OGRErr GNMGenericLayer::InitializeIndexSupport(const char *pszVal)
{
    return m_poLayer->InitializeIndexSupport(pszVal);
}

OGRLayerAttrIndex *GNMGenericLayer::GetIndex()
{
    return m_poLayer->GetIndex();
}

OGRErr GNMGenericLayer::ISetFeature(OGRFeature *poFeature)
{
    VALIDATE_POINTER1(poFeature, "GNMGenericLayer::ISetFeature", CE_Failure);
    std::map<GNMGFID, GIntBig>::iterator it = m_mnFIDMap.find(poFeature->GetFID());
    if (it == m_mnFIDMap.end())
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "The FID " CPL_FRMT_GIB " is invalid",
                  poFeature->GetFID() );
        return OGRERR_NON_EXISTING_FEATURE;
    }

    // TODO: check connection rules if feature can be changed.

    poFeature->SetFID(it->second);
    return m_poLayer->SetFeature(poFeature);
}

OGRErr GNMGenericLayer::ICreateFeature(OGRFeature *poFeature)
{
    VALIDATE_POINTER1(poFeature, "GNMGenericLayer::ICreateFeature", CE_Failure);
    GNMGFID nFID = m_poNetwork->GetNewGlobalFID();
    poFeature->SetFID(nFID);
    poFeature->SetField(GNM_SYSFIELD_GFID, nFID);
    poFeature->SetField(GNM_SYSFIELD_BLOCKED, GNM_BLOCK_NONE);
    if(m_poNetwork->AddFeatureGlobalFID(nFID, GetName()) != CE_None)
        return OGRERR_FAILURE;
    return m_poLayer->CreateFeature(poFeature);
}
//! @endcond

OGRGeometry *GNMGenericLayer::GetSpatialFilter()
{
    return m_poLayer->GetSpatialFilter();
}

void GNMGenericLayer::SetSpatialFilter(OGRGeometry *poGeometry)
{
    m_poLayer->SetSpatialFilter(poGeometry);
}

void GNMGenericLayer::SetSpatialFilterRect(double dfMinX, double dfMinY,
                                           double dfMaxX, double dfMaxY)
{
    m_poLayer->SetSpatialFilterRect(dfMinX, dfMinY, dfMaxX, dfMaxY);
}

void GNMGenericLayer::SetSpatialFilter(int iGeomField, OGRGeometry *poGeometry)
{
    m_poLayer->SetSpatialFilter(iGeomField ,poGeometry);
}

void GNMGenericLayer::SetSpatialFilterRect(int iGeomField,
                                           double dfMinX, double dfMinY,
                                           double dfMaxX, double dfMaxY)
{
    m_poLayer->SetSpatialFilterRect(iGeomField, dfMinX, dfMinY, dfMaxX, dfMaxY);
}

OGRErr GNMGenericLayer::SetAttributeFilter(const char *pszFilter)
{
    return m_poLayer->SetAttributeFilter(pszFilter);
}

void GNMGenericLayer::ResetReading()
{
    m_poLayer->ResetReading();
}

OGRFeature *GNMGenericLayer::GetNextFeature()
{
    OGRFeature* pFeature = m_poLayer->GetNextFeature();
    if(NULL == pFeature)
        return NULL;
    GNMGFID nGFID = pFeature->GetFieldAsGNMGFID(GNM_SYSFIELD_GFID);
    m_mnFIDMap[nGFID] = pFeature->GetFID();
    pFeature->SetFID(nGFID);
    return pFeature;
}

OGRErr GNMGenericLayer::SetNextByIndex(GIntBig nIndex)
{
    return m_poLayer->SetNextByIndex(nIndex);
}

OGRErr GNMGenericLayer::DeleteFeature(GIntBig nFID)
{
    OGRFeature *poFeature = GetFeature(nFID);
    if(NULL == poFeature)
        return CE_Failure;

    nFID = poFeature->GetFID();
    std::map<GNMGFID, GIntBig>::iterator it = m_mnFIDMap.find(nFID);
    if (it == m_mnFIDMap.end())
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "The FID " CPL_FRMT_GIB " is invalid",
                  nFID );
        return OGRERR_NON_EXISTING_FEATURE;
    }

    OGRFeature::DestroyFeature(poFeature);

    //delete from graph
    if(m_poNetwork->DisconnectFeaturesWithId((GNMGFID)nFID) !=
            CE_None)
        return CE_Failure;

    return m_poLayer->DeleteFeature(it->second);
}

const char *GNMGenericLayer::GetName()
{
    return m_soLayerName;
}

OGRwkbGeometryType GNMGenericLayer::GetGeomType()
{
    return m_poLayer->GetGeomType();
}

int GNMGenericLayer::FindFieldIndex(const char *pszFieldName, int bExactMatch)
{
    return m_poLayer->FindFieldIndex(pszFieldName, bExactMatch);
}

OGRSpatialReference *GNMGenericLayer::GetSpatialRef()
{
    return m_poLayer->GetSpatialRef();
}

GIntBig GNMGenericLayer::GetFeatureCount(int bForce)
{
    return m_poLayer->GetFeatureCount(bForce);
}

OGRErr GNMGenericLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    return m_poLayer->GetExtent(psExtent, bForce);
}

OGRErr GNMGenericLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
{
    return m_poLayer->GetExtent(iGeomField, psExtent, bForce);
}

int GNMGenericLayer::TestCapability(const char *pszCapability)
{
    return m_poLayer->TestCapability(pszCapability);
}

OGRErr GNMGenericLayer::CreateField(OGRFieldDefn *poField, int bApproxOK)
{
    return m_poLayer->CreateField(poField, bApproxOK);
}

OGRErr GNMGenericLayer::DeleteField(int iField)
{
    if(iField == FindFieldIndex(GNM_SYSFIELD_GFID, TRUE))
        return OGRERR_UNSUPPORTED_OPERATION;
    if(iField == FindFieldIndex(GNM_SYSFIELD_BLOCKED, TRUE))
        return OGRERR_UNSUPPORTED_OPERATION;
    return m_poLayer->DeleteField(iField);
}

OGRErr GNMGenericLayer::ReorderFields(int *panMap)
{
    return m_poLayer->ReorderFields(panMap);
}

OGRErr GNMGenericLayer::AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn, int nFlagsIn)
{
    if(iField == FindFieldIndex(GNM_SYSFIELD_GFID, TRUE))
        return OGRERR_UNSUPPORTED_OPERATION;
    if(iField == FindFieldIndex(GNM_SYSFIELD_BLOCKED, TRUE))
        return OGRERR_UNSUPPORTED_OPERATION;
    return m_poLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
}

OGRErr GNMGenericLayer::CreateGeomField(OGRGeomFieldDefn *poField, int bApproxOK)
{
    return m_poLayer->CreateGeomField(poField, bApproxOK);
}

OGRErr GNMGenericLayer::SyncToDisk()
{
    return m_poLayer->SyncToDisk();
}

OGRStyleTable *GNMGenericLayer::GetStyleTable()
{
    return m_poLayer->GetStyleTable();
}

void GNMGenericLayer::SetStyleTableDirectly(OGRStyleTable *poStyleTable)
{
    return m_poLayer->SetStyleTableDirectly(poStyleTable);
}

void GNMGenericLayer::SetStyleTable(OGRStyleTable *poStyleTable)
{
    return m_poLayer->SetStyleTable(poStyleTable);
}

OGRErr GNMGenericLayer::StartTransaction()
{
    return m_poLayer->StartTransaction();
}

OGRErr GNMGenericLayer::CommitTransaction()
{
    return m_poLayer->CommitTransaction();
}

OGRErr GNMGenericLayer::RollbackTransaction()
{
    return m_poLayer->RollbackTransaction();
}

OGRFeatureDefn *GNMGenericLayer::GetLayerDefn()
{
    //TODO: hide GNM_SYSFIELD_GFID filed
    return m_poLayer->GetLayerDefn();
}
