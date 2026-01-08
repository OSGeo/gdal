/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLMutexedDataSource class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRMUTEXEDDATASOURCELAYER_H_INCLUDED
#define OGRMUTEXEDDATASOURCELAYER_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrsf_frmts.h"
#include "cpl_multiproc.h"
#include "ogrmutexedlayer.h"
#include <map>

/** OGRMutexedDataSource class protects all virtual methods of GDALDataset,
 *  related to vector layers, with a mutex.
 *  If the passed mutex is NULL, then no locking will be done.
 *
 *  Note that the constructors and destructors are not explicitly protected
 *  by the mutex.
 */
class CPL_DLL OGRMutexedDataSource final : public GDALDataset
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMutexedDataSource)

  protected:
    GDALDataset *m_poBaseDataSource = nullptr;
    int m_bHasOwnership = false;
    CPLMutex *m_hGlobalMutex = nullptr;
    int m_bWrapLayersInMutexedLayer = false;
    std::map<OGRLayer *, OGRMutexedLayer *> m_oMapLayers{};
    std::map<OGRMutexedLayer *, OGRLayer *> m_oReverseMapLayers{};

    OGRLayer *WrapLayerIfNecessary(OGRLayer *poLayer);

  public:
    /* The construction of the object isn't protected by the mutex */
    OGRMutexedDataSource(GDALDataset *poBaseDataSource, int bTakeOwnership,
                         CPLMutex *hMutexIn, int bWrapLayersInMutexedLayer);

    /* The destruction of the object isn't protected by the mutex */
    ~OGRMutexedDataSource() override;

    GDALDataset *GetBaseDataSource()
    {
        return m_poBaseDataSource;
    }

    int GetLayerCount() const override;
    using GDALDataset::GetLayer;
    const OGRLayer *GetLayer(int) const override;
    OGRLayer *GetLayerByName(const char *) override;
    OGRErr DeleteLayer(int) override;
    bool IsLayerPrivate(int iLayer) const override;

    int TestCapability(const char *) const override;

    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   const OGRGeomFieldDefn *poGeomFieldDefn,
                                   CSLConstList papszOptions) override;
    virtual OGRLayer *CopyLayer(OGRLayer *poSrcLayer, const char *pszNewName,
                                char **papszOptions = nullptr) override;

    OGRStyleTable *GetStyleTable() override;
    void SetStyleTableDirectly(OGRStyleTable *poStyleTable) override;

    void SetStyleTable(OGRStyleTable *poStyleTable) override;

    OGRLayer *ExecuteSQL(const char *pszStatement, OGRGeometry *poSpatialFilter,
                         const char *pszDialect) override;
    void ReleaseResultSet(OGRLayer *poResultsSet) override;

    CPLErr FlushCache(bool bAtClosing) override;

    OGRErr StartTransaction(int bForce = FALSE) override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;

    CSLConstList GetMetadata(const char *pszDomain = "") override;
    CPLErr SetMetadata(CSLConstList papszMetadata,
                       const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "") override;

    virtual std::vector<std::string>
    GetFieldDomainNames(CSLConstList papszOptions = nullptr) const override;
    virtual const OGRFieldDomain *
    GetFieldDomain(const std::string &name) const override;

    virtual bool AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                                std::string &failureReason) override;
    virtual bool DeleteFieldDomain(const std::string &name,
                                   std::string &failureReason) override;
    virtual bool UpdateFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                                   std::string &failureReason) override;

    std::vector<std::string>
    GetRelationshipNames(CSLConstList papszOptions = nullptr) const override;

    const GDALRelationship *
    GetRelationship(const std::string &name) const override;

    std::shared_ptr<GDALGroup> GetRootGroup() const override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif  // OGRMUTEXEDDATASOURCELAYER_H_INCLUDED
