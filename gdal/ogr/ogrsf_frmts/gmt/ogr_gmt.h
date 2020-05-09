/******************************************************************************
 * $Id$
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

#ifndef OGRGMT_H_INCLUDED
#define OGRGMT_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "cpl_string.h"

/************************************************************************/
/*                             OGRGmtLayer                              */
/************************************************************************/

class OGRGmtLayer final: public OGRLayer, public OGRGetNextFeatureThroughRaw<OGRGmtLayer>
{
    OGRSpatialReference *poSRS;
    OGRFeatureDefn     *poFeatureDefn;

    int                 iNextFID;

    bool                bUpdate;
    bool                bHeaderComplete;

    bool                bRegionComplete;
    OGREnvelope         sRegion;
    vsi_l_offset        nRegionOffset;

    VSILFILE           *fp;

    bool                ReadLine();
    CPLString           osLine;
    char              **papszKeyedValues;

    bool                ScanAheadForHole();
    bool                NextIsFeature();

    OGRFeature         *GetNextRawFeature();

    OGRErr              WriteGeometry( OGRGeometryH hGeom, bool bHaveAngle );
    OGRErr              CompleteHeader( OGRGeometry * );

  public:
    bool                bValidFile;

                        OGRGmtLayer( const char *pszFilename, int bUpdate );
                        virtual ~OGRGmtLayer();

    void                ResetReading() override;
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRGmtLayer)

    OGRFeatureDefn *    GetLayerDefn() override { return poFeatureDefn; }

    OGRErr              GetExtent(OGREnvelope *psExtent, int bForce) override;
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    OGRErr              ICreateFeature( OGRFeature *poFeature ) override;

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;

    int                 TestCapability( const char * ) override;
};

/************************************************************************/
/*                           OGRGmtDataSource                           */
/************************************************************************/

class OGRGmtDataSource final: public OGRDataSource
{
    OGRGmtLayer       **papoLayers;
    int                 nLayers;

    char                *pszName;

    bool                bUpdate;

  public:
                        OGRGmtDataSource();
                        virtual ~OGRGmtDataSource();

    int                 Open( const char *pszFilename, int bUpdate );
    int                 Create( const char *pszFilename, char **papszOptions );

    const char          *GetName() override { return pszName; }
    int                 GetLayerCount() override { return nLayers; }
    OGRLayer            *GetLayer( int ) override;

    virtual OGRLayer    *ICreateLayer( const char *,
                                      OGRSpatialReference * = nullptr,
                                      OGRwkbGeometryType = wkbUnknown,
                                      char ** = nullptr ) override;
    int                 TestCapability( const char * ) override;
};

#endif /* ndef OGRGMT_H_INCLUDED */
