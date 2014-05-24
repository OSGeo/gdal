/******************************************************************************
 * $Id$
 *
 * Project:  STS Translator
 * Purpose:  Definition of classes finding SDTS support into OGRDriver
 *           framework.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Frank Warmerdam
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

#ifndef _OGR_SDTS_H_INCLUDED
#define _OGR_SDTS_H_INCLUDED

#include "sdts_al.h"
#include "ogrsf_frmts.h"

class OGRSDTSDataSource;

/************************************************************************/
/*                             OGRSDTSLayer                             */
/************************************************************************/

class OGRSDTSLayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    SDTSTransfer       *poTransfer;
    int                 iLayer;
    SDTSIndexedReader  *poReader;

    OGRSDTSDataSource  *poDS;

    OGRFeature         *GetNextUnfilteredFeature();

    void                BuildPolygons();
    int                 bPolygonsBuilt;
    
  public:
                        OGRSDTSLayer( SDTSTransfer *, int, OGRSDTSDataSource*);
                        ~OGRSDTSLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

//    OGRFeature         *GetFeature( long nFeatureId );
    
    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

//    int                 GetFeatureCount( int );
    
    int                 TestCapability( const char * );
};

/************************************************************************/
/*                          OGRSDTSDataSource                           */
/************************************************************************/

class OGRSDTSDataSource : public OGRDataSource
{
    SDTSTransfer        *poTransfer;
    char                *pszName;

    int                 nLayers;
    OGRSDTSLayer        **papoLayers;

    OGRSpatialReference *poSRS;
    
  public:
                        OGRSDTSDataSource();
                        ~OGRSDTSDataSource();

    int                 Open( const char * pszFilename, int bTestOpen );
    
    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );
    int                 TestCapability( const char * );

    OGRSpatialReference *GetSpatialRef() { return poSRS; }
};

#endif /* ndef _OGR_SDTS_H_INCLUDED */
