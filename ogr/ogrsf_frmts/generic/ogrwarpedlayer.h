/******************************************************************************
 * $Id$
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
#include <memory>

/************************************************************************/
/*                           OGRWarpedLayer                             */
/************************************************************************/

class CPL_DLL OGRWarpedLayer : public OGRLayerDecorator
{
    CPL_DISALLOW_COPY_ASSIGN(OGRWarpedLayer)

  protected:
    OGRFeatureDefn *m_poFeatureDefn;
    int m_iGeomField;

    OGRCoordinateTransformation *m_poCT;
    OGRCoordinateTransformation *m_poReversedCT; /* may be NULL */
    OGRSpatialReference *m_poSRS;

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

    void SetExtent(double dfXMin, double dfYMin, double dfXMax, double dfYMax);

    virtual void SetSpatialFilter(OGRGeometry *) override;
    virtual void SetSpatialFilterRect(double dfMinX, double dfMinY,
                                      double dfMaxX, double dfMaxY) override;
    virtual void SetSpatialFilter(int iGeomField, OGRGeometry *) override;
    virtual void SetSpatialFilterRect(int iGeomField, double dfMinX,
                                      double dfMinY, double dfMaxX,
                                      double dfMaxY) override;

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
    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce = TRUE) override;
    virtual OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual int TestCapability(const char *) override;

    virtual bool GetArrowStream(struct ArrowArrayStream *out_stream,
                                CSLConstList papszOptions = nullptr) override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif  //  OGRWARPEDLAYER_H_INCLUDED
