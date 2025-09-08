/******************************************************************************
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

#include <algorithm>
#include <mutex>
#include <utility>

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
    ~OGRUnionLayerGeomFieldDefn() override;
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
  private:
    CPL_DISALLOW_COPY_ASSIGN(OGRUnionLayer)

    struct Layer
    {
        std::unique_ptr<OGRLayer> poLayerKeeper{};
        OGRLayer *poLayer = nullptr;
        bool bModified = false;
        bool bCheckIfAutoWrap = false;

        CPL_DISALLOW_COPY_ASSIGN(Layer)

        Layer(OGRLayer *poLayerIn, bool bOwnedIn)
            : poLayerKeeper(bOwnedIn ? poLayerIn : nullptr),
              poLayer(bOwnedIn ? poLayerKeeper.get() : poLayerIn)
        {
        }

        Layer(Layer &&) = default;
        Layer &operator=(Layer &&) = default;

        OGRLayer *operator->()
        {
            return poLayer;
        }

        const OGRLayer *operator->() const
        {
            return poLayer;
        }

        std::pair<OGRLayer *, bool> release()
        {
            const bool bOwnedBackup = poLayerKeeper != nullptr;
            OGRLayer *poLayerBackup =
                poLayerKeeper ? poLayerKeeper.release() : poLayer;
            poLayerKeeper.reset();
            return std::make_pair(poLayerBackup, bOwnedBackup);
        }

        void reset(std::unique_ptr<OGRLayer> poLayerIn)
        {
            poLayerKeeper = std::move(poLayerIn);
            poLayer = poLayerKeeper.get();
        }
    };

    CPLString osName{};

    std::vector<Layer> m_apoSrcLayers{};

    mutable OGRFeatureDefn *poFeatureDefn = nullptr;
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
    mutable int bAttrFilterPassThroughValue = -1;
    mutable const OGRSpatialReference *poGlobalSRS = nullptr;

    std::mutex m_oMutex{};

    void AutoWarpLayerIfNecessary(int iSubLayer);
    OGRFeature *TranslateFromSrcLayer(OGRFeature *poSrcFeature);
    void ApplyAttributeFilterToSrcLayer(int iSubLayer);
    int GetAttrFilterPassThroughValue() const;
    void ConfigureActiveLayer();
    void SetSpatialFilterToSourceLayer(OGRLayer *poSrcLayer);

  public:
    OGRUnionLayer(
        const char *pszName, int nSrcLayers, /* must be >= 1 */
        OGRLayer *
            *papoSrcLayers, /* array itself ownership always transferred, layer
                               ownership depending on bTakeLayerOwnership */
        int bTakeLayerOwnership);

    ~OGRUnionLayer() override;

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

    const char *GetName() const override
    {
        return osName.c_str();
    }

    OGRwkbGeometryType GetGeomType() const override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    OGRFeature *GetFeature(GIntBig nFeatureId) override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    OGRErr ISetFeature(OGRFeature *poFeature) override;

    OGRErr IUpsertFeature(OGRFeature *poFeature) override;

    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;

    const OGRFeatureDefn *GetLayerDefn() const override;

    const OGRSpatialReference *GetSpatialRef() const override;

    GIntBig GetFeatureCount(int) override;

    OGRErr SetAttributeFilter(const char *) override;

    int TestCapability(const char *) const override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *) override;

    OGRErr SetIgnoredFields(CSLConstList papszFields) override;

    OGRErr SyncToDisk() override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif  // OGRUNIONLAYER_H_INCLUDED
