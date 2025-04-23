/******************************************************************************
 *
 * Project:  NAS Reader
 * Purpose:  Declarations for OGR wrapper classes for NAS, and NAS<->OGR
 *           translation of geometry.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_NAS_H_INCLUDED
#define OGR_NAS_H_INCLUDED

#include "ogrsf_frmts.h"
#include "nasreaderp.h"
#include "ogr_api.h"
#include <vector>

class OGRNASDataSource;

/************************************************************************/
/*                            OGRNASLayer                               */
/************************************************************************/

class OGRNASLayer final : public OGRLayer
{
    OGRFeatureDefn *poFeatureDefn;

    int iNextNASId;

    OGRNASDataSource *poDS;

    GMLFeatureClass *poFClass;

  public:
    OGRNASLayer(const char *pszName, OGRNASDataSource *poDS);

    virtual ~OGRNASLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) override;
};

/************************************************************************/
/*                           OGRNASDataSource                           */
/************************************************************************/

class OGRNASDataSource final : public GDALDataset
{
    OGRLayer **papoLayers;
    int nLayers;

    OGRNASLayer *TranslateNASSchema(GMLFeatureClass *);

    // input related parameters.
    IGMLReader *poReader;

    void InsertHeader();

  public:
    OGRNASDataSource();
    ~OGRNASDataSource();

    int Open(const char *);
    int Create(const char *pszFile, char **papszOptions);

    int GetLayerCount() override
    {
        return nLayers;
    }

    OGRLayer *GetLayer(int) override;

    IGMLReader *GetReader()
    {
        return poReader;
    }

    void GrowExtents(OGREnvelope *psGeomBounds);
};

#endif /* OGR_NAS_H_INCLUDED */
