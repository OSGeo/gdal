/******************************************************************************
 * $Id$
 *
 * Project:  Idrisi Translator
 * Purpose:  Definition of classes for OGR Idrisi driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_IDRISI_H_INCLUDED
#define OGR_IDRISI_H_INCLUDED

#include "ogrsf_frmts.h"

/************************************************************************/
/*                         OGRIdrisiLayer                               */
/************************************************************************/

class OGRIdrisiLayer : public OGRLayer
{
protected:
    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;
    OGRwkbGeometryType eGeomType;

    VSILFILE*          fp;
    VSILFILE*          fpAVL;
    bool               bEOF;

    int                nNextFID;

    bool               bExtentValid;
    double             dfMinX;
    double             dfMinY;
    double             dfMaxX;
    double             dfMaxY;

    unsigned int       nTotalFeatures;

    bool               Detect_AVL_ADC( const char* pszFilename );
    void               ReadAVLLine( OGRFeature* poFeature );

    virtual OGRFeature *       GetNextRawFeature();

  public:
    OGRIdrisiLayer( const char* pszFilename,
                    const char* pszLayerName, VSILFILE* fp,
                    OGRwkbGeometryType eGeomType, const char* pszWTKString );
    virtual ~OGRIdrisiLayer();

    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    virtual int                 TestCapability( const char * );

    void SetExtent( double dfMinX, double dfMinY, double dfMaxX, double dfMaxY );
    virtual OGRErr GetExtent( OGREnvelope *psExtent, int bForce = TRUE );
    virtual OGRErr GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual GIntBig         GetFeatureCount( int bForce = TRUE );
};

/************************************************************************/
/*                        OGRIdrisiDataSource                           */
/************************************************************************/

class OGRIdrisiDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

  public:
    OGRIdrisiDataSource();
    virtual ~OGRIdrisiDataSource();

    int                 Open( const char * pszFilename );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return nLayers; }
    virtual OGRLayer*           GetLayer( int );

    virtual int                 TestCapability( const char * );
};

/************************************************************************/
/*                         OGRIdrisiDriver                              */
/************************************************************************/

class OGRIdrisiDriver : public OGRSFDriver
{
  public:
    virtual ~OGRIdrisiDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );
};

#endif // ndef OGR_IDRISI_H_INCLUDED
