/******************************************************************************
 * $Id$
 *
 * Project:  X-Plane aeronautical data reader
 * Purpose:  Definition of classes for OGR X-Plane aeronautical data driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_XPLANE_H_INCLUDED
#define OGR_XPLANE_H_INCLUDED

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

    explicit           OGRXPlaneLayer(const char* pszLayerName);

    void               RegisterFeature(OGRFeature* poFeature);

  public:
    virtual                   ~OGRXPlaneLayer();

    void                      SetDataSource(OGRXPlaneDataSource* poDS);

    void                      SetReader(OGRXPlaneReader* poReader);
    int                       IsEmpty() { return nFeatureArraySize == 0; }
    void                      AutoAdjustColumnsWidth();

    virtual void              ResetReading() override;
    virtual OGRFeature *      GetNextFeature() override;
    virtual OGRFeature *      GetFeature( GIntBig nFID ) override;
    virtual OGRErr            SetNextByIndex( GIntBig nIndex ) override;
    virtual GIntBig           GetFeatureCount( int bForce = TRUE ) override;

    virtual OGRFeatureDefn *  GetLayerDefn() override;
    virtual int               TestCapability( const char * pszCap ) override;
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
    bool                bReadWholeFile;
    bool                bWholeFiledReadingDone;

    void                Reset();

  public:
                        OGRXPlaneDataSource();
                        virtual ~OGRXPlaneDataSource();

    int                 Open( const char * pszFilename, int bReadWholeFile = TRUE );

    void                RegisterLayer( OGRXPlaneLayer* poLayer );

    virtual int         GetLayerCount() override { return nLayers; }
    virtual OGRLayer*   GetLayer( int ) override;
    virtual const char* GetName() override { return pszName; }

    virtual int         TestCapability( const char * pszCap ) override;

    void                ReadWholeFileIfNecessary();
};

/************************************************************************/
/*                             OGRXPlaneDriver                          */
/************************************************************************/

class OGRXPlaneDriver : public OGRSFDriver
{
  public:

    virtual const char* GetName() override;
    OGRDataSource*      Open( const char *, int ) override;

    virtual int         TestCapability( const char * pszCap ) override;
};

#endif /* ndef OGR_XPLANE_H_INCLUDED */
