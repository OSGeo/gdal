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

#ifndef _OGR_ELASTIC_H_INCLUDED
#define _OGR_ELASTIC_H_INCLUDED

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "cpl_hash_set.h"
#include <vector>
#include <json.h>

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
    OGRFeatureDefn* poFeatureDefn;
    int bFeatureDefnFinalized;
    OGRElasticDataSource* poDS;
    CPLString sIndex;

    int bManualMapping;
    CPLString osWriteMapFilename;
    int bMappingWritten;
    CPLString osIndexName;
    CPLString osMappingName;
    CPLString osESSearch;
    int nBulkUpload;
    CPLString osPrecision;
    
    CPLString m_osFID;
    
    std::vector< std::vector<CPLString> > m_aaosFieldPaths;
    std::map< CPLString, int> m_aosMapToFieldIndex;

    std::vector< std::vector<CPLString> > m_aaosGeomFieldPaths;
    std::map< CPLString, int> m_aosMapToGeomFieldIndex;
    
    std::vector< OGRCoordinateTransformation* > m_apoCT;
    std::vector< int > m_abIsGeoPoint;
    ESGeometryTypeMapping eGeomTypeMapping;
    
    CPLString osScrollID;
    GIntBig iCurID, m_nNextFID;
    int iCurFeatureInPage;
    std::vector<OGRFeature*> apoCachedFeatures;
    int bEOF;
    
    json_object* m_poSpatialFilter;
    CPLString m_osJSONFilter;
    
    int bIgnoreSourceID;
    
    int bDotAsNestedField;

    int PushIndex();
    CPLString BuildMap();

    OGRErr WriteMapIfNecessary();
    OGRFeature * GetNextRawFeature();
    void BuildFeature(OGRFeature* poFeature, json_object* poSource,
                      CPLString osPath);
    void CreateFieldFromSchema(const char* pszName,
                               const char* pszPrefix,
                               std::vector<CPLString> aosPath,
                               json_object* poObj);
    void  AddOrUpdateField(const char* pszAttrName,
                           const char* pszKey,
                           json_object* poObj,
                           char chNestedAttributeSeparator,
                           std::vector<CPLString>& aosPath);

    CPLString BuildJSonFromFeature(OGRFeature *poFeature);
    
    static CPLString BuildPathFromArray(const std::vector<CPLString>& aosPath);
    
    void AddFieldDefn( const char* pszName,
                       OGRFieldType eType,
                       const std::vector<CPLString>& aosPath,
                       OGRFieldSubType eSubType = OFSTNone );
    void AddGeomFieldDefn( const char* pszName,
                           OGRwkbGeometryType eType,
                           const std::vector<CPLString>& aosPath,
                           int bIsGeoPoint );
public:
    OGRElasticLayer(
            const char* pszLayerName,
            const char* pszIndexName,
            const char* pszMappingName,
            OGRElasticDataSource* poDS,
            char** papszOptions,
            const char* pszESSearch = NULL);
    ~OGRElasticLayer();

    void ResetReading();
    OGRFeature * GetNextFeature();

    OGRErr ICreateFeature(OGRFeature *poFeature);
    OGRErr ISetFeature(OGRFeature *poFeature);
    OGRErr CreateField(OGRFieldDefn *poField, int bApproxOK);
    OGRErr CreateGeomField(OGRGeomFieldDefn *poField, int bApproxOK);

    const char* GetName() { return poFeatureDefn->GetName(); }
    OGRFeatureDefn * GetLayerDefn();
    const char* GetFIDColumn();

    int TestCapability(const char *);

    GIntBig GetFeatureCount(int bForce);
    
    virtual void        SetSpatialFilter( OGRGeometry *poGeom ) { SetSpatialFilter(0, poGeom); }
    virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom );
    virtual OGRErr      SetAttributeFilter(const char* pszFilter);
    
    virtual OGRErr      SyncToDisk();
    
    void FinalizeFeatureDefn(int bReadFeatures = TRUE);
    void InitFeatureDefnFromMapping(json_object* poSchema,
                                    const char* pszPrefix,
                                    const std::vector<CPLString>& aosPath);
    
    const CPLString& GetIndexName() const { return osIndexName; }
    const CPLString& GetMappingName() const { return osMappingName; }
    
    void                SetIgnoreSourceID(int bFlag) { bIgnoreSourceID = bFlag; }
    void                SetManualMapping() { bManualMapping = TRUE; }
    void                SetDotAsNestedField(int bFlag) { bDotAsNestedField = bFlag; }
    void                SetFID(const CPLString& m_osFIDIn) { m_osFID = m_osFIDIn; }
    void                SetNextFID(GIntBig nNextFID) { m_nNextFID = nNextFID; }
};

/************************************************************************/
/*                         OGRElasticDataSource                         */
/************************************************************************/

class OGRElasticDataSource : public GDALDataset {
    char* pszName;
    CPLString osURL;
    CPLString m_osFID;

    OGRElasticLayer** papoLayers;
    int nLayers;

public:
    OGRElasticDataSource();
    ~OGRElasticDataSource();

    int Open(GDALOpenInfo* poOpenInfo);

    int Create(const char *pszFilename,
            char **papszOptions);

    const char* GetURL() { return osURL.c_str(); }

    const char* GetName() {
        return pszName;
    }

    int GetLayerCount() {
        return nLayers;
    }
    OGRLayer* GetLayer(int);

    OGRLayer * ICreateLayer(const char * pszLayerName,
            OGRSpatialReference *poSRS,
            OGRwkbGeometryType eType,
            char ** papszOptions);
    virtual OGRErr      DeleteLayer( int iLayer );
    
    virtual OGRLayer *  ExecuteSQL( const char *pszSQLCommand,
                                            OGRGeometry *poSpatialFilter,
                                            const char *pszDialect );
    virtual void        ReleaseResultSet( OGRLayer * poLayer );

    int TestCapability(const char *);

    int UploadFile(const CPLString &url, const CPLString &data);
    void Delete(const CPLString &url);

    int bOverwrite;
    int nBulkUpload;
    char* pszWriteMap;
    char* pszMapping;
    int nBatchSize;
    int nFeatureCountToEstablishFeatureDefn;
    int bJSonField;
    int bFlattenNestedAttributes;
    
    json_object* RunRequest(const char* pszURL, const char* pszPostContent = NULL);
    const CPLString& GetFID() const { return m_osFID; }
};


#endif /* ndef _OGR_Elastic_H_INCLUDED */
