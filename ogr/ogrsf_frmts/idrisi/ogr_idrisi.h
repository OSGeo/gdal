/******************************************************************************
 *
 * Project:  Idrisi Translator
 * Purpose:  Definition of classes for OGR Idrisi driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_IDRISI_H_INCLUDED
#define OGR_IDRISI_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                         OGRIdrisiLayer                               */
/************************************************************************/

class OGRIdrisiLayer final : public OGRLayer,
                             public OGRGetNextFeatureThroughRaw<OGRIdrisiLayer>
{
  protected:
    OGRFeatureDefn *poFeatureDefn;
    OGRSpatialReference *poSRS;
    OGRwkbGeometryType eGeomType;

    VSILFILE *fp;
    VSILFILE *fpAVL;
    bool bEOF;

    int nNextFID;

    bool bExtentValid;
    double dfMinX;
    double dfMinY;
    double dfMaxX;
    double dfMaxY;

    unsigned int nTotalFeatures;

    bool Detect_AVL_ADC(const char *pszFilename);
    void ReadAVLLine(OGRFeature *poFeature);

    OGRFeature *GetNextRawFeature();

  public:
    OGRIdrisiLayer(const char *pszFilename, const char *pszLayerName,
                   VSILFILE *fp, OGRwkbGeometryType eGeomType,
                   const char *pszWTKString);
    virtual ~OGRIdrisiLayer();

    virtual void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRIdrisiLayer)

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    virtual int TestCapability(const char *) override;

    void SetExtent(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
    virtual OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                              bool bForce) override;

    virtual GIntBig GetFeatureCount(int bForce = TRUE) override;
};

/************************************************************************/
/*                        OGRIdrisiDataSource                           */
/************************************************************************/

class OGRIdrisiDataSource final : public GDALDataset
{
    OGRLayer **papoLayers = nullptr;
    int nLayers = 0;

  public:
    OGRIdrisiDataSource();
    virtual ~OGRIdrisiDataSource();

    int Open(const char *pszFilename);

    virtual int GetLayerCount() override
    {
        return nLayers;
    }

    virtual OGRLayer *GetLayer(int) override;
};

#endif  // ndef OGR_IDRISI_H_INCLUDED
