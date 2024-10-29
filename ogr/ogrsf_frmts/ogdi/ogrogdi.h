/******************************************************************************
 * $Id$
 *
 * Project:  OGDI Bridge
 * Purpose:  Private definitions within the OGDI driver to implement
 *           integration with OGR.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *           (Based on some code contributed by Frank Warmerdam :)
 *
 ******************************************************************************
 * Copyright (c) 2000,  Daniel Morissette
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGDOGDI_H_INCLUDED
#define OGDOGDI_H_INCLUDED

#include <math.h>
extern "C"
{
/* Older versions of OGDI have register keywords as qualifier for arguments
 * of functions, which is illegal in C++17 */
#define register
#include "ecs.h"
#undef register
}
#include "ogrsf_frmts.h"

/************************************************************************/
/*                             OGROGDILayer                             */
/************************************************************************/
class OGROGDIDataSource;

class OGROGDILayer final : public OGRLayer
{
    OGROGDIDataSource *m_poODS;
    int m_nClientID;
    char *m_pszOGDILayerName;
    ecs_Family m_eFamily;

    OGRFeatureDefn *m_poFeatureDefn;
    OGRSpatialReference *m_poSpatialRef;
    ecs_Region m_sFilterBounds;

    int m_iNextShapeId;
    int m_nTotalShapeCount;
    int m_nFilteredOutShapes;

    OGRFeature *GetNextRawFeature();

  public:
    OGROGDILayer(OGROGDIDataSource *, const char *, ecs_Family);
    virtual ~OGROGDILayer();

    virtual void SetSpatialFilter(OGRGeometry *) override;

    virtual void SetSpatialFilter(int iGeomField, OGRGeometry *poGeom) override
    {
        OGRLayer::SetSpatialFilter(iGeomField, poGeom);
    }

    virtual OGRErr SetAttributeFilter(const char *pszQuery) override;

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;

    OGRFeature *GetFeature(GIntBig nFeatureId) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    GIntBig GetFeatureCount(int) override;

    int TestCapability(const char *) override;

  private:
    void BuildFeatureDefn();
};

/************************************************************************/
/*                          OGROGDIDataSource                           */
/************************************************************************/

class OGROGDIDataSource final : public GDALDataset
{
    OGROGDILayer **m_papoLayers;
    int m_nLayers;

    int m_nClientID;

    ecs_Region m_sGlobalBounds;
    OGRSpatialReference *m_poSpatialRef;

    OGROGDILayer *m_poCurrentLayer;

    int m_bLaunderLayerNames;

    void IAddLayer(const char *pszLayerName, ecs_Family eFamily);

  public:
    OGROGDIDataSource();
    ~OGROGDIDataSource();

    int Open(const char *);

    int GetLayerCount() override
    {
        return m_nLayers;
    }

    OGRLayer *GetLayer(int) override;

    ecs_Region *GetGlobalBounds()
    {
        return &m_sGlobalBounds;
    }

    OGRSpatialReference *DSGetSpatialRef()
    {
        return m_poSpatialRef;
    }

    int GetClientID()
    {
        return m_nClientID;
    }

    OGROGDILayer *GetCurrentLayer()
    {
        return m_poCurrentLayer;
    }

    void SetCurrentLayer(OGROGDILayer *poLayer)
    {
        m_poCurrentLayer = poLayer;
    }

    int LaunderLayerNames()
    {
        return m_bLaunderLayerNames;
    }
};

#endif /* OGDOGDI_H_INCLUDED */
