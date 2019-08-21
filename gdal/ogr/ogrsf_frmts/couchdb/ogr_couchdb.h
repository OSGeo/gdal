/******************************************************************************
 * $Id$
 *
 * Project:  CouchDB Translator
 * Purpose:  Definition of classes for OGR CouchDB / GeoCouch driver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_json_header.h"
#include "cpl_http.h"

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

class OGRCouchDBLayer CPL_NON_FINAL: public OGRLayer
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
    static void                        ParseFieldValue(OGRFeature* poFeature,
                                                const char* pszKey,
                                                json_object* poValue);

    bool                        FetchNextRowsAnalyseDocs( json_object* poAnswerObj );
    virtual bool                FetchNextRows() = 0;

    bool                        bGeoJSONDocument;

    void                        BuildFeatureDefnFromDoc(json_object* poDoc);
    bool                        BuildFeatureDefnFromRows( json_object* poAnswerObj );

    virtual int                 GetFeaturesToFetch() { return atoi(CPLGetConfigOption("COUCHDB_PAGE_SIZE", "500")); }

  public:
    explicit OGRCouchDBLayer(OGRCouchDBDataSource* poDS);
    virtual ~OGRCouchDBLayer();

    virtual void                ResetReading() override;
    virtual OGRFeature *        GetNextFeature() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override;

    virtual int                 TestCapability( const char * ) override;

    virtual CouchDBLayerType    GetLayerType() = 0;

    virtual OGRErr              SetNextByIndex( GIntBig nIndex ) override;

    virtual OGRSpatialReference * GetSpatialRef() override;
};

/************************************************************************/
/*                      OGRCouchDBTableLayer                            */
/************************************************************************/

class OGRCouchDBTableLayer CPL_NON_FINAL: public OGRCouchDBLayer
{
    int                       nNextFIDForCreate;
    bool                      bInTransaction;
    std::vector<json_object*> aoTransactionFeatures;

    virtual bool              FetchNextRows() override;

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

    void                      BuildLayerDefn();

    public:
            OGRCouchDBTableLayer(OGRCouchDBDataSource* poDS,
                                 const char* pszName);
    virtual ~OGRCouchDBTableLayer();

    virtual void                ResetReading() override;

    virtual OGRFeatureDefn *    GetLayerDefn() override;

    virtual const char *        GetName() override { return osName.c_str(); }

    virtual GIntBig             GetFeatureCount( int bForce = TRUE ) override;
    virtual OGRErr              GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;
    virtual OGRErr              GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

    virtual OGRFeature *        GetFeature( GIntBig nFID ) override;

    virtual void                SetSpatialFilter( OGRGeometry * ) override;
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }
    virtual OGRErr              SetAttributeFilter( const char * ) override;

    virtual OGRErr              CreateField( OGRFieldDefn *poField,
                                             int bApproxOK = TRUE ) override;
    virtual OGRErr              ICreateFeature( OGRFeature *poFeature ) override;
    virtual OGRErr              ISetFeature( OGRFeature *poFeature ) override;
    virtual OGRErr              DeleteFeature( GIntBig nFID ) override;

    virtual OGRErr              StartTransaction() override;
    virtual OGRErr              CommitTransaction() override;
    virtual OGRErr              RollbackTransaction() override;

    virtual int                 TestCapability( const char * ) override;

    void                        SetInfoAfterCreation( OGRwkbGeometryType eGType,
                                                      OGRSpatialReference* poSRSIn,
                                                      int nUpdateSeqIn,
                                                      bool bGeoJSONDocumentIn );

    void                        SetUpdateSeq(int nUpdateSeqIn) { nUpdateSeq = nUpdateSeqIn; }

    int                       HasFilterOnFieldOrCreateIfNecessary(const char* pszFieldName);

    void                        SetCoordinatePrecision( int nCoordPrecisionIn )
        { nCoordPrecision = nCoordPrecisionIn; }

    virtual CouchDBLayerType    GetLayerType() override { return COUCHDB_TABLE_LAYER; }

    OGRErr            DeleteFeature( const char* pszId );
};

/************************************************************************/
/*                       OGRCouchDBRowsLayer                            */
/************************************************************************/

class OGRCouchDBRowsLayer final: public OGRCouchDBLayer
{
    bool                      bAllInOne;

    virtual bool              FetchNextRows() override;

    public:
            explicit OGRCouchDBRowsLayer( OGRCouchDBDataSource* poDS );
            virtual ~OGRCouchDBRowsLayer();

    virtual void                ResetReading() override;

    bool                        BuildFeatureDefn();

    virtual CouchDBLayerType    GetLayerType() override { return COUCHDB_TABLE_LAYER; }
};

/************************************************************************/
/*                         OGRCouchDBDataSource                         */
/************************************************************************/

class OGRCouchDBDataSource CPL_NON_FINAL: public OGRDataSource
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

    OGRLayer*           OpenDatabase(const char* pszLayerName = nullptr);
    OGRLayer*           OpenView();
    void                DeleteLayer( const char *pszLayerName );

    OGRLayer *          ExecuteSQLStats( const char *pszSQLCommand );

  public:
                        OGRCouchDBDataSource();
                        virtual ~OGRCouchDBDataSource();

    int                 Open( const char * pszFilename,
                              int bUpdate );

    virtual const char* GetName() override { return pszName; }

    virtual int         GetLayerCount() override { return nLayers; }
    virtual OGRLayer*   GetLayer( int ) override;
    virtual OGRLayer    *GetLayerByName(const char *) override;

    virtual int         TestCapability( const char * ) override;

    virtual OGRLayer   *ICreateLayer( const char *pszName,
                                     OGRSpatialReference *poSpatialRef = nullptr,
                                     OGRwkbGeometryType eGType = wkbUnknown,
                                     char ** papszOptions = nullptr ) override;
    virtual OGRErr      DeleteLayer(int) override;

    virtual OGRLayer*  ExecuteSQL( const char *pszSQLCommand,
                                   OGRGeometry *poSpatialFilter,
                                   const char *pszDialect ) override;
    virtual void       ReleaseResultSet( OGRLayer * poLayer ) override;

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

#endif /* ndef OGR_COUCHDB_H_INCLUDED */
