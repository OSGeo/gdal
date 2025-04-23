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
    virtual ~OGRLayerDecorator();

    virtual OGRGeometry *GetSpatialFilter() override;
    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *) override;

    virtual OGRErr SetAttributeFilter(const char *) override;

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;
    virtual OGRErr SetNextByIndex(GIntBig nIndex) override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;
    virtual OGRErr ISetFeature(OGRFeature *poFeature) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual OGRErr IUpsertFeature(OGRFeature *poFeature) override;
    virtual OGRErr DeleteFeature(GIntBig nFID) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;

    virtual GDALDataset *GetDataset() override;
    virtual bool GetArrowStream(struct ArrowArrayStream *out_stream,
                                CSLConstList papszOptions = nullptr) override;

    virtual const char *GetName() override;
    virtual OGRwkbGeometryType GetGeomType() override;
    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce = true) override;
    virtual OGRErr IGetExtent3D(int iGeomField, OGREnvelope3D *psExtent,
                                bool bForce = true) override;

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

    virtual OGRStyleTable *GetStyleTable() override;
    virtual void SetStyleTableDirectly(OGRStyleTable *poStyleTable) override;

    virtual void SetStyleTable(OGRStyleTable *poStyleTable) override;

    virtual OGRErr StartTransaction() override;
    virtual OGRErr CommitTransaction() override;
    virtual OGRErr RollbackTransaction() override;

    virtual const char *GetFIDColumn() override;
    virtual const char *GetGeometryColumn() override;

    virtual OGRErr SetIgnoredFields(CSLConstList papszFields) override;

    virtual char **GetMetadata(const char *pszDomain = "") override;
    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    virtual const char *GetMetadataItem(const char *pszName,
                                        const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;
    virtual OGRErr Rename(const char *pszNewName) override;

    OGRLayer *GetBaseLayer() const
    {
        return m_poDecoratedLayer;
    }
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif  // OGRLAYERDECORATOR_H_INCLUDED
