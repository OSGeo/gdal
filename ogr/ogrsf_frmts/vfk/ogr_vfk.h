/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions for OGR/VFK driver.
 * Author:   Martin Landa, landa.martin gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Martin Landa <landa.martin gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_OGR_VFK_H_INCLUDED
#define GDAL_OGR_VFK_H_INCLUDED

#include <map>
#include <string>

#include "ogrsf_frmts.h"
#include "vfkreader.h"

class OGRVFKDataSource;

/************************************************************************/
/*                            OGRVFKLayer                               */
/************************************************************************/

class OGRVFKLayer : public OGRLayer
{
  private:
    /* spatial reference */
    OGRSpatialReference *poSRS;

    /* feature definition */
    OGRFeatureDefn *poFeatureDefn;

    /* VFK data block */
    IVFKDataBlock *poDataBlock;

    /* get next feature */
    int m_iNextFeature;

    /* private methods */
    static const OGRGeometry *GetGeometry(IVFKFeature *);
    OGRFeature *GetFeature(IVFKFeature *);

  public:
    OGRVFKLayer(const char *, OGRSpatialReference *, OGRwkbGeometryType,
                OGRVFKDataSource *);
    ~OGRVFKLayer();

    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    void ResetReading() override;

    int TestCapability(const char *) override;

    GIntBig GetFeatureCount(int = TRUE) override;
};

/************************************************************************/
/*                           OGRVFKDataSource                           */
/************************************************************************/
class OGRVFKDataSource : public GDALDataset
{
  private:
    /* list of available layers */
    OGRVFKLayer **papoLayers;
    int nLayers;

    /* input related parameters */
    IVFKReader *poReader;

    /* private methods */
    OGRVFKLayer *CreateLayerFromBlock(const IVFKDataBlock *);

  public:
    OGRVFKDataSource();
    ~OGRVFKDataSource();

    int Open(GDALOpenInfo *poOpenInfo);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;

    int TestCapability(const char *) override;

    IVFKReader *GetReader() const
    {
        return poReader;
    }
};

#endif  // GDAL_OGR_VFK_H_INCLUDED
