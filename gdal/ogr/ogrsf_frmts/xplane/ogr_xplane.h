/******************************************************************************
 * $Id: ogr_xplane.h $
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Definition of classes for OGR X-Plane aeronautical data driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault
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

#ifndef _OGR_XPLANE_H_INCLUDED
#define _OGR_XPLANE_H_INCLUDED

#include "ogrsf_frmts.h"

class OGRXPlaneReader;
class OGRXPlaneDataSource;

/************************************************************************/
/*                             OGRXPlaneLayer                           */
/************************************************************************/

class OGRXPlaneLayer : public OGRLayer
{
  private:
    int                nFID;
    int                nFeatureArraySize;
    int                nFeatureArrayMaxSize;
    int                nFeatureArrayIndex;
    
    OGRFeature**       papoFeatures;
    OGRSpatialReference *poSRS;
    
    OGRXPlaneDataSource* poDS;

  protected:
    OGRXPlaneReader*   poReader;
    OGRFeatureDefn*    poFeatureDefn;
                       OGRXPlaneLayer(const char* pszLayerName);

    void               RegisterFeature(OGRFeature* poFeature);

  public:
    virtual                   ~OGRXPlaneLayer();
    
    void                      SetDataSource(OGRXPlaneDataSource* poDS);

    void                      SetReader(OGRXPlaneReader* poReader);
    int                       IsEmpty() { return nFeatureArraySize == 0; }
    void                      AutoAdjustColumnsWidth();

    virtual void              ResetReading();
    virtual OGRFeature *      GetNextFeature();
    virtual OGRFeature *      GetFeature( long nFID );
    virtual OGRErr            SetNextByIndex( long nIndex );
    virtual int               GetFeatureCount( int bForce = TRUE );

    virtual OGRFeatureDefn *  GetLayerDefn();
    virtual int               TestCapability( const char * pszCap );
};


/************************************************************************/
/*                           OGRXPlaneDataSource                        */
/************************************************************************/

class OGRXPlaneDataSource : public OGRDataSource
{
    char*               pszName;

    OGRXPlaneLayer**    papoLayers;
    int                 nLayers;

    OGRXPlaneReader*    poReader;
    int                 bReadWholeFile;
    int                 bWholeFiledReadingDone;

    void                Reset();

  public:
                        OGRXPlaneDataSource();
                        ~OGRXPlaneDataSource();

    int                 Open( const char * pszFilename, int bReadWholeFile = TRUE );

    void                RegisterLayer( OGRXPlaneLayer* poLayer );

    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer*   GetLayer( int );
    virtual const char* GetName() { return pszName; }

    virtual int         TestCapability( const char * pszCap );
    
    void                ReadWholeFileIfNecessary();
};

/************************************************************************/
/*                             OGRXPlaneDriver                          */
/************************************************************************/

class OGRXPlaneDriver : public OGRSFDriver
{
  public:

    virtual const char* GetName();
    OGRDataSource*      Open( const char *, int );

    virtual int         TestCapability( const char * pszCap );
};


#endif /* ndef _OGR_XPLANE_H_INCLUDED */
