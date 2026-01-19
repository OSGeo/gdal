/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLayerPool and OGRProxiedLayer class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRLAYERPOOL_H_INCLUDED
#define OGRLAYERPOOL_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrsf_frmts.h"

#include <mutex>

typedef OGRLayer *(*OpenLayerFunc)(void *user_data);
typedef void (*ReleaseLayerFunc)(OGRLayer *, void *user_data);
typedef void (*FreeUserDataFunc)(void *user_data);

class OGRLayerPool;

/************************************************************************/
/*                      OGRAbstractProxiedLayer                         */
/************************************************************************/

class CPL_DLL OGRAbstractProxiedLayer : public OGRLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRAbstractProxiedLayer)

    friend class OGRLayerPool;

    OGRAbstractProxiedLayer
        *poPrevLayer; /* Chain to a layer that was used more recently */
    OGRAbstractProxiedLayer
        *poNextLayer; /* Chain to a layer that was used less recently */

  protected:
    OGRLayerPool *poPool;

    virtual void CloseUnderlyingLayer() = 0;

  public:
    explicit OGRAbstractProxiedLayer(OGRLayerPool *poPool);
    ~OGRAbstractProxiedLayer() override;
};

/************************************************************************/
/*                             OGRLayerPool                             */
/************************************************************************/

class CPL_DLL OGRLayerPool
{
    CPL_DISALLOW_COPY_ASSIGN(OGRLayerPool)

  protected:
    OGRAbstractProxiedLayer *poMRULayer; /* the most recently used layer */
    OGRAbstractProxiedLayer
        *poLRULayer;  /* the least recently used layer (still opened) */
    int nMRUListSize; /* the size of the list */
    int nMaxSimultaneouslyOpened;

  public:
    explicit OGRLayerPool(int nMaxSimultaneouslyOpened = 100);
    ~OGRLayerPool();

    void SetLastUsedLayer(OGRAbstractProxiedLayer *poProxiedLayer);
    void UnchainLayer(OGRAbstractProxiedLayer *poProxiedLayer);

    int GetMaxSimultaneouslyOpened() const
    {
        return nMaxSimultaneouslyOpened;
    }

    int GetSize() const
    {
        return nMRUListSize;
    }
};

/************************************************************************/
/*                           OGRProxiedLayer                            */
/************************************************************************/

class CPL_DLL OGRProxiedLayer : public OGRAbstractProxiedLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRProxiedLayer)

    OpenLayerFunc pfnOpenLayer;
    ReleaseLayerFunc pfnReleaseLayer;
    FreeUserDataFunc pfnFreeUserData;
    void *pUserData;
    mutable OGRLayer *poUnderlyingLayer;
    mutable OGRFeatureDefn *poFeatureDefn;
    mutable OGRSpatialReference *poSRS;
    mutable std::recursive_mutex m_oMutex{};

    int OpenUnderlyingLayer() const;

  protected:
    void CloseUnderlyingLayer() override;

  public:
    OGRProxiedLayer(OGRLayerPool *poPool, OpenLayerFunc pfnOpenLayer,
                    FreeUserDataFunc pfnFreeUserData, void *pUserData);
    OGRProxiedLayer(OGRLayerPool *poPool, OpenLayerFunc pfnOpenLayer,
                    ReleaseLayerFunc pfnReleaseLayer,
                    FreeUserDataFunc pfnFreeUserData, void *pUserData);
    ~OGRProxiedLayer() override;

    OGRLayer *GetUnderlyingLayer();

    OGRGeometry *GetSpatialFilter() override;
    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *) override;

    OGRErr SetAttributeFilter(const char *) override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRErr SetNextByIndex(GIntBig nIndex) override;
    OGRFeature *GetFeature(GIntBig nFID) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ISetFeatureUniqPtr(std::unique_ptr<OGRFeature> poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeatureUniqPtr(std::unique_ptr<OGRFeature> poFeature,
                                 GIntBig *pnFID) override;
    OGRErr IUpsertFeature(OGRFeature *poFeature) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;
    OGRErr DeleteFeature(GIntBig nFID) override;

    GDALDataset *GetDataset() override;
    virtual bool GetArrowStream(struct ArrowArrayStream *out_stream,
                                CSLConstList papszOptions = nullptr) override;

    const char *GetName() const override;
    OGRwkbGeometryType GetGeomType() const override;
    const OGRFeatureDefn *GetLayerDefn() const override;

    const OGRSpatialReference *GetSpatialRef() const override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    int TestCapability(const char *) const override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    OGRErr DeleteField(int iField) override;
    OGRErr ReorderFields(int *panMap) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                  int nFlags) override;
    virtual OGRErr
    AlterGeomFieldDefn(int iGeomField,
                       const OGRGeomFieldDefn *poNewGeomFieldDefn,
                       int nFlags) override;

    OGRErr SyncToDisk() override;

    OGRStyleTable *GetStyleTable() override;
    void SetStyleTableDirectly(OGRStyleTable *poStyleTable) override;

    void SetStyleTable(OGRStyleTable *poStyleTable) override;

    OGRErr StartTransaction() override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;

    const char *GetFIDColumn() const override;
    const char *GetGeometryColumn() const override;

    OGRErr SetIgnoredFields(CSLConstList papszFields) override;

    OGRErr Rename(const char *pszNewName) override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif  // OGRLAYERPOOL_H_INCLUDED
