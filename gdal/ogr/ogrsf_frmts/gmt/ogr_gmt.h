/******************************************************************************
 * $Id: ogr_mem.h 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Private definitions within the OGR GMT driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGRGMT_H_INCLUDED
#define _OGRGMT_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "cpl_string.h"

/************************************************************************/
/*                             OGRGmtLayer                              */
/************************************************************************/

class OGRGmtLayer : public OGRLayer
{
    OGRSpatialReference *poSRS;
    OGRFeatureDefn     *poFeatureDefn;
    
    int                 iNextFID;

    OGRwkbGeometryType  eWkbType;

    int                 bUpdate;
    int                 bHeaderComplete;

    int                 bRegionComplete;
    OGREnvelope         sRegion;
    vsi_l_offset        nRegionOffset;

    VSILFILE           *fp;

    int                 ReadLine();
    CPLString           osLine;
    char              **papszKeyedValues;

    int                 ScanAheadForHole();
    int                 NextIsFeature();

    OGRFeature         *GetNextRawFeature();

    OGRErr              WriteGeometry( OGRGeometryH hGeom, int bHaveAngle );
    OGRErr              CompleteHeader( OGRGeometry * );

  public:
    int                 bValidFile;

                        OGRGmtLayer( const char *pszFilename, int bUpdate );
                        ~OGRGmtLayer();

    void                ResetReading();
    OGRFeature *        GetNextFeature();

    OGRFeatureDefn *    GetLayerDefn() { return poFeatureDefn; }

    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce);

    OGRErr              CreateFeature( OGRFeature *poFeature );
    
    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE );

    int                 TestCapability( const char * );
};

/************************************************************************/
/*                           OGRGmtDataSource                           */
/************************************************************************/

class OGRGmtDataSource : public OGRDataSource
{
    OGRGmtLayer       **papoLayers;
    int                 nLayers;
    
    char                *pszName;

    int                 bUpdate;

  public:
                        OGRGmtDataSource();
                        ~OGRGmtDataSource();

    int                 Open( const char *pszFilename, int bUpdate );
    int                 Create( const char *pszFilename, char **papszOptions );

    const char          *GetName() { return pszName; }
    int                 GetLayerCount() { return nLayers; }
    OGRLayer            *GetLayer( int );

    virtual OGRLayer    *ICreateLayer( const char *, 
                                      OGRSpatialReference * = NULL,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = NULL );
    int                 TestCapability( const char * );
};

/************************************************************************/
/*                             OGRGmtDriver                             */
/************************************************************************/

class OGRGmtDriver : public OGRSFDriver
{
  public:
                ~OGRGmtDriver();
                
    const char *GetName();
    OGRDataSource *Open( const char *, int );

    virtual OGRDataSource *CreateDataSource( const char *pszName,
                                             char ** = NULL );
    
    int                 TestCapability( const char * );
};


#endif /* ndef _OGRGMT_H_INCLUDED */

