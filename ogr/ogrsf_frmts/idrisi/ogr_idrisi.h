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
/*                            OGRIdrisiLayer                            */
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
    ~OGRIdrisiLayer() override;

    void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRIdrisiLayer)

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) const override;

    void SetExtent(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;
};

/************************************************************************/
/*                         OGRIdrisiDataSource                          */
/************************************************************************/

class OGRIdrisiDataSource final : public GDALDataset
{
    OGRLayer **papoLayers = nullptr;
    int nLayers = 0;

  public:
    OGRIdrisiDataSource();
    ~OGRIdrisiDataSource() override;

    int Open(const char *pszFilename);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;
};

#endif  // ndef OGR_IDRISI_H_INCLUDED
