/******************************************************************************
 * $Id$
 *
 * Project:  SEG-P1 / UKOOA P1-90 Translator
 * Purpose:  Definition of classes for OGR SEG-P1 / UKOOA P1-90 driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMSEGUKOOAS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef OGR_SEGUKOOA_H_INCLUDED
#define OGR_SEGUKOOA_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                        OGRSEGUKOOABaseLayer                          */
/************************************************************************/

class OGRSEGUKOOABaseLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn*    poFeatureDefn;
    bool               bEOF;
    int                nNextFID;

    virtual OGRFeature *       GetNextRawFeature() = 0;

  public:
    virtual OGRFeature *        GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override { return FALSE; }
};

/************************************************************************/
/*                          OGRUKOOAP190Layer                           */
/************************************************************************/

class OGRUKOOAP190Layer : public OGRSEGUKOOABaseLayer
{
    OGRSpatialReference* poSRS;

    VSILFILE*          fp;

    int                bUseEastingNorthingAsGeometry;
    int                nYear;
    void               ParseHeaders();

  protected:
    OGRFeature *       GetNextRawFeature() override;

  public:
                        OGRUKOOAP190Layer(const char* pszFilename,
                                         VSILFILE* fp);
                        virtual ~OGRUKOOAP190Layer();

    virtual void                ResetReading() override;
};

/************************************************************************/
/*                        OGRSEGUKOOALineLayer                          */
/************************************************************************/

class OGRSEGUKOOALineLayer : public OGRSEGUKOOABaseLayer
{
    OGRLayer          *poBaseLayer;
    OGRFeature        *poNextBaseFeature;

  protected:
    OGRFeature *       GetNextRawFeature() override;

  public:
                        OGRSEGUKOOALineLayer(const char* pszFilename,
                                             OGRLayer *poBaseLayer);
                        virtual ~OGRSEGUKOOALineLayer();

    virtual void                ResetReading() override;
};

/************************************************************************/
/*                         OGRSEGP1Layer                                */
/************************************************************************/

class OGRSEGP1Layer: public OGRSEGUKOOABaseLayer
{
    OGRSpatialReference* poSRS;

    VSILFILE*          fp;
    int                nLatitudeCol;

    int                bUseEastingNorthingAsGeometry;

  protected:
    OGRFeature *       GetNextRawFeature() override;

  public:
                        OGRSEGP1Layer(const char* pszFilename,
                                      VSILFILE* fp,
                                      int nLatitudeCol);
                        virtual ~OGRSEGP1Layer();

    virtual void                ResetReading() override;

public:
    static char* ExpandTabs(const char* pszLine);
    static int   DetectLatitudeColumn(const char* pzLine);
};

/************************************************************************/
/*                         OGRSEGUKOOADataSource                        */
/************************************************************************/

class OGRSEGUKOOADataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

  public:
                        OGRSEGUKOOADataSource();
                        virtual ~OGRSEGUKOOADataSource();

    int                 Open( const char * pszFilename );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;
};

#endif /* ndef OGR_SEGUKOOA_H_INCLUDED */
