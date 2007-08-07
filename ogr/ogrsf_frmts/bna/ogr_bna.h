/******************************************************************************
 * $Id: ogr_bna.h 10646 2007-01-18 02:38:10Z warmerdam $
 *
 * Project:  BNA Translator
 * Purpose:  Definition of classes for OGR .bna driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007, Even Rouault
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

#ifndef _OGR_BNA_H_INCLUDED
#define _OGR_BNA_H_INLLUDED

#include "ogrsf_frmts.h"

#include "ogrbnaparser.h"

class OGRBNADataSource;

/************************************************************************/
/*                             OGRBNALayer                              */
/************************************************************************/

typedef struct
{
  int   offset;
  int   line;
} OffsetAndLine;

class OGRBNALayer : public OGRLayer
{
    OGRFeatureDefn     *poFeatureDefn;

    int                eof;
    int                failed;
    int                curLine;
    int                nNextFID;
    FILE               *fpBNA;
    int                nFeatures;
    int                partialIndexTable;
    OffsetAndLine*     offsetAndLineFeaturesTable;

    BNAFeatureType     bnaFeatureType;
    
    OGRFeature *       BuildFeatureFromBNARecord (BNARecord* record, long fid);
    void               FastParseUntil ( int interestFID);
  public:
    OGRBNALayer( const char *pszFilename,
                 const char* layerName,
                 BNAFeatureType bnaFeatureType,
                 OGRwkbGeometryType eLayerGeomType );
    ~OGRBNALayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }
    
    int                 GetFeatureCount( int );
    
    OGRFeature *        GetFeature( long nFID );

    int                 TestCapability( const char * );

};

/************************************************************************/
/*                           OGRBNADataSource                           */
/************************************************************************/

class OGRBNADataSource : public OGRDataSource
{
    char                *pszName;

    OGRBNALayer       **papoLayers;
    int                 nLayers;

    int                 bUpdate;
    
  public:
                        OGRBNADataSource();
                        ~OGRBNADataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );
    
    const char          *GetName() { return pszName; }

    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRBNADriver                             */
/************************************************************************/

class OGRBNADriver : public OGRSFDriver
{
  public:
                ~OGRBNADriver();

    const char *GetName();
    OGRDataSource *Open( const char *, int );
    int         TestCapability( const char * );
    
};


#endif /* ndef _OGR_BNA_H_INCLUDED */
