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

#ifndef _OGR_SEGUKOOA_H_INCLUDED
#define _OGR_SEGUKOOA_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                        OGRSEGUKOOABaseLayer                          */
/************************************************************************/

class OGRSEGUKOOABaseLayer : public OGRLayer
{
  protected:
    OGRFeatureDefn*    poFeatureDefn;
    int                bEOF;
    int                nNextFID;

    virtual OGRFeature *       GetNextRawFeature() = 0;

  public:
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) { return FALSE; }
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
    OGRFeature *       GetNextRawFeature();

  public:
                        OGRUKOOAP190Layer(const char* pszFilename,
                                         VSILFILE* fp);
                        ~OGRUKOOAP190Layer();


    virtual void                ResetReading();
};

/************************************************************************/
/*                        OGRSEGUKOOALineLayer                          */
/************************************************************************/

class OGRSEGUKOOALineLayer : public OGRSEGUKOOABaseLayer
{
    OGRLayer          *poBaseLayer;
    OGRFeature        *poNextBaseFeature;

  protected:
    OGRFeature *       GetNextRawFeature();

  public:
                        OGRSEGUKOOALineLayer(const char* pszFilename,
                                             OGRLayer *poBaseLayer);
                        ~OGRSEGUKOOALineLayer();

    virtual void                ResetReading();
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
    OGRFeature *       GetNextRawFeature();

  public:
                        OGRSEGP1Layer(const char* pszFilename,
                                      VSILFILE* fp,
                                      int nLatitudeCol);
                        ~OGRSEGP1Layer();

    virtual void                ResetReading();

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
                        ~OGRSEGUKOOADataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return nLayers; }
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );
};

/************************************************************************/
/*                          OGRSEGUKOOADriver                           */
/************************************************************************/

class OGRSEGUKOOADriver : public OGRSFDriver
{
  public:
                ~OGRSEGUKOOADriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );
};


#endif /* ndef _OGR_SEGUKOOA_H_INCLUDED */
