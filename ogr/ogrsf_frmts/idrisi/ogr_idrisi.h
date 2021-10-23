/******************************************************************************
 * $Id$
 *
 * Project:  Idrisi Translator
 * Purpose:  Definition of classes for OGR Idrisi driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
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

class OGRIdrisiLayer final: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRIdrisiLayer>
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

    OGRFeature *       GetNextRawFeature();

  public:
    OGRIdrisiLayer( const char* pszFilename,
                    const char* pszLayerName, VSILFILE* fp,
                    OGRwkbGeometryType eGeomType, const char* pszWTKString );
    virtual ~OGRIdrisiLayer();

    virtual void                ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRIdrisiLayer)

    virtual OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    virtual int                 TestCapability( const char * ) override;

    void SetExtent( double dfMinX, double dfMinY, double dfMaxX, double dfMaxY );
    virtual OGRErr GetExtent( OGREnvelope *psExtent, int bForce = TRUE ) override;
    virtual OGRErr GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce ) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual GIntBig         GetFeatureCount( int bForce = TRUE ) override;
};

/************************************************************************/
/*                        OGRIdrisiDataSource                           */
/************************************************************************/

class OGRIdrisiDataSource final: public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

  public:
    OGRIdrisiDataSource();
    virtual ~OGRIdrisiDataSource();

    int                 Open( const char * pszFilename );

    virtual const char*         GetName() override { return pszName; }

    virtual int                 GetLayerCount() override { return nLayers; }
    virtual OGRLayer*           GetLayer( int ) override;

    virtual int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                         OGRIdrisiDriver                              */
/************************************************************************/

class OGRIdrisiDriver final: public OGRSFDriver
{
  public:
    virtual ~OGRIdrisiDriver();

    virtual const char*         GetName() override;
    virtual OGRDataSource*      Open( const char *, int ) override;
    virtual int                 TestCapability( const char * ) override;
};

#endif // ndef OGR_IDRISI_H_INCLUDED
