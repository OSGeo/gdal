/******************************************************************************
 * $Id$
 *
 * Project:  HTF Translator
 * Purpose:  Definition of classes for OGR .htf driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_HTF_H_INCLUDED
#define OGR_HTF_H_INCLUDED

#include "ogrsf_frmts.h"

#include <vector>

/************************************************************************/
/*                             OGRHTFLayer                              */
/************************************************************************/

class OGRHTFLayer : public OGRLayer
{
protected:
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;

    VSILFILE*          fpHTF;
    bool               bEOF;

    int                nNextFID;

    virtual OGRFeature *       GetNextRawFeature() = 0;

    bool              bHasExtent;
    double             dfMinX;
    double             dfMinY;
    double             dfMaxX;
    double             dfMaxY;

  public:
                        OGRHTFLayer(const char* pszFilename, int nZone, int bIsNorth);
                        virtual ~OGRHTFLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature *        GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }
    void    SetExtent(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
};

/************************************************************************/
/*                      OGRHTFPolygonLayer                              */
/************************************************************************/

class OGRHTFPolygonLayer : public OGRHTFLayer
{
protected:
    virtual OGRFeature *       GetNextRawFeature() override;

  public:
                        OGRHTFPolygonLayer(const char* pszFilename, int nZone, int bIsNorth);

    virtual void                ResetReading() override;
};

/************************************************************************/
/*                      OGRHTFSoundingLayer                             */
/************************************************************************/

class OGRHTFSoundingLayer : public OGRHTFLayer
{
private:
    bool                       bHasFPK;
    int                        nFieldsPresent;
    bool                      *panFieldPresence;
    int                        nEastingIndex;
    int                        nNorthingIndex;
    int                        nTotalSoundings;

protected:
    virtual OGRFeature *       GetNextRawFeature() override;

  public:
                        OGRHTFSoundingLayer(const char* pszFilename, int nZone, int bIsNorth, int nTotalSoundings);
                       virtual ~OGRHTFSoundingLayer();

    virtual void                ResetReading() override;

    virtual int                 TestCapability( const char * ) override;

    virtual GIntBig             GetFeatureCount(int bForce = TRUE) override;
};

/************************************************************************/
/*                          OGRHTFMetadataLayer                         */
/************************************************************************/

class OGRHTFMetadataLayer : public OGRLayer
{
protected:
    OGRFeatureDefn        *poFeatureDefn;
    OGRFeature            *poFeature;
    std::vector<CPLString> aosMD;

    int                nNextFID;

  public:
    explicit            OGRHTFMetadataLayer(const std::vector<CPLString>& aosMD);
                        virtual ~OGRHTFMetadataLayer();

    virtual void                ResetReading() override { nNextFID = 0; }
    virtual OGRFeature *        GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                           OGRHTFDataSource                           */
/************************************************************************/

class OGRHTFDataSource : public OGRDataSource
{
    char*               pszName;

    OGRHTFLayer**       papoLayers;
    int                 nLayers;
    OGRLayer           *poMetadataLayer;

  public:
                        OGRHTFDataSource();
                        virtual ~OGRHTFDataSource();

    int                 Open( const char * pszFilename );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;
    virtual OGRLayer*           GetLayerByName( const char* pszLayerName ) override;

    virtual int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_HTF_H_INCLUDED */
