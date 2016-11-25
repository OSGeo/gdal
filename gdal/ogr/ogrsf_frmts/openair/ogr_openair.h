/******************************************************************************
 * $Id$
 *
 * Project:  OpenAir Translator
 * Purpose:  Definition of classes for OGR .sua driver.
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

#ifndef OGR_OPENAIR_H_INCLUDED
#define OGR_OPENAIR_H_INCLUDED

#include "ogrsf_frmts.h"
#include <map>

/************************************************************************/
/*                           OGROpenAirLayer                            */
/************************************************************************/

typedef struct
{
    int penStyle;
    int penWidth;
    int penR, penG, penB;
    int fillR;
    int fillG;
    int fillB;
} OpenAirStyle;

class OGROpenAirLayer : public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;

    VSILFILE*          fpOpenAir;
    bool               bEOF;
    bool               bHasLastLine;
    CPLString          osLastLine;

    int                nNextFID;

    std::map<CPLString,OpenAirStyle*> oStyleMap;

    OGRFeature *       GetNextRawFeature();

  public:
    explicit            OGROpenAirLayer(VSILFILE* fp);
                        virtual ~OGROpenAirLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature *        GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                       OGROpenAirLabelLayer                           */
/************************************************************************/

class OGROpenAirLabelLayer : public OGRLayer
{
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;

    VSILFILE*          fpOpenAir;
    CPLString          osLastLine;

    int                nNextFID;

    OGRFeature *       GetNextRawFeature();

    CPLString          osCLASS;
    CPLString          osNAME;
    CPLString          osFLOOR;
    CPLString          osCEILING;

  public:
    explicit            OGROpenAirLabelLayer(VSILFILE* fp);
                        virtual ~OGROpenAirLabelLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature *        GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                         OGROpenAirDataSource                         */
/************************************************************************/

class OGROpenAirDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

  public:
                        OGROpenAirDataSource();
                        virtual ~OGROpenAirDataSource();

    int                 Open( const char * pszFilename );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;
};

bool OGROpenAirGetLatLon( const char* pszStr, double& dfLat, double& dfLon );

#endif /* ndef OGR_OPENAIR_H_INCLUDED */
