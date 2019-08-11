/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OGR Driver for DGN Reader.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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

#ifndef OGR_DGN_H_INCLUDED
#define OGR_DGN_H_INCLUDED

#include "dgnlib.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                            OGRDGNLayer                               */
/************************************************************************/

class OGRDGNLayer final: public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    int                 iNextShapeId;

    DGNHandle           hDGN;
    int                 bUpdate;

    char               *pszLinkFormat;

    OGRFeature         *ElementToFeature( DGNElemCore *, int nRecLevel );

    void                ConsiderBrush( DGNElemCore *, const char *pszPen,
                                       OGRFeature *poFeature );

    DGNElemCore       **LineStringToElementGroup( OGRLineString *, int );
    DGNElemCore       **TranslateLabel( OGRFeature * );

    // Unused:
    // int                 bHaveSimpleQuery;
    OGRFeature         *poEvalFeature;

    OGRErr              CreateFeatureWithGeom( OGRFeature *, OGRGeometry * );

  public:
                        OGRDGNLayer( const char * pszName, DGNHandle hDGN,
                                     int bUpdate );
                        virtual ~OGRDGNLayer();

    void                SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

    void                ResetReading() override;
    OGRFeature *        GetNextFeature() override;
    OGRFeature *        GetFeature( GIntBig nFeatureId ) override;

    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    int                 TestCapability( const char * ) override;

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
};

/************************************************************************/
/*                          OGRDGNDataSource                            */
/************************************************************************/

class OGRDGNDataSource final: public OGRDataSource
{
    OGRDGNLayer     **papoLayers;
    int                 nLayers;

    char                *pszName;
    DGNHandle           hDGN;

    char                **papszOptions;

  public:
                        OGRDGNDataSource();
                        ~OGRDGNDataSource();

    int                 Open( const char *, int bTestOpen, int bUpdate );
    bool                PreCreate( const char *, char ** );

    OGRLayer           *ICreateLayer( const char *,
                                     OGRSpatialReference * = nullptr,
                                     OGRwkbGeometryType = wkbUnknown,
                                     char ** = nullptr ) override;

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_DGN_H_INCLUDED */
