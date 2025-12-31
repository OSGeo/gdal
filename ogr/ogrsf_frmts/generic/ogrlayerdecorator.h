/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLayerDecorator class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRLAYERDECORATOR_H_INCLUDED
#define OGRLAYERDECORATOR_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrsf_frmts.h"

class CPL_DLL OGRLayerDecorator : virtual public OGRLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRLayerDecorator)

  protected:
    OGRLayer *m_poDecoratedLayer;
    int m_bHasOwnership;

  public:
    OGRLayerDecorator(OGRLayer *poDecoratedLayer, int bTakeOwnership);
    ~OGRLayerDecorator() override;

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
    OGRErr DeleteFeature(GIntBig nFID) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;

    GDALDataset *GetDataset() override;
    virtual bool GetArrowStream(struct ArrowArrayStream *out_stream,
                                CSLConstList papszOptions = nullptr) override;

    const char *GetName() const override;
    OGRwkbGeometryType GetGeomType() const override;
    using OGRLayer::GetLayerDefn;
    const OGRFeatureDefn *GetLayerDefn() const override;

    const OGRSpatialReference *GetSpatialRef() const override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce = true) override;
    OGRErr IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                        bool bForce = true) override;

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

    OGRStyleTable *GetStyleTable() override;
    void SetStyleTableDirectly(OGRStyleTable *poStyleTable) override;

    void SetStyleTable(OGRStyleTable *poStyleTable) override;

    OGRErr StartTransaction() override;
    OGRErr CommitTransaction() override;
    OGRErr RollbackTransaction() override;

    const char *GetFIDColumn() const override;
    const char *GetGeometryColumn() const override;

    OGRErr SetIgnoredFields(CSLConstList papszFields) override;

    char **GetMetadata(const char *pszDomain = "") override;
    CPLErr SetMetadata(char **papszMetadata,
                       const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                           const char *pszDomain = "") override;
    OGRErr Rename(const char *pszNewName) override;

    OGRLayer *GetBaseLayer() const
    {
        return m_poDecoratedLayer;
    }
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif  // OGRLAYERDECORATOR_H_INCLUDED
