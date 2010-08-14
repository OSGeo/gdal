/******************************************************************************
 * $Id$
 *
 * Project:  WFS Translator
 * Purpose:  Definition of classes for OGR .sua driver.
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

#ifndef _OGR_WFS_H_INCLUDED
#define _OGR_WFS_H_INCLUDED

#include "ogrsf_frmts.h"

CPLString FetchValueFromURL(const char* pszURL, const char* pszKey);
CPLString AddKVToURL(const char* pszURL, const char* pszKey, const char* pszValue);

/************************************************************************/
/*                             OGRWFSLayer                              */
/************************************************************************/

class OGRWFSDataSource;

class OGRWFSLayer : public OGRLayer
{
    OGRWFSDataSource*   poDS;

    OGRFeatureDefn*     poFeatureDefn;
    int                 bGotApproximateLayerDefn;

    int                  bAxisOrderAlreadyInverted;
    OGRSpatialReference *poSRS;

    char*               pszBaseURL;
    char*               pszName;
    char*               pszNS;
    char*               pszNSVal;

    OGRDataSource      *poBaseDS;
    OGRLayer           *poBaseLayer;
    int                 bHasFetched;
    int                 bReloadNeeded;

    CPLString           osGeometryColumnName;
    OGRwkbGeometryType  eGeomType;
    int nFeatures;

    CPLString           MakeGetFeatureURL(int nMaxFeatures, int bRequestHits);
    OGRDataSource*      FetchGetFeature(int nMaxFeatures);
    OGRFeatureDefn*     DescribeFeatureType();
    int                 ExecuteGetFeatureResultTypeHits();

    double              dfMinX, dfMinY, dfMaxX, dfMaxY;
    int                 bHasExtents;

    OGRGeometry        *poFetchedFilterGeom;

  public:
                        OGRWFSLayer(OGRWFSDataSource* poDS,
                                    OGRSpatialReference* poSRS,
                                    int bAxisOrderAlreadyInverted,
                                    const char* pszBaseURL,
                                    const char* pszName,
                                    const char* pszNS,
                                    const char* pszNSVal);
                        ~OGRWFSLayer();

    const char                 *GetName() { return pszName; }

    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn();

    virtual const char *GetGeometryColumn() { return osGeometryColumnName; }

    virtual int                 TestCapability( const char * );

    virtual OGRSpatialReference *GetSpatialRef();

    virtual void        SetSpatialFilter( OGRGeometry * );

    virtual OGRErr      SetAttributeFilter( const char * );

    virtual int         GetFeatureCount( int bForce = TRUE );

    void                SetExtents(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY);
    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE);

};

/************************************************************************/
/*                           OGRWFSDataSource                           */
/************************************************************************/

class OGRWFSDataSource : public OGRDataSource
{
    char*               pszName;

    OGRWFSLayer**       papoLayers;
    int                 nLayers;

    int                 bGetFeatureSupportHits;
    CPLString           osVersion;
    int                 bNeedNAMESPACE;

  public:
                        OGRWFSDataSource();
                        ~OGRWFSDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char*         GetName() { return pszName; }

    virtual int                 GetLayerCount() { return nLayers; }
    virtual OGRLayer*           GetLayer( int );
    virtual OGRLayer*           GetLayerByName(const char* pszLayerName);

    virtual int                 TestCapability( const char * );

    int                         GetFeatureSupportHits() { return bGetFeatureSupportHits; }
    const char                 *GetVersion() { return osVersion.c_str(); }

    int                         IsOldDeegree(const char* pszErrorString);
    int                         GetNeedNAMESPACE() { return bNeedNAMESPACE; }
};

/************************************************************************/
/*                             OGRWFSDriver                             */
/************************************************************************/

class OGRWFSDriver : public OGRSFDriver
{
  public:
                ~OGRWFSDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual int                 TestCapability( const char * );
};


#endif /* ndef _OGR_WFS_H_INCLUDED */
