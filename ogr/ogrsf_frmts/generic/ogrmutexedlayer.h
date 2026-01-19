/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLMutexedLayer class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRMUTEXEDLAYER_H_INCLUDED
#define OGRMUTEXEDLAYER_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrlayerdecorator.h"
#include "cpl_multiproc.h"

/** OGRMutexedLayer class protects all virtual methods of OGRLayer with a mutex.
 *
 *  If the passed mutex is NULL, then no locking will be done.
 *
 *  Note that the constructors and destructors are not explicitly protected
 *  by the mutex.
 */
class CPL_DLL OGRMutexedLayer final : public OGRLayerDecorator
{
    CPL_DISALLOW_COPY_ASSIGN(OGRMutexedLayer)

  protected:
    CPLMutex *m_hMutex = nullptr;

  public:
    /* The construction of the object isn't protected by the mutex */
    OGRMutexedLayer(OGRLayer *poDecoratedLayer, int bTakeOwnership,
                    CPLMutex *hMutex);

    /* The destruction of the object isn't protected by the mutex */
    ~OGRMutexedLayer() override;

    OGRGeometry *GetSpatialFilter() override;
    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *) override;

    OGRErr SetAttributeFilter(const char *) override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRErr SetNextByIndex(GIntBig nIndex) override;
    OGRFeature *GetFeature(GIntBig nFID) override;
    OGRErr ISetFeature(OGRFeature *poFeature) override;
    OGRErr ICreateFeature(OGRFeature *poFeature) override;
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

    CSLConstList GetMetadata(const char *pszDomain = "") override;
    CPLErr SetMetadata(CSLConstList papszMetadata,
                       const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "") override;
    OGRErr Rename(const char *pszNewName) override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif  // OGRMUTEXEDLAYER_H_INCLUDED
