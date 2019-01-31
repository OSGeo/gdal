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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef OGDOGDI_H_INCLUDED
#define OGDOGDI_H_INCLUDED

#include <math.h>
extern "C" {
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

class OGROGDILayer final: public OGRLayer
{
    OGROGDIDataSource  *m_poODS;
    int                 m_nClientID;
    char               *m_pszOGDILayerName;
    ecs_Family          m_eFamily;

    OGRFeatureDefn     *m_poFeatureDefn;
    OGRSpatialReference *m_poSpatialRef;
    ecs_Region          m_sFilterBounds;

    int                 m_iNextShapeId;
    int                 m_nTotalShapeCount;
    int                 m_nFilteredOutShapes;

    OGRFeature *        GetNextRawFeature();

  public:
                        OGROGDILayer(OGROGDIDataSource *, const char *,
                                     ecs_Family);
                        virtual ~OGROGDILayer();

    virtual void        SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }
    virtual OGRErr      SetAttributeFilter( const char *pszQuery ) override;

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;

    OGRFeature         *GetFeature( GIntBig nFeatureId ) override;

    OGRFeatureDefn *    GetLayerDefn() override { return m_poFeatureDefn; }

    GIntBig             GetFeatureCount( int ) override;

    int                 TestCapability( const char * ) override;

  private:
    void                BuildFeatureDefn();
};

/************************************************************************/
/*                          OGROGDIDataSource                           */
/************************************************************************/

class OGROGDIDataSource final: public OGRDataSource
{
    OGROGDILayer      **m_papoLayers;
    int                 m_nLayers;

    int                 m_nClientID;

    ecs_Region          m_sGlobalBounds;
    OGRSpatialReference *m_poSpatialRef;

    OGROGDILayer        *m_poCurrentLayer;

    char                *m_pszFullName;

    int                 m_bLaunderLayerNames;

    void                IAddLayer( const char *pszLayerName,
                                   ecs_Family eFamily );

  public:
                        OGROGDIDataSource();
                        ~OGROGDIDataSource();

    int                 Open( const char * );

    const char          *GetName() override { return m_pszFullName; }
    int                 GetLayerCount() override { return m_nLayers; }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;

    ecs_Region         *GetGlobalBounds() { return &m_sGlobalBounds; }
    OGRSpatialReference*DSGetSpatialRef() { return m_poSpatialRef; }
    int                 GetClientID() { return m_nClientID; }

    OGROGDILayer       *GetCurrentLayer() { return m_poCurrentLayer; }
    void                SetCurrentLayer(OGROGDILayer* poLayer) { m_poCurrentLayer = poLayer ; }

    int                 LaunderLayerNames() { return m_bLaunderLayerNames; }
};

/************************************************************************/
/*                            OGROGDIDriver                             */
/************************************************************************/

class OGROGDIDriver final: public OGRSFDriver
{
  public:
                ~OGROGDIDriver();

    const char *GetName() override;
    OGRDataSource *Open( const char *, int ) override;

    int         TestCapability( const char * ) override;
};

#endif /* OGDOGDI_H_INCLUDED */
