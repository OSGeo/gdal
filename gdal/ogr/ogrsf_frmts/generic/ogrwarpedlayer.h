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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef OGRWARPEDLAYER_H_INCLUDED
#define OGRWARPEDLAYER_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrlayerdecorator.h"

/************************************************************************/
/*                           OGRWarpedLayer                             */
/************************************************************************/

class OGRWarpedLayer : public OGRLayerDecorator
{
    CPL_DISALLOW_COPY_ASSIGN(OGRWarpedLayer)

  protected:
      OGRFeatureDefn              *m_poFeatureDefn;
      int                          m_iGeomField;

      OGRCoordinateTransformation *m_poCT;
      OGRCoordinateTransformation *m_poReversedCT; /* may be NULL */
      OGRSpatialReference         *m_poSRS;

      OGREnvelope                  sStaticEnvelope{};

      static int ReprojectEnvelope( OGREnvelope* psEnvelope,
                                    OGRCoordinateTransformation* poCT );

      OGRFeature *                 SrcFeatureToWarpedFeature(OGRFeature* poFeature);
      OGRFeature *                 WarpedFeatureToSrcFeature(OGRFeature* poFeature);

  public:

                       OGRWarpedLayer(OGRLayer* poDecoratedLayer,
                                      int iGeomField,
                                      int bTakeOwnership,
                                      OGRCoordinateTransformation* poCT,  /* must NOT be NULL, ownership acquired by OGRWarpedLayer */
                                      OGRCoordinateTransformation* poReversedCT /* may be NULL, ownership acquired by OGRWarpedLayer */);
    virtual           ~OGRWarpedLayer();

    void                SetExtent(double dfXMin, double dfYMin, double dfXMax, double dfYMax);

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilterRect( double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry * ) override;
    virtual void        SetSpatialFilterRect( int iGeomField, double dfMinX, double dfMinY,
                                              double dfMaxX, double dfMaxY ) override;

    virtual OGRFeature *GetNextFeature() override;
    virtual OGRFeature *GetFeature( GIntBig nFID ) override;
    virtual OGRErr      ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr      ICreateFeature( OGRFeature *poFeature ) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRSpatialReference *GetSpatialRef() override;

    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual int         TestCapability( const char * ) override;
};

#endif /* #ifndef DOXYGEN_SKIP */

#endif //  OGRWARPEDLAYER_H_INCLUDED
