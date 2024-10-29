/******************************************************************************
 * $Id$
 *
 * Project:  STS Translator
 * Purpose:  Definition of classes finding SDTS support into OGRDriver
 *           framework.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_SDTS_H_INCLUDED
#define OGR_SDTS_H_INCLUDED

#include "sdts_al.h"
#include "ogrsf_frmts.h"

class OGRSDTSDataSource;

/************************************************************************/
/*                             OGRSDTSLayer                             */
/************************************************************************/

class OGRSDTSLayer final : public OGRLayer
{
    OGRFeatureDefn *poFeatureDefn;

    SDTSTransfer *poTransfer;
    int iLayer;
    SDTSIndexedReader *poReader;

    OGRSDTSDataSource *poDS;

    OGRFeature *GetNextUnfilteredFeature();

  public:
    OGRSDTSLayer(SDTSTransfer *, int, OGRSDTSDataSource *);
    ~OGRSDTSLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) override;
};

/************************************************************************/
/*                          OGRSDTSDataSource                           */
/************************************************************************/

class OGRSDTSDataSource final : public GDALDataset
{
    SDTSTransfer *poTransfer;

    int nLayers;
    OGRSDTSLayer **papoLayers;

    OGRSpatialReference *poSRS;

  public:
    OGRSDTSDataSource();
    ~OGRSDTSDataSource();

    int Open(const char *pszFilename, int bTestOpen);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;
};

#endif /* ndef OGR_SDTS_H_INCLUDED */
