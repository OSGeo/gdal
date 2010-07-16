/******************************************************************************
 * $Id: ogr_csv.h 17495 2009-08-02 11:44:13Z rouault $
 *
 * Project:  PCIDSK Translator
 * Purpose:  Definition of classes for PCIDSK vector segment driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009,  Frank Warmerdam
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

#ifndef _OGR_PCIDSK_H_INCLUDED
#define _OGR_PCIDSK_H_INCLUDED

#include "ogrsf_frmts.h"
#include "pcidsk.h"
#include "pcidsk_vectorsegment.h"

class OGRPCIDSKDataSource;

/************************************************************************/
/*                             OGRPCIDSKLayer                              */
/************************************************************************/

class OGRPCIDSKLayer : public OGRLayer
{
    PCIDSK::PCIDSKVectorSegment *poVecSeg;
    PCIDSK::PCIDSKSegment       *poSeg;

    OGRFeatureDefn     *poFeatureDefn;

    OGRFeature *        GetNextUnfilteredFeature();

    int                 iRingStartField;
    PCIDSK::ShapeId     hLastShapeId;

    bool                bUpdateAccess;

    OGRSpatialReference *poSRS;

  public:
    OGRPCIDSKLayer( PCIDSK::PCIDSKSegment*, bool bUpdate );
    ~OGRPCIDSKLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();
    OGRFeature         *GetFeature( long nFeatureId );
    OGRErr              SetFeature( OGRFeature *poFeature );

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    int                 TestCapability( const char * );

    OGRErr              DeleteFeature( long nFID );
    OGRErr              CreateFeature( OGRFeature *poFeature );
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    virtual OGRSpatialReference *GetSpatialRef();
    int                 GetFeatureCount( int );
    OGRErr              GetExtent( OGREnvelope *psExtent, int bForce );
};

/************************************************************************/
/*                           OGRPCIDSKDataSource                           */
/************************************************************************/

class OGRPCIDSKDataSource : public OGRDataSource
{
    CPLString           osName;

    std::vector<OGRPCIDSKLayer*> apoLayers;

    bool                bUpdate;

    PCIDSK::PCIDSKFile  *poFile;
    
  public:
                        OGRPCIDSKDataSource();
                        ~OGRPCIDSKDataSource();

    int                 Open( const char * pszFilename, int bUpdate);
    
    const char          *GetName() { return osName; }

    int                 GetLayerCount() { return (int) apoLayers.size(); }
    OGRLayer            *GetLayer( int );

    int                 TestCapability( const char * );

    OGRLayer           *CreateLayer( const char *, OGRSpatialReference *,
                                     OGRwkbGeometryType, char ** );
};

/************************************************************************/
/*                           OGRPCIDSKDriver                            */
/************************************************************************/

class OGRPCIDSKDriver : public OGRSFDriver
{
  public:
                ~OGRPCIDSKDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );
    OGRDataSource *CreateDataSource( const char *pszName, 
                                     char **papszOptions );
    int         TestCapability( const char * );
};


#endif /* ndef _OGR_PCIDSK_H_INCLUDED */
