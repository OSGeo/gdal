/******************************************************************************
 *
 * Project:  Interlis 2 Translator
 * Purpose:   Definition of classes for OGR Interlis 2 driver.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_ILI2_H_INCLUDED
#define OGR_ILI2_H_INCLUDED

#include "ogrsf_frmts.h"
#include "imdreader.h"
#include "ili2reader.h"

#include <string>
#include <list>

class OGRILI2DataSource;

/************************************************************************/
/*                           OGRILI2Layer                               */
/************************************************************************/

class OGRILI2Layer final : public OGRLayer
{
  private:
    OGRFeatureDefn *poFeatureDefn;
    GeomFieldInfos oGeomFieldInfos;
    std::list<OGRFeature *> listFeature;
    std::list<OGRFeature *>::const_iterator listFeatureIt;

    OGRILI2DataSource *poDS;

  public:
    OGRILI2Layer(OGRFeatureDefn *poFeatureDefn,
                 const GeomFieldInfos &oGeomFieldInfos,
                 OGRILI2DataSource *poDS);

    ~OGRILI2Layer();

    void AddFeature(OGRFeature *poFeature);

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    CPLString GetIliGeomType(const char *cFieldName)
    {
        return oGeomFieldInfos[cFieldName].iliGeomType;
    }

    int TestCapability(const char *) override;

    GDALDataset *GetDataset() override;
};

/************************************************************************/
/*                          OGRILI2DataSource                           */
/************************************************************************/

class OGRILI2DataSource final : public GDALDataset
{
  private:
    std::list<OGRLayer *> listLayer;

    char *pszName;
    ImdReader *poImdReader;
    IILI2Reader *poReader;

    int nLayers;
    OGRILI2Layer **papoLayers;

    CPL_DISALLOW_COPY_ASSIGN(OGRILI2DataSource)

  public:
    OGRILI2DataSource();
    virtual ~OGRILI2DataSource();

    int Open(const char *, char **papszOpenOptions, int bTestOpen);

    int GetLayerCount() override
    {
        return static_cast<int>(listLayer.size());
    }

    OGRLayer *GetLayer(int) override;

    int TestCapability(const char *) override;
};

#endif /* OGR_ILI2_H_INCLUDED */
