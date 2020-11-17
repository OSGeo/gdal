/******************************************************************************
 * $Id$
 *
 * Project:  Elasticsearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
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

#ifndef OGR_ELASTIC_H_INCLUDED
#define OGR_ELASTIC_H_INCLUDED

#include "ogrsf_frmts.h"

#include "cpl_json_header.h"
#include "cpl_hash_set.h"
#include "ogr_p.h"
#include "cpl_http.h"

#include <map>
#include <memory>
#include <set>
#include <vector>

typedef enum
{
    ES_GEOMTYPE_AUTO,
    ES_GEOMTYPE_GEO_POINT,
    ES_GEOMTYPE_GEO_SHAPE
} ESGeometryTypeMapping;

class OGRElasticDataSource;

class OGRESSortDesc
{
    public:
        CPLString osColumn;
        bool      bAsc;

        OGRESSortDesc( const CPLString& osColumnIn, bool bAscIn ) :
            osColumn(osColumnIn),
            bAsc(bAscIn) {}
};

/************************************************************************/
/*                          OGRElasticLayer                             */
/************************************************************************/

class OGRElasticLayer final: public OGRLayer {
    OGRElasticDataSource                *m_poDS;

    CPLString                            m_osIndexName;
    CPLString                            m_osMappingName;

    OGRFeatureDefn                      *m_poFeatureDefn;
    bool                                 m_bFeatureDefnFinalized;

    bool                                 m_bManualMapping;
    bool                                 m_bSerializeMapping;
    CPLString                            m_osWriteMapFilename;
    bool                                 m_bStoreFields;
    char                               **m_papszStoredFields;
    char                               **m_papszNotAnalyzedFields;
    char                               **m_papszNotIndexedFields;
    char                               **m_papszFieldsWithRawValue;

    CPLString                            m_osESSearch;
    std::vector<OGRESSortDesc>           m_aoSortColumns;

    CPLString                            m_osBulkContent;
    int                                  m_nBulkUpload;

    CPLString                            m_osFID;

    std::vector< std::vector<CPLString> > m_aaosFieldPaths;
    std::map< CPLString, int>             m_aosMapToFieldIndex;

    std::vector< std::vector<CPLString> > m_aaosGeomFieldPaths;
    std::map< CPLString, int>             m_aosMapToGeomFieldIndex;
    std::vector< OGRCoordinateTransformation* > m_apoCT;
    std::vector< int >                    m_abIsGeoPoint;
    ESGeometryTypeMapping                 m_eGeomTypeMapping;
    CPLString                             m_osPrecision;

    CPLString                             m_osScrollID;
    GIntBig                               m_iCurID;
    GIntBig                               m_nNextFID; // for creation
    int                                   m_iCurFeatureInPage;
    std::vector<OGRFeature*>              m_apoCachedFeatures;
    bool                                  m_bEOF;

    json_object*                          m_poSpatialFilter;
    CPLString                             m_osJSONFilter;
    bool                                  m_bFilterMustBeClientSideEvaluated;
    json_object*                          m_poJSONFilter;

    bool                                  m_bIgnoreSourceID;
    bool                                  m_bDotAsNestedField;

    bool                                  m_bAddPretty;
    bool                                  m_bGeoShapeAsGeoJSON;

    CPLString                             m_osSingleQueryTimeout;
    double                                m_dfSingleQueryTimeout = 0;
    double                                m_dfFeatureIterationTimeout = 0;
    //! Timestamp after which the query must be terminated
    double                                m_dfEndTimeStamp = 0;

    GIntBig                               m_nReadFeaturesSinceResetReading = 0;
    GIntBig                               m_nSingleQueryTerminateAfter = 0;
    GIntBig                               m_nFeatureIterationTerminateAfter = 0;
    CPLString                             m_osSingleQueryTerminateAfter;

    bool                                  m_bUseSingleQueryParams = false;

    bool                                  PushIndex();
    CPLString                             BuildMap();

    OGRErr                                WriteMapIfNecessary();
    OGRFeature                           *GetNextRawFeature();
    void                                  BuildFeature(OGRFeature* poFeature,
                                                       json_object* poSource,
                                                       CPLString osPath);
    void                                  CreateFieldFromSchema(const char* pszName,
                                                    const char* pszPrefix,
                                                    std::vector<CPLString> aosPath,
                                                    json_object* poObj);
    void                                  AddOrUpdateField(const char* pszAttrName,
                                                const char* pszKey,
                                                json_object* poObj,
                                                char chNestedAttributeSeparator,
                                                std::vector<CPLString>& aosPath);

    CPLString                             BuildMappingURL(bool bMappingApi);

    CPLString                             BuildJSonFromFeature(OGRFeature *poFeature);

    static CPLString                      BuildPathFromArray(const std::vector<CPLString>& aosPath);

    void                                  AddFieldDefn( const char* pszName,
                                            OGRFieldType eType,
                                            const std::vector<CPLString>& aosPath,
                                            OGRFieldSubType eSubType = OFSTNone );
    void                                  AddGeomFieldDefn( const char* pszName,
                                            OGRwkbGeometryType eType,
                                            const std::vector<CPLString>& aosPath,
                                            int bIsGeoPoint );

    CPLString                             BuildQuery(bool bCountOnly);
    json_object*                          GetValue( int nFieldIdx,
                                                    swq_expr_node* poValNode );
    json_object*                          TranslateSQLToFilter(swq_expr_node* poNode);
    json_object*                          BuildSort();

    void                                  AddTimeoutTerminateAfterToURL( CPLString& osURL );

public:
                        OGRElasticLayer( const char* pszLayerName,
                                         const char* pszIndexName,
                                         const char* pszMappingName,
                                         OGRElasticDataSource* poDS,
                                         char** papszOptions,
                                         const char* pszESSearch = nullptr);
                        virtual ~OGRElasticLayer();

    virtual void        ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;

    virtual OGRErr      ICreateFeature(OGRFeature *poFeature) override;
    virtual OGRErr      ISetFeature(OGRFeature *poFeature) override;
    virtual OGRErr      CreateField(OGRFieldDefn *poField, int bApproxOK) override;
    virtual OGRErr      CreateGeomField(OGRGeomFieldDefn *poField, int bApproxOK) override;

    virtual const char* GetName() override { return m_poFeatureDefn->GetName(); }
    virtual OGRFeatureDefn *GetLayerDefn() override;
    virtual const char *GetFIDColumn() override;

    virtual int         TestCapability(const char *) override;

    virtual GIntBig     GetFeatureCount(int bForce) override;

    virtual void        SetSpatialFilter( OGRGeometry *poGeom ) override { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) override;
    virtual OGRErr      SetAttributeFilter(const char* pszFilter) override;

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual OGRErr      SyncToDisk() override;

    void                FinalizeFeatureDefn(bool bReadFeatures = true);
    void                InitFeatureDefnFromMapping(json_object* poSchema,
                                    const char* pszPrefix,
                                    const std::vector<CPLString>& aosPath);

    const CPLString&    GetIndexName() const { return m_osIndexName; }
    const CPLString&    GetMappingName() const { return m_osMappingName; }

    void                SetIgnoreSourceID( bool bFlag ) { m_bIgnoreSourceID = bFlag; }
    void                SetManualMapping() { m_bManualMapping = true; }
    void                SetDotAsNestedField( bool bFlag ) { m_bDotAsNestedField = bFlag; }
    void                SetFID(const CPLString& m_osFIDIn) { m_osFID = m_osFIDIn; }
    void                SetNextFID(GIntBig nNextFID) { m_nNextFID = nNextFID; }

    OGRElasticLayer*    Clone() const;
    void                SetOrderBy( const std::vector<OGRESSortDesc>& v )
                                                        { m_aoSortColumns = v; }
};

/************************************************************************/
/*                         OGRElasticDataSource                         */
/************************************************************************/

class OGRElasticDataSource final: public GDALDataset {
    char               *m_pszName;
    CPLString           m_osURL;
    CPLString           m_osUserPwd;
    CPLString           m_osFID;

    std::set<CPLString> m_oSetLayers;
    std::vector<std::unique_ptr<OGRElasticLayer>> m_apoLayers;
    bool                m_bAllLayersListed = false;
    std::map<OGRLayer*, OGRLayer*> m_oMapResultSet;
    std::map<std::string, std::string> m_oMapHeadersFromEnv{};

    bool                CheckVersion();
    int                 GetLayerIndex( const char* pszName );
    void                FetchMapping(const char* pszIndexName);

public:
                            OGRElasticDataSource();
                            virtual ~OGRElasticDataSource();

    bool                m_bOverwrite;
    int                 m_nBulkUpload;
    char               *m_pszWriteMap;
    char               *m_pszMapping;
    int                 m_nBatchSize;
    int                 m_nFeatureCountToEstablishFeatureDefn;
    bool                m_bJSonField;
    bool                m_bFlattenNestedAttributes;
    int                 m_nMajorVersion = 0;
    int                 m_nMinorVersion = 0;

    int Open(GDALOpenInfo* poOpenInfo);

    int Create(const char *pszFilename,
               char **papszOptions);

    CPLHTTPResult*      HTTPFetch(const char* pszURL, CSLConstList papszOptions);

    const char         *GetURL() { return m_osURL.c_str(); }

    virtual const char *GetName() { return m_pszName; }

    virtual int         GetLayerCount() override;
    virtual OGRLayer   *GetLayer(int) override;
    virtual OGRLayer   *GetLayerByName(const char* pszName) override;

    virtual OGRLayer   *ICreateLayer(const char * pszLayerName,
                                    OGRSpatialReference *poSRS,
                                    OGRwkbGeometryType eType,
                                    char ** papszOptions) override;
    virtual OGRErr      DeleteLayer( int iLayer ) override;

    virtual OGRLayer   *ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect ) override;
    virtual void        ReleaseResultSet( OGRLayer * poLayer ) override;

    virtual int         TestCapability(const char *) override;

    bool                UploadFile(const CPLString &url,
                                   const CPLString &data,
                                   const CPLString &osVerb = CPLString());
    void                Delete(const CPLString &url);

    json_object*        RunRequest(const char* pszURL,
                                   const char* pszPostContent = nullptr,
                                   const std::vector<int>& anSilentedHTTPErrors = std::vector<int>());
    const CPLString&    GetFID() const { return m_osFID; }
};

#endif /* ndef _OGR_Elastic_H_INCLUDED */
