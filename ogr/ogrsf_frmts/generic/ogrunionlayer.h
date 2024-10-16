/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRUnionLayer class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRUNIONLAYER_H_INCLUDED
#define OGRUNIONLAYER_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrsf_frmts.h"

/************************************************************************/
/*                      OGRUnionLayerGeomFieldDefn                      */
/************************************************************************/

class CPL_DLL OGRUnionLayerGeomFieldDefn final : public OGRGeomFieldDefn
{
  public:
    int bGeomTypeSet = false;
    int bSRSSet = false;
    OGREnvelope sStaticEnvelope{};

    OGRUnionLayerGeomFieldDefn(const char *pszName, OGRwkbGeometryType eType);
    explicit OGRUnionLayerGeomFieldDefn(const OGRGeomFieldDefn *poSrc);
    explicit OGRUnionLayerGeomFieldDefn(
        const OGRUnionLayerGeomFieldDefn *poSrc);
    ~OGRUnionLayerGeomFieldDefn();
};

/************************************************************************/
/*                         OGRUnionLayer                                */
/************************************************************************/

typedef enum
{
    FIELD_FROM_FIRST_LAYER,
    FIELD_UNION_ALL_LAYERS,
    FIELD_INTERSECTION_ALL_LAYERS,
    FIELD_SPECIFIED,
} FieldUnionStrategy;

class CPL_DLL OGRUnionLayer final : public OGRLayer
{
    CPL_DISALLOW_COPY_ASSIGN(OGRUnionLayer)

  protected:
    CPLString osName{};
    int nSrcLayers = 0;
    OGRLayer **papoSrcLayers = nullptr;
    int bHasLayerOwnership = false;

    OGRFeatureDefn *poFeatureDefn = nullptr;
    int nFields = 0;
    OGRFieldDefn **papoFields = nullptr;
    int nGeomFields = 0;
    OGRUnionLayerGeomFieldDefn **papoGeomFields = nullptr;
    FieldUnionStrategy eFieldStrategy = FIELD_UNION_ALL_LAYERS;
    CPLString osSourceLayerFieldName{};

    int bPreserveSrcFID = false;

    GIntBig nFeatureCount = -1;

    int iCurLayer = -1;
    char *pszAttributeFilter = nullptr;
    int nNextFID = 0;
    int *panMap = nullptr;
    CPLStringList m_aosIgnoredFields{};
    int bAttrFilterPassThroughValue = -1;
    int *pabModifiedLayers = nullptr;
    int *pabCheckIfAutoWrap = nullptr;
    const OGRSpatialReference *poGlobalSRS = nullptr;

    void AutoWarpLayerIfNecessary(int iSubLayer);
    OGRFeature *TranslateFromSrcLayer(OGRFeature *poSrcFeature);
    void ApplyAttributeFilterToSrcLayer(int iSubLayer);
    int GetAttrFilterPassThroughValue();
    void ConfigureActiveLayer();
    void SetSpatialFilterToSourceLayer(OGRLayer *poSrcLayer);

  public:
    OGRUnionLayer(
        const char *pszName, int nSrcLayers, /* must be >= 1 */
        OGRLayer *
            *papoSrcLayers, /* array itself ownership always transferred, layer
                               ownership depending on bTakeLayerOwnership */
        int bTakeLayerOwnership);

    virtual ~OGRUnionLayer();

    /* All the following non virtual methods must be called just after the
     * constructor */
    /* and before any virtual method */
    void SetFields(
        FieldUnionStrategy eFieldStrategy, int nFields,
        OGRFieldDefn **papoFields, /* duplicated by the method */
        int nGeomFields, /* maybe -1 to explicitly disable geometry fields */
        OGRUnionLayerGeomFieldDefn *
            *papoGeomFields /* duplicated by the method */);
    void SetSourceLayerFieldName(const char *pszSourceLayerFieldName);
    void SetPreserveSrcFID(int bPreserveSrcFID);
    void SetFeatureCount(int nFeatureCount);

    virtual const char *GetName() override
    {
        return osName.c_str();
    }

    virtual OGRwkbGeometryType GetGeomType() override;

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRFeature *GetFeature(GIntBig nFeatureId) override;

    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;

    virtual OGRErr ISetFeature(OGRFeature *poFeature) override;

    virtual OGRErr IUpsertFeature(OGRFeature *poFeature) override;

    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual GIntBig GetFeatureCount(int) override;

    virtual OGRErr SetAttributeFilter(const char *) override;

    virtual int TestCapability(const char *) override;

    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce = TRUE) override;
    virtual OGRErr GetExtent(OGREnvelope *psExtent, int bForce) override;

    virtual void SetSpatialFilter(OGRGeometry *poGeomIn) override;
    virtual void SetSpatialFilter(int iGeomField, OGRGeometry *) override;

    virtual OGRErr SetIgnoredFields(CSLConstList papszFields) override;

    virtual OGRErr SyncToDisk() override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif  // OGRUNIONLAYER_H_INCLUDED
