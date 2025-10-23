/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRWarpedLayer class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRWARPEDLAYER_H_INCLUDED
#define OGRWARPEDLAYER_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrlayerdecorator.h"
#include "ogrlayerwithtranslatefeature.h"

#include <memory>

#if defined(_MSC_VER)
#pragma warning(push)
// Silence warnings of the type warning C4250: 'OGRWarpedLayer': inherits 'OGRLayerDecorator::OGRLayerDecorator::GetMetadata' via dominance
#pragma warning(disable : 4250)
#endif

/************************************************************************/
/*                           OGRWarpedLayer                             */
/************************************************************************/

class CPL_DLL OGRWarpedLayer : public OGRLayerDecorator,
                               public OGRLayerWithTranslateFeature
{
    CPL_DISALLOW_COPY_ASSIGN(OGRWarpedLayer)

  protected:
    OGRFeatureDefn *m_poFeatureDefn;
    int m_iGeomField;

    OGRGeometryFactory::TransformWithOptionsCache m_transformCacheForward{};
    OGRGeometryFactory::TransformWithOptionsCache m_transformCacheReverse{};
    OGRCoordinateTransformation *m_poCT = nullptr;
    OGRCoordinateTransformation *m_poReversedCT = nullptr; /* may be NULL */
    OGRSpatialReference *m_poSRS = nullptr;

    OGREnvelope sStaticEnvelope{};

    static int ReprojectEnvelope(OGREnvelope *psEnvelope,
                                 OGRCoordinateTransformation *poCT);

    std::unique_ptr<OGRFeature>
    SrcFeatureToWarpedFeature(std::unique_ptr<OGRFeature> poFeature);
    std::unique_ptr<OGRFeature>
    WarpedFeatureToSrcFeature(std::unique_ptr<OGRFeature> poFeature);

  public:
    OGRWarpedLayer(
        OGRLayer *poDecoratedLayer, int iGeomField, int bTakeOwnership,
        OGRCoordinateTransformation
            *poCT, /* must NOT be NULL, ownership acquired by OGRWarpedLayer */
        OGRCoordinateTransformation *
            poReversedCT /* may be NULL, ownership acquired by OGRWarpedLayer */);
    virtual ~OGRWarpedLayer();

    void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) override;

    void SetExtent(double dfXMin, double dfYMin, double dfXMax, double dfYMax);

    virtual OGRErr ISetSpatialFilter(int iGeomField,
                                     const OGRGeometry *) override;

    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature(GIntBig nFID) override;
    virtual OGRErr ISetFeature(OGRFeature *poFeature) override;
    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual OGRErr IUpsertFeature(OGRFeature *poFeature) override;
    OGRErr IUpdateFeature(OGRFeature *poFeature, int nUpdatedFieldsCount,
                          const int *panUpdatedFieldsIdx,
                          int nUpdatedGeomFieldsCount,
                          const int *panUpdatedGeomFieldsIdx,
                          bool bUpdateStyleString) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce = true) override;

    virtual int TestCapability(const char *) override;

    virtual bool GetArrowStream(struct ArrowArrayStream *out_stream,
                                CSLConstList papszOptions = nullptr) override;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif /* #ifndef DOXYGEN_SKIP */

#endif  //  OGRWARPEDLAYER_H_INCLUDED
