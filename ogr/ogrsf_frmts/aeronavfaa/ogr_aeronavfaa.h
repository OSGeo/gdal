/******************************************************************************
 * $Id$
 *
 * Project:  AeronavFAA Translator
 * Purpose:  Definition of classes for OGR AeronavFAA driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#ifndef _OGR_AeronavFAA_H_INCLUDED
#define _OGR_AeronavFAA_H_INCLUDED

#include "ogrsf_frmts.h"


typedef struct
{
    const char* pszFieldName;
    int nStartCol; /* starting at 1 */
    int nLastCol; /* starting at 1 */
    OGRFieldType eType;
} RecordFieldDesc;

typedef struct
{
    int              nFields;
    const RecordFieldDesc* pasFields;
    int              nLatStartCol; /* starting at 1 */
    int              nLonStartCol; /* starting at 1 */
} RecordDesc;



/************************************************************************/
/*                         OGRAeronavFAALayer                           */
/************************************************************************/

class OGRAeronavFAALayer : public OGRLayer
{
protected:
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;

    VSILFILE*          fpAeronavFAA;
    int                bEOF;

    int                nNextFID;

    const RecordDesc*  psRecordDesc;

    virtual OGRFeature *       GetNextRawFeature() = 0;

  public:
                        OGRAeronavFAALayer(VSILFILE* fp, const char* pszLayerName);
                        ~OGRAeronavFAALayer();


    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int                 TestCapability( const char * );

    virtual OGRSpatialReference *GetSpatialRef() { return poSRS; }

};

/************************************************************************/
/*                       OGRAeronavFAADOFLayer                          */
/************************************************************************/

class OGRAeronavFAADOFLayer : public OGRAeronavFAALayer
{
  private:
    int GetLatLon(const char* pszLat, const char* pszLon, double& dfLat, double& dfLon);

  protected:
    virtual OGRFeature *       GetNextRawFeature();

  public:
                        OGRAeronavFAADOFLayer(VSILFILE* fp, const char* pszLayerName);
};

/************************************************************************/
/*                     OGRAeronavFAANAVAIDLayer                         */
/************************************************************************/

class OGRAeronavFAANAVAIDLayer : public OGRAeronavFAALayer
{
  private:
    int GetLatLon(const char* pszLat, const char* pszLon, double& dfLat, double& dfLon);

  protected:
    virtual OGRFeature *       GetNextRawFeature();

  public:
                        OGRAeronavFAANAVAIDLayer(VSILFILE* fp, const char* pszLayerName);
};

/************************************************************************/
/*                     OGRAeronavFAARouteLayer                          */
/************************************************************************/

class OGRAeronavFAARouteLayer : public OGRAeronavFAALayer
{
  private:
    int       bIsDPOrSTARS;
    CPLString osLastReadLine;
    CPLString osAPTName;
    CPLString osStateName;
    int GetLatLon(const char* pszLat, const char* pszLon, double& dfLat, double& dfLon);

  protected:
    virtual OGRFeature *       GetNextRawFeature();

  public:
                        OGRAeronavFAARouteLayer(VSILFILE* fp, const char* pszLayerName, int bIsDPOrSTARS);

    virtual void                ResetReading();
};

/************************************************************************/
/*                     OGRAeronavFAAIAPLayer                            */
/************************************************************************/

class OGRAeronavFAAIAPLayer : public OGRAeronavFAALayer
{
  private:
    CPLString osCityName;
    CPLString osStateName;
    CPLString osAPTName;
    CPLString osAPTId;
    int GetLatLon(const char* pszLat, const char* pszLon, double& dfLat, double& dfLon);

  protected:
    virtual OGRFeature *       GetNextRawFeature();

  public:
                        OGRAeronavFAAIAPLayer(VSILFILE* fp, const char* pszLayerName);

    virtual void                ResetReading();
};

/************************************************************************/
/*                        OGRAeronavFAADataSource                       */
/************************************************************************/

class OGRAeronavFAADataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

  public:
                        OGRAeronavFAADataSource();
                        ~OGRAeronavFAADataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return nLayers; }
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );
};

/************************************************************************/
/*                         OGRAeronavFAADriver                          */
/************************************************************************/

class OGRAeronavFAADriver : public OGRSFDriver
{
  public:
                ~OGRAeronavFAADriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );
};


#endif /* ndef _OGR_AeronavFAA_H_INCLUDED */
