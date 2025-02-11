/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMutexedDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef DOXYGEN_SKIP

#include "ogrmutexeddatasource.h"
#include "cpl_multiproc.h"

OGRMutexedDataSource::OGRMutexedDataSource(GDALDataset *poBaseDataSource,
                                           int bTakeOwnership,
                                           CPLMutex *hMutexIn,
                                           int bWrapLayersInMutexedLayer)
    : m_poBaseDataSource(poBaseDataSource), m_bHasOwnership(bTakeOwnership),
      m_hGlobalMutex(hMutexIn),
      m_bWrapLayersInMutexedLayer(bWrapLayersInMutexedLayer)
{
    SetDescription(poBaseDataSource->GetDescription());
    poDriver = poBaseDataSource->GetDriver();
}

OGRMutexedDataSource::~OGRMutexedDataSource()
{
    std::map<OGRLayer *, OGRMutexedLayer *>::iterator oIter =
        m_oMapLayers.begin();
    for (; oIter != m_oMapLayers.end(); ++oIter)
        delete oIter->second;

    if (m_bHasOwnership)
        delete m_poBaseDataSource;
}

int OGRMutexedDataSource::GetLayerCount()
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetLayerCount();
}

OGRLayer *OGRMutexedDataSource::WrapLayerIfNecessary(OGRLayer *poLayer)
{
    if (poLayer && m_bWrapLayersInMutexedLayer)
    {
        OGRLayer *poWrappedLayer = m_oMapLayers[poLayer];
        if (poWrappedLayer)
            poLayer = poWrappedLayer;
        else
        {
            OGRMutexedLayer *poMutexedLayer =
                new OGRMutexedLayer(poLayer, FALSE, m_hGlobalMutex);
            m_oMapLayers[poLayer] = poMutexedLayer;
            m_oReverseMapLayers[poMutexedLayer] = poLayer;
            poLayer = poMutexedLayer;
        }
    }
    return poLayer;
}

OGRLayer *OGRMutexedDataSource::GetLayer(int iIndex)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return WrapLayerIfNecessary(m_poBaseDataSource->GetLayer(iIndex));
}

OGRLayer *OGRMutexedDataSource::GetLayerByName(const char *pszName)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return WrapLayerIfNecessary(m_poBaseDataSource->GetLayerByName(pszName));
}

OGRErr OGRMutexedDataSource::DeleteLayer(int iIndex)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    OGRLayer *poLayer =
        m_bWrapLayersInMutexedLayer ? GetLayer(iIndex) : nullptr;
    OGRErr eErr = m_poBaseDataSource->DeleteLayer(iIndex);
    if (eErr == OGRERR_NONE && poLayer)
    {
        std::map<OGRLayer *, OGRMutexedLayer *>::iterator oIter =
            m_oMapLayers.find(poLayer);
        if (oIter != m_oMapLayers.end())
        {
            delete oIter->second;
            m_oReverseMapLayers.erase(oIter->second);
            m_oMapLayers.erase(oIter);
        }
    }
    return eErr;
}

bool OGRMutexedDataSource::IsLayerPrivate(int iLayer) const
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->IsLayerPrivate(iLayer);
}

int OGRMutexedDataSource::TestCapability(const char *pszCap)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->TestCapability(pszCap);
}

OGRLayer *
OGRMutexedDataSource::ICreateLayer(const char *pszName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return WrapLayerIfNecessary(m_poBaseDataSource->CreateLayer(
        pszName, poGeomFieldDefn, papszOptions));
}

OGRLayer *OGRMutexedDataSource::CopyLayer(OGRLayer *poSrcLayer,
                                          const char *pszNewName,
                                          char **papszOptions)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return WrapLayerIfNecessary(
        m_poBaseDataSource->CopyLayer(poSrcLayer, pszNewName, papszOptions));
}

OGRStyleTable *OGRMutexedDataSource::GetStyleTable()
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetStyleTable();
}

void OGRMutexedDataSource::SetStyleTableDirectly(OGRStyleTable *poStyleTable)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    m_poBaseDataSource->SetStyleTableDirectly(poStyleTable);
}

void OGRMutexedDataSource::SetStyleTable(OGRStyleTable *poStyleTable)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    m_poBaseDataSource->SetStyleTable(poStyleTable);
}

OGRLayer *OGRMutexedDataSource::ExecuteSQL(const char *pszStatement,
                                           OGRGeometry *poSpatialFilter,
                                           const char *pszDialect)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return WrapLayerIfNecessary(m_poBaseDataSource->ExecuteSQL(
        pszStatement, poSpatialFilter, pszDialect));
}

void OGRMutexedDataSource::ReleaseResultSet(OGRLayer *poResultsSet)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    if (poResultsSet && m_bWrapLayersInMutexedLayer)
    {
        std::map<OGRMutexedLayer *, OGRLayer *>::iterator oIter =
            m_oReverseMapLayers.find(
                dynamic_cast<OGRMutexedLayer *>(poResultsSet));
        CPLAssert(oIter != m_oReverseMapLayers.end());
        delete poResultsSet;
        poResultsSet = oIter->second;
        m_oMapLayers.erase(poResultsSet);
        m_oReverseMapLayers.erase(oIter);
    }

    m_poBaseDataSource->ReleaseResultSet(poResultsSet);
}

CPLErr OGRMutexedDataSource::FlushCache(bool bAtClosing)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->FlushCache(bAtClosing);
}

OGRErr OGRMutexedDataSource::StartTransaction(int bForce)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->StartTransaction(bForce);
}

OGRErr OGRMutexedDataSource::CommitTransaction()
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->CommitTransaction();
}

OGRErr OGRMutexedDataSource::RollbackTransaction()
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->RollbackTransaction();
}

char **OGRMutexedDataSource::GetMetadata(const char *pszDomain)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetMetadata(pszDomain);
}

CPLErr OGRMutexedDataSource::SetMetadata(char **papszMetadata,
                                         const char *pszDomain)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->SetMetadata(papszMetadata, pszDomain);
}

const char *OGRMutexedDataSource::GetMetadataItem(const char *pszName,
                                                  const char *pszDomain)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetMetadataItem(pszName, pszDomain);
}

CPLErr OGRMutexedDataSource::SetMetadataItem(const char *pszName,
                                             const char *pszValue,
                                             const char *pszDomain)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->SetMetadataItem(pszName, pszValue, pszDomain);
}

std::vector<std::string>
OGRMutexedDataSource::GetFieldDomainNames(CSLConstList papszOptions) const
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetFieldDomainNames(papszOptions);
}

const OGRFieldDomain *
OGRMutexedDataSource::GetFieldDomain(const std::string &name) const
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetFieldDomain(name);
}

bool OGRMutexedDataSource::AddFieldDomain(
    std::unique_ptr<OGRFieldDomain> &&domain, std::string &failureReason)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->AddFieldDomain(std::move(domain), failureReason);
}

bool OGRMutexedDataSource::DeleteFieldDomain(const std::string &name,
                                             std::string &failureReason)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->DeleteFieldDomain(name, failureReason);
}

bool OGRMutexedDataSource::UpdateFieldDomain(
    std::unique_ptr<OGRFieldDomain> &&domain, std::string &failureReason)
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->UpdateFieldDomain(std::move(domain),
                                                 failureReason);
}

std::vector<std::string>
OGRMutexedDataSource::GetRelationshipNames(CSLConstList papszOptions) const
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetRelationshipNames(papszOptions);
}

const GDALRelationship *
OGRMutexedDataSource::GetRelationship(const std::string &name) const
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetRelationship(name);
}

std::shared_ptr<GDALGroup> OGRMutexedDataSource::GetRootGroup() const
{
    CPLMutexHolderOptionalLockD(m_hGlobalMutex);
    return m_poBaseDataSource->GetRootGroup();
}

#endif /* #ifndef DOXYGEN_SKIP */
