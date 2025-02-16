/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMutexedLayer class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef DOXYGEN_SKIP

#include "ogrmutexedlayer.h"
#include "cpl_multiproc.h"

OGRMutexedLayer::OGRMutexedLayer(OGRLayer *poDecoratedLayer, int bTakeOwnership,
                                 CPLMutex *hMutex)
    : OGRLayerDecorator(poDecoratedLayer, bTakeOwnership), m_hMutex(hMutex)
{
    SetDescription(poDecoratedLayer->GetDescription());
}

OGRMutexedLayer::~OGRMutexedLayer()
{
}

OGRGeometry *OGRMutexedLayer::GetSpatialFilter()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetSpatialFilter();
}

OGRErr OGRMutexedLayer::ISetSpatialFilter(int iGeomField,
                                          const OGRGeometry *poGeom)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::ISetSpatialFilter(iGeomField, poGeom);
}

OGRErr OGRMutexedLayer::SetAttributeFilter(const char *poAttrFilter)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetAttributeFilter(poAttrFilter);
}

void OGRMutexedLayer::ResetReading()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    OGRLayerDecorator::ResetReading();
}

OGRFeature *OGRMutexedLayer::GetNextFeature()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetNextFeature();
}

GDALDataset *OGRMutexedLayer::GetDataset()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetDataset();
}

bool OGRMutexedLayer::GetArrowStream(struct ArrowArrayStream *out_stream,
                                     CSLConstList papszOptions)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetArrowStream(out_stream, papszOptions);
}

OGRErr OGRMutexedLayer::SetNextByIndex(GIntBig nIndex)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetNextByIndex(nIndex);
}

OGRFeature *OGRMutexedLayer::GetFeature(GIntBig nFID)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetFeature(nFID);
}

OGRErr OGRMutexedLayer::ISetFeature(OGRFeature *poFeature)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::ISetFeature(poFeature);
}

OGRErr OGRMutexedLayer::ICreateFeature(OGRFeature *poFeature)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::ICreateFeature(poFeature);
}

OGRErr OGRMutexedLayer::IUpsertFeature(OGRFeature *poFeature)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::IUpsertFeature(poFeature);
}

OGRErr OGRMutexedLayer::IUpdateFeature(OGRFeature *poFeature,
                                       int nUpdatedFieldsCount,
                                       const int *panUpdatedFieldsIdx,
                                       int nUpdatedGeomFieldsCount,
                                       const int *panUpdatedGeomFieldsIdx,
                                       bool bUpdateStyleString)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::IUpdateFeature(
        poFeature, nUpdatedFieldsCount, panUpdatedFieldsIdx,
        nUpdatedGeomFieldsCount, panUpdatedGeomFieldsIdx, bUpdateStyleString);
}

OGRErr OGRMutexedLayer::DeleteFeature(GIntBig nFID)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::DeleteFeature(nFID);
}

const char *OGRMutexedLayer::GetName()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetName();
}

OGRwkbGeometryType OGRMutexedLayer::GetGeomType()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetGeomType();
}

OGRFeatureDefn *OGRMutexedLayer::GetLayerDefn()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetLayerDefn();
}

OGRSpatialReference *OGRMutexedLayer::GetSpatialRef()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetSpatialRef();
}

GIntBig OGRMutexedLayer::GetFeatureCount(int bForce)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetFeatureCount(bForce);
}

OGRErr OGRMutexedLayer::IGetExtent(int iGeomField, OGREnvelope *psExtent,
                                   bool bForce)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::IGetExtent(iGeomField, psExtent, bForce);
}

int OGRMutexedLayer::TestCapability(const char *pszCapability)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::TestCapability(pszCapability);
}

OGRErr OGRMutexedLayer::CreateField(const OGRFieldDefn *poField, int bApproxOK)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::CreateField(poField, bApproxOK);
}

OGRErr OGRMutexedLayer::DeleteField(int iField)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::DeleteField(iField);
}

OGRErr OGRMutexedLayer::ReorderFields(int *panMap)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::ReorderFields(panMap);
}

OGRErr OGRMutexedLayer::AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                       int nFlagsIn)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::AlterFieldDefn(iField, poNewFieldDefn, nFlagsIn);
}

OGRErr OGRMutexedLayer::AlterGeomFieldDefn(
    int iGeomField, const OGRGeomFieldDefn *poNewGeomFieldDefn, int nFlagsIn)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::AlterGeomFieldDefn(iGeomField, poNewGeomFieldDefn,
                                                 nFlagsIn);
}

OGRErr OGRMutexedLayer::SyncToDisk()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SyncToDisk();
}

OGRStyleTable *OGRMutexedLayer::GetStyleTable()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetStyleTable();
}

void OGRMutexedLayer::SetStyleTableDirectly(OGRStyleTable *poStyleTable)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetStyleTableDirectly(poStyleTable);
}

void OGRMutexedLayer::SetStyleTable(OGRStyleTable *poStyleTable)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetStyleTable(poStyleTable);
}

OGRErr OGRMutexedLayer::StartTransaction()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::StartTransaction();
}

OGRErr OGRMutexedLayer::CommitTransaction()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::CommitTransaction();
}

OGRErr OGRMutexedLayer::RollbackTransaction()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::RollbackTransaction();
}

const char *OGRMutexedLayer::GetFIDColumn()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetFIDColumn();
}

const char *OGRMutexedLayer::GetGeometryColumn()
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetGeometryColumn();
}

OGRErr OGRMutexedLayer::SetIgnoredFields(CSLConstList papszFields)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetIgnoredFields(papszFields);
}

char **OGRMutexedLayer::GetMetadata(const char *pszDomain)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetMetadata(pszDomain);
}

CPLErr OGRMutexedLayer::SetMetadata(char **papszMetadata, const char *pszDomain)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetMetadata(papszMetadata, pszDomain);
}

const char *OGRMutexedLayer::GetMetadataItem(const char *pszName,
                                             const char *pszDomain)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::GetMetadataItem(pszName, pszDomain);
}

CPLErr OGRMutexedLayer::SetMetadataItem(const char *pszName,
                                        const char *pszValue,
                                        const char *pszDomain)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::SetMetadataItem(pszName, pszValue, pszDomain);
}

OGRErr OGRMutexedLayer::Rename(const char *pszNewName)
{
    CPLMutexHolderOptionalLockD(m_hMutex);
    return OGRLayerDecorator::Rename(pszNewName);
}

#endif /* #ifndef DOXYGEN_SKIP */
