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

class CPL_DLL IOGREditableLayerSynchronizer
{
  public:
    virtual ~IOGREditableLayerSynchronizer();

    virtual OGRErr EditableSyncToDisk(OGRLayer *poEditableLayer,
                                      OGRLayer **ppoDecoratedLayer) = 0;
};

class CPL_DLL OGREditableLayer : public OGRLayerDecorator
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
    virtual ~OGREditableLayer();

    void SetNextFID(GIntBig nNextFID);
    void SetSupportsCreateGeomField(bool SupportsCreateGeomField);
    void SetSupportsCurveGeometries(bool bSupportsCurveGeometries);

    virtual OGRGeometry *GetSpatialFilter() override;
    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *) override;

    virtual OGRErr SetAttributeFilter(const char *) override;
    virtual bool GetArrowStream(struct ArrowArrayStream *out_stream,
                                CSLConstList papszOptions = nullptr) override;

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;
    virtual OGRErr ISetFeature(OGRFeature *poFeature) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual OGRErr IUpsertFeature(OGRFeature *poFeature) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;
    virtual OGRErr DeleteFeature(GIntBig nFID) override;

    virtual OGRwkbGeometryType GetGeomType() override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;

    virtual int TestCapability(const char *) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr DeleteField(int iField) override;
    virtual OGRErr ReorderFields(int *panMap) override;
    virtual OGRErr AlterFieldDefn(int iField, OGRFieldDefn *poNewFieldDefn,
                                  int nFlags) override;
    virtual OGRErr
    AlterGeomFieldDefn(int iGeomField,
                       const OGRGeomFieldDefn *poNewGeomFieldDefn,
                       int nFlags) override;

    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poField,
                                   int bApproxOK = TRUE) override;

    virtual OGRErr SyncToDisk() override;

    virtual OGRErr StartTransaction() override;
    virtual OGRErr CommitTransaction() override;
    virtual OGRErr RollbackTransaction() override;

    virtual const char *GetGeometryColumn() override;
};

//! @endcond

#endif  // OGREDITABLELAYER_H_INCLUDED
