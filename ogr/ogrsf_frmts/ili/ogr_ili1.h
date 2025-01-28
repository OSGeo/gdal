/******************************************************************************
 *
 * Project:  Interlis 1 Translator
 * Purpose:   Definition of classes for OGR Interlis 1 driver.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_ILI1_H_INCLUDED
#define OGR_ILI1_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ili1reader.h"

class OGRILI1DataSource;

/************************************************************************/
/*                           OGRILI1Layer                               */
/************************************************************************/

class OGRILI1Layer final : public OGRLayer
{
  private:
#if 0
    OGRSpatialReference *poSRS;
#endif
    OGRFeatureDefn *poFeatureDefn;
    GeomFieldInfos oGeomFieldInfos;

    int nFeatures;
    OGRFeature **papoFeatures;
    int nFeatureIdx;

    bool bGeomsJoined;

    OGRILI1DataSource *poDS;

  public:
    OGRILI1Layer(OGRFeatureDefn *poFeatureDefn,
                 const GeomFieldInfos &oGeomFieldInfos,
                 OGRILI1DataSource *poDS);

    ~OGRILI1Layer();

    OGRErr AddFeature(OGRFeature *poFeature);

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetNextFeatureRef();
    OGRFeature *GetFeatureRef(GIntBig nFid);
    OGRFeature *GetFeatureRef(const char *);

    GIntBig GetFeatureCount(int bForce = TRUE) override;

    int GeometryAppend(OGRGeometry *poGeometry);

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    GeomFieldInfos GetGeomFieldInfos()
    {
        return oGeomFieldInfos;
    }

    int TestCapability(const char *) override;

    GDALDataset *GetDataset() override;

  private:
    void JoinGeomLayers();
    void JoinSurfaceLayer(OGRILI1Layer *poSurfaceLineLayer,
                          int nSurfaceFieldIndex);
    OGRMultiPolygon *Polygonize(OGRGeometryCollection *poLines,
                                bool fix_crossing_lines = false);
    void PolygonizeAreaLayer(OGRILI1Layer *poAreaLineLayer, int nAreaFieldIndex,
                             int nPointFieldIndex);
};

/************************************************************************/
/*                          OGRILI1DataSource                           */
/************************************************************************/

class OGRILI1DataSource final : public GDALDataset
{
  private:
    ImdReader *poImdReader;
    IILI1Reader *poReader;
    int nLayers;
    OGRILI1Layer **papoLayers;

    CPL_DISALLOW_COPY_ASSIGN(OGRILI1DataSource)

  public:
    OGRILI1DataSource();
    virtual ~OGRILI1DataSource();

    int Open(const char *, char **papszOpenOptions, int bTestOpen);

    int GetLayerCount() override
    {
        return poReader ? poReader->GetLayerCount() : 0;
    }

    OGRLayer *GetLayer(int) override;
    OGRILI1Layer *GetLayerByName(const char *) override;

    int TestCapability(const char *) override;
};

#endif /* OGR_ILI1_H_INCLUDED */
