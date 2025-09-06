/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the OGR GMT driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRGMT_H_INCLUDED
#define OGRGMT_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "cpl_string.h"

/************************************************************************/
/*                             OGRGmtLayer                              */
/************************************************************************/

class OGRGmtLayer final : public OGRLayer,
                          public OGRGetNextFeatureThroughRaw<OGRGmtLayer>
{
    GDALDataset *m_poDS = nullptr;
    OGRSpatialReference *m_poSRS = nullptr;
    OGRFeatureDefn *poFeatureDefn{};

    int iNextFID{};

    bool bUpdate{};
    bool bHeaderComplete{};

    bool bRegionComplete{};
    OGREnvelope sRegion{};
    vsi_l_offset nRegionOffset{};

    VSILFILE *m_fp = nullptr;

    bool ReadLine();
    CPLString osLine{};
    char **papszKeyedValues{};

    bool ScanAheadForHole();
    bool NextIsFeature();

    OGRFeature *GetNextRawFeature();

    OGRErr WriteGeometry(OGRGeometryH hGeom, bool bHaveAngle);
    OGRErr CompleteHeader(OGRGeometry *);

    CPL_DISALLOW_COPY_ASSIGN(OGRGmtLayer)

  public:
    bool bValidFile;

    OGRGmtLayer(GDALDataset *poDS, const char *pszFilename, VSILFILE *fp,
                const OGRSpatialReference *poSRS, int bUpdate);
    ~OGRGmtLayer() override;

    void ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRGmtLayer)

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;

    int TestCapability(const char *) const override;

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
};

/************************************************************************/
/*                           OGRGmtDataSource                           */
/************************************************************************/

class OGRGmtDataSource final : public GDALDataset
{
    OGRGmtLayer **papoLayers;
    int nLayers;

    bool bUpdate;

    CPL_DISALLOW_COPY_ASSIGN(OGRGmtDataSource)

  public:
    OGRGmtDataSource();
    ~OGRGmtDataSource() override;

    int Open(const char *pszFilename, VSILFILE *fp,
             const OGRSpatialReference *poSRS, int bUpdate);

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;
    int TestCapability(const char *) const override;
};

#endif /* ndef OGRGMT_H_INCLUDED */
