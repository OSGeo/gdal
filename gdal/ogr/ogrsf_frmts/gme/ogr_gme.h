/******************************************************************************
 * $Id: ogr_gft.h 25483 2013-01-10 17:06:59Z warmerdam $
 *
 * Project:  Google Maps Engine (GME) API Driver
 * Purpose:  GME driver declarations
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *           (derived from GFT driver by Even)
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam <warmerdam@pobox.com>
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

#ifndef _OGR_GME_H_INCLUDED
#define _OGR_GME_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_http.h"

#include <vector>

/************************************************************************/
/*                             OGRGMELayer                              */
/************************************************************************/
class OGRGMEDataSource;

class OGRGMELayer : public OGRLayer
{
protected:
    OGRGMEDataSource* poDS;

    OGRFeatureDefn*    poFeatureDefn;
    OGRSpatialReference *poSRS;

    int                nNextInSeq;

    int                iGeometryField;
    int                iLatitudeField;
    int                iLongitudeField;
    int                bHiddenGeometryField;

    OGRFeature *       GetNextRawFeature();

    int                nOffset;
    int                bEOF;
    virtual int                FetchNextRows() = 0;

    std::vector<CPLString> aosRows;

    int                 bFirstTokenIsFID;
    OGRFeature*         BuildFeatureFromSQL(const char* pszLine);

    static CPLString    LaunderColName(const char* pszColName);

  public:
                         OGRGMELayer(OGRGMEDataSource* poDS);
                        ~OGRGMELayer();

    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn();

    virtual int                 TestCapability( const char * );

    virtual OGRSpatialReference*GetSpatialRef();

    virtual const char *        GetGeometryColumn();

    virtual OGRErr              SetNextByIndex( long nIndex );

    const char *        GetDefaultGeometryColumnName() { return "geometry"; }

    static CPLString            PatchSQL(const char* pszSQL);

    int                         GetGeometryFieldIndex() { return iGeometryField; }
    int                         GetLatitudeFieldIndex() { return iLatitudeField; }
    int                         GetLongitudeFieldIndex() { return iLongitudeField; }

    int                         GetFeaturesToFetch() { return atoi(CPLGetConfigOption("GME_PAGE_SIZE", "500")); }
};

/************************************************************************/
/*                         OGRGMETableLayer                             */
/************************************************************************/

class OGRGMETableLayer : public OGRGMELayer
{
    CPLString         osTableName;
    CPLString         osTableId;
    CPLString         osGeomColumnName;

    int                bHasTriedCreateTable;
    void               CreateTableIfNecessary();

    CPLString           osWHERE;
    CPLString           osQuery;

    void                BuildWhere(void);

    CPLString          osTransaction;
    int                bInTransaction;
    int                nFeaturesInTransaction;

    int                FetchDescribe();
    virtual int                FetchNextRows();

    OGRwkbGeometryType eGTypeForCreation;

    std::vector<CPLString>  aosColumnInternalName;

    public:
            OGRGMETableLayer(OGRGMEDataSource* poDS,
                             const char* pszTableName,
                             const char* pszTableId = "");
            ~OGRGMETableLayer();

    virtual void                ResetReading();

    virtual OGRFeatureDefn *    GetLayerDefn();

    virtual const char *        GetName() { return osTableName.c_str(); }
    virtual int         GetFeatureCount( int bForce = TRUE );

    virtual OGRFeature *        GetFeature( long nFID );

    virtual void        SetSpatialFilter( OGRGeometry * );
    virtual OGRErr      SetAttributeFilter( const char * );

    const CPLString&            GetTableId() const { return osTableId; }

    virtual int                 TestCapability( const char * );

    void                SetGeometryType(OGRwkbGeometryType eGType);
};

/************************************************************************/
/*                           OGRGMEDataSource                           */
/************************************************************************/

class OGRGMEDataSource : public OGRDataSource
{
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

    int                 bReadWrite;

    int                 bUseHTTPS;

    CPLString           osAuth;
    CPLString           osAccessToken;
    CPLString           osRefreshToken;
    CPLString           osAPIKey;

    void                DeleteLayer( const char *pszLayerName );

    int                 bMustCleanPersistant;

    static CPLStringList ParseSimpleJson(const char *pszJSon);

  public:
                        OGRGMEDataSource();
                        ~OGRGMEDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char* GetName() { return pszName; }

    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer*   GetLayer( int );

    virtual int         TestCapability( const char * );

    CPLHTTPResult*      MakeRequest(const char *pszRequest);
    const CPLString&    GetAccessToken() const { return osAccessToken;}
    const char*         GetAPIURL() const;
    int                 IsReadWrite() const { return bReadWrite; }
    void                AddHTTPOptions(CPLStringList &oOptions);
};

/************************************************************************/
/*                             OGRGMEDriver                             */
/************************************************************************/

class OGRGMEDriver : public OGRSFDriver
{
  public:
                ~OGRGMEDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual OGRDataSource*      CreateDataSource( const char * pszName,
                                                  char **papszOptions );
    virtual int                 TestCapability( const char * );
};

#endif /* ndef _OGR_GME_H_INCLUDED */
