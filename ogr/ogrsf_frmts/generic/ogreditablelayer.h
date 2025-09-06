/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGREditableLayer class
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGREDITABLELAYER_H_INCLUDED
#define OGREDITABLELAYER_H_INCLUDED

//! @cond Doxygen_Suppress
#include "ogrlayerdecorator.h"
#include <set>
#include <map>

class CPL_DLL IOGREditableLayerSynchronizer /* non final */
{
  public:
    virtual ~IOGREditableLayerSynchronizer();

    virtual OGRErr EditableSyncToDisk(OGRLayer *poEditableLayer,
                                      OGRLayer **ppoDecoratedLayer) = 0;
};

class CPL_DLL OGREditableLayer /* non final */ : public OGRLayerDecorator
{
    CPL_DISALLOW_COPY_ASSIGN(OGREditableLayer)

  protected:
    IOGREditableLayerSynchronizer *m_poSynchronizer;
    bool m_bTakeOwnershipSynchronizer;
    OGRFeatureDefn *m_poEditableFeatureDefn;
    GIntBig m_nNextFID;
    std::set<GIntBig> m_oSetCreated{};
    std::set<GIntBig> m_oSetEdited{};
    std::set<GIntBig> m_oSetDeleted{};
    std::set<GIntBig>::iterator m_oIter{};
    std::set<CPLString> m_oSetDeletedFields{};
    OGRLayer *m_poMemLayer;
    bool m_bStructureModified;
    bool m_bSupportsCreateGeomField;
    bool m_bSupportsCurveGeometries;
    std::map<CPLString, int> m_oMapEditableFDefnFieldNameToIdx{};

    OGRFeature *Translate(OGRFeatureDefn *poTargetDefn,
                          OGRFeature *poSrcFeature, bool bCanStealSrcFeature,
                          bool bHideDeletedFields);
    void DetectNextFID();
    int GetSrcGeomFieldIndex(int iGeomField);

  public:
    OGREditableLayer(OGRLayer *poDecoratedLayer,
                     bool bTakeOwnershipDecoratedLayer,
                     IOGREditableLayerSynchronizer *poSynchronizer,
                     bool bTakeOwnershipSynchronizer);
    ~OGREditableLayer() override;

    void SetNextFID(GIntBig nNextFID);
    void SetSupportsCreateGeomField(bool SupportsCreateGeomField);
    void SetSupportsCurveGeometries(bool bSupportsCurveGeometries);

    OGRGeometry *GetSpatialFilter() override;
    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *) override;

    OGRErr SetAttributeFilter(const char *) override;
    virtual bool GetArrowStream(struct ArrowArrayStream *out_stream,
                                CSLConstList papszOptions = nullptr) override;

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

    OGRwkbGeometryType GetGeomType() const override;
    using OGRLayerDecorator::GetLayerDefn;
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

    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poField,
                                   int bApproxOK = TRUE) override;

    OGRErr SyncToDisk() override;

    OGRErr StartTransaction() override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;

    const char *GetGeometryColumn() const override;
};

//! @endcond

#endif  // OGREDITABLELAYER_H_INCLUDED
