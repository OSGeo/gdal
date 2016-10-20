/******************************************************************************
 * $Id$
 *
 * Project:  ElasticSearch Translator
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
#include "ogr_p.h"
#include "cpl_hash_set.h"
#include <vector>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include <json.h>
#ifdef __clang
#pragma clang diagnostic pop
#endif


typedef enum
{
    ES_GEOMTYPE_AUTO,
    ES_GEOMTYPE_GEO_POINT,
    ES_GEOMTYPE_GEO_SHAPE
} ESGeometryTypeMapping;

class OGRElasticDataSource;

/************************************************************************/
/*                          OGRElasticLayer                             */
/************************************************************************/

class OGRElasticLayer : public OGRLayer {
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

    CPLString                            m_osESSearch;

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
    GIntBig                               m_nNextFID;
    int                                   m_iCurFeatureInPage;
    std::vector<OGRFeature*>              m_apoCachedFeatures;
    bool                                  m_bEOF;

    json_object*                          m_poSpatialFilter;
    CPLString                             m_osJSONFilter;

    bool                                  m_bIgnoreSourceID;
    bool                                  m_bDotAsNestedField;

    bool                                  m_bAddPretty;

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

public:
                        OGRElasticLayer( const char* pszLayerName,
                                         const char* pszIndexName,
                                         const char* pszMappingName,
                                         OGRElasticDataSource* poDS,
                                         char** papszOptions,
                                         const char* pszESSearch = NULL);
                        virtual ~OGRElasticLayer();

    virtual void        ResetReading();
    virtual OGRFeature *GetNextFeature();

    virtual OGRErr      ICreateFeature(OGRFeature *poFeature);
    virtual OGRErr      ISetFeature(OGRFeature *poFeature);
    virtual OGRErr      CreateField(OGRFieldDefn *poField, int bApproxOK);
    virtual OGRErr      CreateGeomField(OGRGeomFieldDefn *poField, int bApproxOK);

    virtual const char* GetName() { return m_poFeatureDefn->GetName(); }
    virtual OGRFeatureDefn *GetLayerDefn();
    virtual const char *GetFIDColumn();

    virtual int         TestCapability(const char *);

    virtual GIntBig     GetFeatureCount(int bForce);

    virtual void        SetSpatialFilter( OGRGeometry *poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom );
    virtual OGRErr      SetAttributeFilter(const char* pszFilter);

    virtual OGRErr      GetExtent(OGREnvelope *psExtent, int bForce = TRUE) { return GetExtent(0, psExtent, bForce); }
    virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce = TRUE);

    virtual OGRErr      SyncToDisk();

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
};

/************************************************************************/
/*                         OGRElasticDataSource                         */
/************************************************************************/

class OGRElasticDataSource : public GDALDataset {
    char               *m_pszName;
    CPLString           m_osURL;
    CPLString           m_osFID;

    OGRElasticLayer   **m_papoLayers;
    int                 m_nLayers;

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

    int Open(GDALOpenInfo* poOpenInfo);

    int Create(const char *pszFilename,
               char **papszOptions);

    const char         *GetURL() { return m_osURL.c_str(); }

    virtual const char *GetName() { return m_pszName; }

    virtual int         GetLayerCount() { return m_nLayers; }
    virtual OGRLayer   *GetLayer(int);

    virtual OGRLayer   *ICreateLayer(const char * pszLayerName,
                                    OGRSpatialReference *poSRS,
                                    OGRwkbGeometryType eType,
                                    char ** papszOptions);
    virtual OGRErr      DeleteLayer( int iLayer );

    virtual OGRLayer   *ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    virtual int         TestCapability(const char *);

    bool                 UploadFile(const CPLString &url, const CPLString &data);
    void                Delete(const CPLString &url);

    json_object*        RunRequest(const char* pszURL, const char* pszPostContent = NULL);
    const CPLString&    GetFID() const { return m_osFID; }
};


#endif /* ndef _OGR_Elastic_H_INCLUDED */
