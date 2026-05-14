/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGN Reader.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_DGN_H_INCLUDED
#define OGR_DGN_H_INCLUDED

#include "dgnlib.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGRDGNLayer                              */
/************************************************************************/

class OGRDGNDataSource;

class OGRDGNLayer final : public OGRLayer
{
    OGRDGNDataSource *m_poDS = nullptr;
    OGRFeatureDefn *poFeatureDefn{};

    int iNextShapeId{};

    DGNHandle hDGN{};
    int bUpdate{};

    char *pszLinkFormat{};

    OGRFeature *ElementToFeature(DGNElemCore *, int nRecLevel);

    void ConsiderBrush(DGNElemCore *, const char *pszPen,
                       OGRFeature *poFeature);

    DGNElemCore **LineStringToElementGroup(const OGRLineString *, int);
    DGNElemCore **TranslateLabel(OGRFeature *);

    // Unused:
    // int                 bHaveSimpleQuery;
    OGRFeature *poEvalFeature{};

    OGRErr CreateFeatureWithGeom(OGRFeature *, const OGRGeometry *);

    CPL_DISALLOW_COPY_ASSIGN(OGRDGNLayer)

  public:
    OGRDGNLayer(OGRDGNDataSource *poDS, const char *pszName, DGNHandle hDGN,
                int bUpdate);
    ~OGRDGNLayer() override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    OGRFeature *GetFeature(GIntBig nFeatureId) override;

    GIntBig GetFeatureCount(int bForce = TRUE) override;

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    const OGRFeatureDefn *GetLayerDefn() const override
    {
        return poFeatureDefn;
    }

    int TestCapability(const char *) const override;

    OGRErr ICreateFeature(OGRFeature *poFeature) override;

    GDALDataset *GetDataset() override;
};

/************************************************************************/
/*                           OGRDGNDataSource                           */
/************************************************************************/

class OGRDGNDataSource final : public GDALDataset
{
    OGRDGNLayer **papoLayers = nullptr;
    int nLayers = 0;

    DGNHandle hDGN = nullptr;

    char **papszOptions = nullptr;

    std::string m_osEncoding{};

    CPL_DISALLOW_COPY_ASSIGN(OGRDGNDataSource)

  public:
    OGRDGNDataSource();
    ~OGRDGNDataSource() override;

    bool Open(GDALOpenInfo *poOpenInfo);
    void PreCreate(CSLConstList);

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList) override;

    int GetLayerCount() const override
    {
        return nLayers;
    }

    const OGRLayer *GetLayer(int) const override;

    int TestCapability(const char *) const override;

    const std::string &GetEncoding() const
    {
        return m_osEncoding;
    }
};

#endif /* ndef OGR_DGN_H_INCLUDED */
