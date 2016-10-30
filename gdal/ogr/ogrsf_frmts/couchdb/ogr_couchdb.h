/******************************************************************************
 * $Id$
 *
 * Project:  CouchDB Translator
 * Purpose:  Definition of classes for OGR CouchDB / GeoCouch driver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef OGR_COUCHDB_H_INCLUDED
#define OGR_COUCHDB_H_INCLUDED

#include "ogrsf_frmts.h"
#include "cpl_http.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <json.h>
#ifdef __clang
#pragma clang diagnostic pop
#endif

#include <vector>
#include <map>

#define COUCHDB_ID_FIELD       0
#define COUCHDB_REV_FIELD      1
#define COUCHDB_FIRST_FIELD    2

typedef enum
{
    COUCHDB_TABLE_LAYER,
    COUCHDB_ROWS_LAYER
} CouchDBLayerType;

/************************************************************************/
/*                           OGRCouchDBLayer                            */
/************************************************************************/
class OGRCouchDBDataSource;

class OGRCouchDBLayer : public OGRLayer
{
protected:
    OGRCouchDBDataSource*       poDS;

    OGRFeatureDefn*             poFeatureDefn;
    OGRSpatialReference*        poSRS;

    int                         nNextInSeq;
    int                         nOffset;
    bool                        bEOF;

    json_object*                poFeatures;
    std::vector<json_object*>   aoFeatures;

    OGRFeature*                 GetNextRawFeature();
    OGRFeature*                 TranslateFeature( json_object* poObj );
    void                        ParseFieldValue(OGRFeature* poFeature,
                                                const char* pszKey,
                                                json_object* poValue);

    bool                        FetchNextRowsAnalyseDocs( json_object* poAnswerObj );
    virtual bool                FetchNextRows() = 0;

    bool                        bGeoJSONDocument;

    void                        BuildFeatureDefnFromDoc(json_object* poDoc);
    bool                        BuildFeatureDefnFromRows( json_object* poAnswerObj );

    virtual int                 GetFeaturesToFetch() { return atoi(CPLGetConfigOption("COUCHDB_PAGE_SIZE", "500")); }

  public:
                         OGRCouchDBLayer(OGRCouchDBDataSource* poDS);
    virtual ~OGRCouchDBLayer();

    virtual void                ResetReading();
    virtual OGRFeature *        GetNextFeature();

    virtual OGRFeatureDefn *    GetLayerDefn();

    virtual int                 TestCapability( const char * );

    virtual CouchDBLayerType    GetLayerType() = 0;

    virtual OGRErr              SetNextByIndex( GIntBig nIndex );

    virtual OGRSpatialReference * GetSpatialRef();
};

/************************************************************************/
/*                      OGRCouchDBTableLayer                            */
/************************************************************************/

class OGRCouchDBTableLayer : public OGRCouchDBLayer
{
    int                       nNextFIDForCreate;
    bool                      bInTransaction;
    std::vector<json_object*> aoTransactionFeatures;

    virtual bool              FetchNextRows();

    int                       bHasOGRSpatial;
    bool                      bHasGeocouchUtilsMinimalSpatialView;
    bool                      bServerSideAttributeFilteringWorks;
    bool                      FetchNextRowsSpatialFilter();

    bool                      bHasInstalledAttributeFilter;
    CPLString                 osURIAttributeFilter;
    std::map<CPLString, int>  oMapFilterFields;
    CPLString                 BuildAttrQueryURI(bool& bOutHasStrictComparisons);
    bool                      FetchNextRowsAttributeFilter();

    int                       GetTotalFeatureCount();
    int                       GetMaximumId();

    int                       nUpdateSeq;
    bool                      bAlwaysValid;
    int                       FetchUpdateSeq();

    int                       nCoordPrecision;

    OGRFeature*               GetFeature( const char* pszId );
    OGRErr                    DeleteFeature( OGRFeature* poFeature );

    protected:

    CPLString                 osName;
    CPLString                 osEscapedName;
    bool                      bMustWriteMetadata;
    bool                      bMustRunSpatialFilter;
    std::vector<CPLString>    aosIdsToFetch;
    bool                      bServerSideSpatialFilteringWorks;
    bool                      bHasLoadedMetadata;
    CPLString                 osMetadataRev;
    bool                      bExtentValid;

    bool                      bExtentSet;
    double                    dfMinX;
    double                    dfMinY;
    double                    dfMaxX;
    double                    dfMaxY;

    OGRwkbGeometryType        eGeomType;

    virtual void              WriteMetadata();
    virtual void              LoadMetadata();
    virtual bool              RunSpatialFilterQueryIfNecessary();

    public:
            OGRCouchDBTableLayer(OGRCouchDBDataSource* poDS,
                                 const char* pszName);
    virtual ~OGRCouchDBTableLayer();

    virtual void                ResetReading();

    virtual OGRFeatureDefn *    GetLayerDefn();

    virtual const char *        GetName() { return osName.c_str(); }

    virtual GIntBig             GetFeatureCount( int bForce = TRUE );
    virtual OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE);
    virtual OGRErr              GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce)
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual OGRFeature *        GetFeature( GIntBig nFID );

    virtual void                SetSpatialFilter( OGRGeometry * );
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom )
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }
    virtual OGRErr              SetAttributeFilter( const char * );

    virtual OGRErr              CreateField( OGRFieldDefn *poField,
                                             int bApproxOK = TRUE );
    virtual OGRErr              ICreateFeature( OGRFeature *poFeature );
    virtual OGRErr              ISetFeature( OGRFeature *poFeature );
    virtual OGRErr              DeleteFeature( GIntBig nFID );

    virtual OGRErr              StartTransaction();
    virtual OGRErr              CommitTransaction();
    virtual OGRErr              RollbackTransaction();

    virtual int                 TestCapability( const char * );

    void                        SetInfoAfterCreation( OGRwkbGeometryType eGType,
                                                      OGRSpatialReference* poSRSIn,
                                                      int nUpdateSeqIn,
                                                      bool bGeoJSONDocumentIn );

    void                        SetUpdateSeq(int nUpdateSeqIn) { nUpdateSeq = nUpdateSeqIn; };

    int                       HasFilterOnFieldOrCreateIfNecessary(const char* pszFieldName);

    void                        SetCoordinatePrecision( int nCoordPrecisionIn )
        { nCoordPrecision = nCoordPrecisionIn; }

    virtual CouchDBLayerType    GetLayerType() { return COUCHDB_TABLE_LAYER; }

    OGRErr            DeleteFeature( const char* pszId );
};

/************************************************************************/
/*                       OGRCouchDBRowsLayer                            */
/************************************************************************/

class OGRCouchDBRowsLayer : public OGRCouchDBLayer
{
    bool                      bAllInOne;

    virtual bool              FetchNextRows();

    public:
            OGRCouchDBRowsLayer( OGRCouchDBDataSource* poDS );
            virtual ~OGRCouchDBRowsLayer();

    virtual void                ResetReading();

    bool                        BuildFeatureDefn();

    virtual CouchDBLayerType    GetLayerType() { return COUCHDB_TABLE_LAYER; }
};

/************************************************************************/
/*                         OGRCouchDBDataSource                         */
/************************************************************************/

class OGRCouchDBDataSource : public OGRDataSource
{
  protected:
    char*               pszName;

    OGRLayer**          papoLayers;
    int                 nLayers;

    bool                bReadWrite;

    bool                bMustCleanPersistent;

    CPLString           osURL;
    CPLString           osUserPwd;

    json_object*        REQUEST(const char* pszVerb,
                                const char* pszURI,
                                const char* pszData);

    OGRLayer*           OpenDatabase(const char* pszLayerName = NULL);
    OGRLayer*           OpenView();
    void                DeleteLayer( const char *pszLayerName );

    OGRLayer *          ExecuteSQLStats( const char *pszSQLCommand );

  public:
                        OGRCouchDBDataSource();
                        virtual ~OGRCouchDBDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char* GetName() { return pszName; }

    virtual int         GetLayerCount() { return nLayers; }
    virtual OGRLayer*   GetLayer( int );
    virtual OGRLayer    *GetLayerByName(const char *);

    virtual int         TestCapability( const char * );

    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = NULL,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = NULL );
    virtual OGRErr      DeleteLayer(int);

    virtual OGRLayer*  ExecuteSQL( const char *pszSQLCommand,
                                   OGRGeometry *poSpatialFilter,
                                   const char *pszDialect );
    virtual void       ReleaseResultSet( OGRLayer * poLayer );

    bool                IsReadWrite() const { return bReadWrite; }

    char*                 GetETag(const char* pszURI);
    json_object*                GET(const char* pszURI);
    json_object*                PUT(const char* pszURI, const char* pszData);
    json_object*                POST(const char* pszURI, const char* pszData);
    json_object*                DELETE(const char* pszURI);

    const CPLString&            GetURL() const { return osURL; }

    static bool                 IsError(json_object* poAnswerObj,
                                        const char* pszErrorMsg);
    static bool                 IsOK   (json_object* poAnswerObj,
                                        const char* pszErrorMsg);
};

/************************************************************************/
/*                           OGRCouchDBDriver                           */
/************************************************************************/

class OGRCouchDBDriver : public OGRSFDriver
{
  public:
    virtual ~OGRCouchDBDriver();

    virtual const char*         GetName();
    virtual OGRDataSource*      Open( const char *, int );
    virtual OGRDataSource*      CreateDataSource( const char * pszName,
                                                  char **papszOptions );
    virtual int                 TestCapability( const char * );
};

#endif /* ndef OGR_COUCHDB_H_INCLUDED */
