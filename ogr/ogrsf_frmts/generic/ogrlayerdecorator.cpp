/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRLayerDecorator class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef DOXYGEN_SKIP

#include "ogrlayerdecorator.h"
#include "ogr_recordbatch.h"

OGRLayerDecorator::OGRLayerDecorator(OGRLayer *poDecoratedLayer,
                                     int bTakeOwnership)
    : m_poDecoratedLayer(poDecoratedLayer), m_bHasOwnership(bTakeOwnership)
{
    CPLAssert(poDecoratedLayer != nullptr);
    SetDescription(poDecoratedLayer->GetDescription());
}

OGRLayerDecorator::~OGRLayerDecorator()
{
    if (m_bHasOwnership)
        delete m_poDecoratedLayer;
}

OGRGeometry *OGRLayerDecorator::GetSpatialFilter()
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetSpatialFilter();
}

OGRErr OGRLayerDecorator::ISetSpatialFilter(int iGeomField,
                                            const OGRGeometry *poGeom)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetSpatialFilter(iGeomField, poGeom);
}

OGRErr OGRLayerDecorator::SetAttributeFilter(const char *poAttrFilter)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetAttributeFilter(poAttrFilter);
}

void OGRLayerDecorator::ResetReading()
{
    if (!m_poDecoratedLayer)
        return;
    m_poDecoratedLayer->ResetReading();
}

OGRFeature *OGRLayerDecorator::GetNextFeature()
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetNextFeature();
}

GDALDataset *OGRLayerDecorator::GetDataset()
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetDataset();
}

bool OGRLayerDecorator::GetArrowStream(struct ArrowArrayStream *out_stream,
                                       CSLConstList papszOptions)
{
    if (!m_poDecoratedLayer)
    {
        memset(out_stream, 0, sizeof(*out_stream));
        return false;
    }
    return m_poDecoratedLayer->GetArrowStream(out_stream, papszOptions);
}

OGRErr OGRLayerDecorator::SetNextByIndex(GIntBig nIndex)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetNextByIndex(nIndex);
}

OGRFeature *OGRLayerDecorator::GetFeature(GIntBig nFID)
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetFeature(nFID);
}

OGRErr OGRLayerDecorator::ISetFeature(OGRFeature *poFeature)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetFeature(poFeature);
}

OGRErr OGRLayerDecorator::ICreateFeature(OGRFeature *poFeature)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->CreateFeature(poFeature);
}

OGRErr OGRLayerDecorator::IUpsertFeature(OGRFeature *poFeature)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->UpsertFeature(poFeature);
}

OGRErr OGRLayerDecorator::IUpdateFeature(OGRFeature *poFeature,
                                         int nUpdatedFieldsCount,
                                         const int *panUpdatedFieldsIdx,
                                         int nUpdatedGeomFieldsCount,
                                         const int *panUpdatedGeomFieldsIdx,
                                         bool bUpdateStyleString)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->UpdateFeature(
        poFeature, nUpdatedFieldsCount, panUpdatedFieldsIdx,
        nUpdatedGeomFieldsCount, panUpdatedGeomFieldsIdx, bUpdateStyleString);
}

OGRErr OGRLayerDecorator::DeleteFeature(GIntBig nFID)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->DeleteFeature(nFID);
}

const char *OGRLayerDecorator::GetName()
{
    if (!m_poDecoratedLayer)
        return GetDescription();
    return m_poDecoratedLayer->GetName();
}

OGRwkbGeometryType OGRLayerDecorator::GetGeomType()
{
    if (!m_poDecoratedLayer)
        return wkbNone;
    return m_poDecoratedLayer->GetGeomType();
}

OGRFeatureDefn *OGRLayerDecorator::GetLayerDefn()
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetLayerDefn();
}

OGRSpatialReference *OGRLayerDecorator::GetSpatialRef()
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetSpatialRef();
}

GIntBig OGRLayerDecorator::GetFeatureCount(int bForce)
{
    if (!m_poDecoratedLayer)
        return 0;
    return m_poDecoratedLayer->GetFeatureCount(bForce);
}

OGRErr OGRLayerDecorator::IGetExtent(int iGeomField, OGREnvelope *psExtent,
                                     bool bForce)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->GetExtent(iGeomField, psExtent, bForce);
}

OGRErr OGRLayerDecorator::IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                                       bool bForce)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->GetExtent3D(iGeomField, psExtent, bForce);
}

int OGRLayerDecorator::TestCapability(const char *pszCapability)
{
    if (!m_poDecoratedLayer)
        return FALSE;
    return m_poDecoratedLayer->TestCapability(pszCapability);
}

OGRErr OGRLayerDecorator::CreateField(const OGRFieldDefn *poField,
                                      int bApproxOK)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->CreateField(poField, bApproxOK);
}

OGRErr OGRLayerDecorator::DeleteField(int iField)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->DeleteField(iField);
}

OGRErr OGRLayerDecorator::ReorderFields(int *panMap)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->ReorderFields(panMap);
}

OGRErr OGRLayerDecorator::AlterFieldDefn(int iField,
                                         OGRFieldDefn *poNewFieldDefn,
                                         int nFlagsIn)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
}

OGRErr OGRLayerDecorator::AlterGeomFieldDefn(
    int iGeomField, const OGRGeomFieldDefn *poNewGeomFieldDefn, int nFlagsIn)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->AlterGeomFieldDefn(iGeomField,
                                                  poNewGeomFieldDefn, nFlagsIn);
}

OGRErr OGRLayerDecorator::CreateGeomField(const OGRGeomFieldDefn *poField,
                                          int bApproxOK)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->CreateGeomField(poField, bApproxOK);
}

OGRErr OGRLayerDecorator::SyncToDisk()
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->SyncToDisk();
}

OGRStyleTable *OGRLayerDecorator::GetStyleTable()
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetStyleTable();
}

void OGRLayerDecorator::SetStyleTableDirectly(OGRStyleTable *poStyleTable)
{
    if (!m_poDecoratedLayer)
        return;
    m_poDecoratedLayer->SetStyleTableDirectly(poStyleTable);
}

void OGRLayerDecorator::SetStyleTable(OGRStyleTable *poStyleTable)
{
    if (!m_poDecoratedLayer)
        return;
    m_poDecoratedLayer->SetStyleTable(poStyleTable);
}

OGRErr OGRLayerDecorator::StartTransaction()
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->StartTransaction();
}

OGRErr OGRLayerDecorator::CommitTransaction()
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->CommitTransaction();
}

OGRErr OGRLayerDecorator::RollbackTransaction()
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->RollbackTransaction();
}

const char *OGRLayerDecorator::GetFIDColumn()
{
    if (!m_poDecoratedLayer)
        return "";
    return m_poDecoratedLayer->GetFIDColumn();
}

const char *OGRLayerDecorator::GetGeometryColumn()
{
    if (!m_poDecoratedLayer)
        return "";
    return m_poDecoratedLayer->GetGeometryColumn();
}

OGRErr OGRLayerDecorator::SetIgnoredFields(CSLConstList papszFields)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    return m_poDecoratedLayer->SetIgnoredFields(papszFields);
}

char **OGRLayerDecorator::GetMetadata(const char *pszDomain)
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetMetadata(pszDomain);
}

CPLErr OGRLayerDecorator::SetMetadata(char **papszMetadata,
                                      const char *pszDomain)
{
    if (!m_poDecoratedLayer)
        return CE_Failure;
    return m_poDecoratedLayer->SetMetadata(papszMetadata, pszDomain);
}

const char *OGRLayerDecorator::GetMetadataItem(const char *pszName,
                                               const char *pszDomain)
{
    if (!m_poDecoratedLayer)
        return nullptr;
    return m_poDecoratedLayer->GetMetadataItem(pszName, pszDomain);
}

CPLErr OGRLayerDecorator::SetMetadataItem(const char *pszName,
                                          const char *pszValue,
                                          const char *pszDomain)
{
    if (!m_poDecoratedLayer)
        return CE_Failure;
    return m_poDecoratedLayer->SetMetadataItem(pszName, pszValue, pszDomain);
}

OGRErr OGRLayerDecorator::Rename(const char *pszNewName)
{
    if (!m_poDecoratedLayer)
        return OGRERR_FAILURE;
    OGRErr eErr = m_poDecoratedLayer->Rename(pszNewName);
    if (eErr == OGRERR_NONE)
    {
        SetDescription(m_poDecoratedLayer->GetDescription());
    }
    return eErr;
}

#endif /* #ifndef DOXYGEN_SKIP */
