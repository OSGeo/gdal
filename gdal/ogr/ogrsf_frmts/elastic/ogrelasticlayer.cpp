/******************************************************************************
 * $Id$
 *
 * Project:  ElasticSearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_elastic.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "../geojson/ogrgeojsonwriter.h"
#include "../geojson/ogrgeojsonreader.h"
#include "../geojson/ogrgeojsonutils.h"
#include "../xplane/ogr_xplane_geo_utils.h"
#include <set>

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRElasticLayer()                          */
/************************************************************************/

OGRElasticLayer::OGRElasticLayer(const char* pszLayerName,
                                 const char* pszIndexName,
                                 const char* pszMappingName,
                                 OGRElasticDataSource* poDS,
                                 char** papszOptions,
                                 const char* pszESSearch)
{
    m_poDS = poDS;

    m_osIndexName = pszIndexName ? pszIndexName : "";
    m_osMappingName = pszMappingName ? pszMappingName : "";
    m_osESSearch = pszESSearch ? pszESSearch : "";
    m_osWriteMapFilename = CSLFetchNameValueDef(papszOptions, "WRITE_MAPPING",
                                      m_poDS->m_pszWriteMap ? m_poDS->m_pszWriteMap : "");

    m_eGeomTypeMapping = ES_GEOMTYPE_AUTO;
    const char* pszESGeomType = CSLFetchNameValue(papszOptions, "GEOM_MAPPING_TYPE");
    if( pszESGeomType != NULL )
    {
        if( EQUAL(pszESGeomType, "GEO_POINT") )
            m_eGeomTypeMapping = ES_GEOMTYPE_GEO_POINT;
        else if( EQUAL(pszESGeomType, "GEO_SHAPE") )
            m_eGeomTypeMapping = ES_GEOMTYPE_GEO_SHAPE;
    }
    m_nBulkUpload = m_poDS->m_nBulkUpload;
    if( CSLFetchBoolean(papszOptions, "BULK_INSERT", TRUE) )
    {
        m_nBulkUpload = atoi(CSLFetchNameValueDef(papszOptions, "BULK_SIZE", "1000000"));
    }
    
    m_osPrecision = CSLFetchNameValueDef(papszOptions, "GEOM_PRECISION", "");
    m_bStoreFields = CPL_TO_BOOL(CSLFetchBoolean(papszOptions, "STORE_FIELDS", false));
    
    const char* pszStoredFields = CSLFetchNameValue(papszOptions, "STORED_FIELDS");
    if( pszStoredFields )
        m_papszStoredFields = CSLTokenizeString2(pszStoredFields, ",", 0);
    else
        m_papszStoredFields = NULL;
    
    const char* pszNotAnalyzedFields = CSLFetchNameValue(papszOptions, "NOT_ANALYZED_FIELDS");
    if( pszNotAnalyzedFields )
        m_papszNotAnalyzedFields = CSLTokenizeString2(pszNotAnalyzedFields, ",", 0);
    else
        m_papszNotAnalyzedFields = NULL;
    
    const char* pszNotIndexedFields = CSLFetchNameValue(papszOptions, "NOT_INDEXED_FIELDS");
    if( pszNotIndexedFields )
        m_papszNotIndexedFields = CSLTokenizeString2(pszNotIndexedFields, ",", 0);
    else
        m_papszNotIndexedFields = NULL;

    m_poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType(wkbNone);
    
    AddFieldDefn("_id", OFTString, std::vector<CPLString>());
    
    if( m_osESSearch.size() )
    {
        AddFieldDefn("_index", OFTString, std::vector<CPLString>());
        AddFieldDefn("_type", OFTString, std::vector<CPLString>());
    }
    
    m_bFeatureDefnFinalized = FALSE;
    m_bSerializeMapping = FALSE;
    m_bManualMapping = FALSE;
    m_bDotAsNestedField = TRUE;

    m_iCurID = 0;
    m_nNextFID = -1;
    m_iCurFeatureInPage = 0;
    m_bEOF = FALSE;
    m_poSpatialFilter = NULL;
    m_bIgnoreSourceID = FALSE;

    // Undocumented. Only usefull for developers
    m_bAddPretty = CSLTestBoolean(CPLGetConfigOption("ES_ADD_PRETTY", "FALSE"));

    ResetReading();
    return;
}

/************************************************************************/
/*                         ~OGRElasticLayer()                           */
/************************************************************************/

OGRElasticLayer::~OGRElasticLayer() {
    SyncToDisk();

    ResetReading();
    
    json_object_put(m_poSpatialFilter);

    for(int i=0;i<(int)m_apoCT.size();i++)
        delete m_apoCT[i];

    m_poFeatureDefn->Release();
    
    CSLDestroy(m_papszStoredFields);
    CSLDestroy(m_papszNotAnalyzedFields);
    CSLDestroy(m_papszNotIndexedFields);
}

/************************************************************************/
/*                              AddFieldDefn()                          */
/************************************************************************/

void OGRElasticLayer::AddFieldDefn( const char* pszName,
                                    OGRFieldType eType,
                                    const std::vector<CPLString>& aosPath,
                                    OGRFieldSubType eSubType )
{
    OGRFieldDefn oFieldDefn(pszName, eType);
    oFieldDefn.SetSubType(eSubType);
    if( eSubType == OFSTBoolean )
        oFieldDefn.SetWidth(1);
    m_aaosFieldPaths.push_back(aosPath);
    if( aosPath.size() )
        m_aosMapToFieldIndex[ BuildPathFromArray(aosPath) ] = m_poFeatureDefn->GetFieldCount();
    m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
}

/************************************************************************/
/*                           AddGeomFieldDefn()                         */
/************************************************************************/

void OGRElasticLayer::AddGeomFieldDefn( const char* pszName,
                                        OGRwkbGeometryType eType,
                                        const std::vector<CPLString>& aosPath,
                                        int bIsGeoPoint )
{
    OGRGeomFieldDefn oFieldDefn(pszName, eType);
    m_aaosGeomFieldPaths.push_back(aosPath);
    m_aosMapToGeomFieldIndex[ BuildPathFromArray(aosPath) ] = m_poFeatureDefn->GetGeomFieldCount();
    m_abIsGeoPoint.push_back(bIsGeoPoint);

    OGRSpatialReference* poSRS_WGS84 = new OGRSpatialReference();
    poSRS_WGS84->SetFromUserInput(SRS_WKT_WGS84);
    oFieldDefn.SetSpatialRef(poSRS_WGS84);
    poSRS_WGS84->Dereference();
    
    m_poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);

    m_apoCT.push_back(NULL);
}

/************************************************************************/
/*                     InitFeatureDefnFromMapping()                     */
/************************************************************************/

void OGRElasticLayer::InitFeatureDefnFromMapping(json_object* poSchema,
                                                 const char* pszPrefix,
                                                 const std::vector<CPLString>& aosPath)
{
    json_object* poTopProperties = json_object_object_get(poSchema, "properties");
    if( poTopProperties == NULL || json_object_get_type(poTopProperties) != json_type_object )
        return;
    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC( poTopProperties, it )
    {
        json_object* poProperties = json_object_object_get(it.val, "properties");
        if( poProperties && json_object_get_type(poProperties) == json_type_object )
        {
            json_object* poType = json_ex_get_object_by_path(poProperties, "coordinates.type");
            if( poType && json_object_get_type(poType) == json_type_string &&
                strcmp(json_object_get_string(poType), "geo_point") == 0 )
            {
                CPLString osFieldName;
                if( pszPrefix[0] )
                {
                    osFieldName = pszPrefix;
                    osFieldName += ".";
                }
                osFieldName += it.key;

                if( m_poFeatureDefn->GetGeomFieldIndex(osFieldName) < 0 )
                {
                    std::vector<CPLString> aosNewPaths = aosPath;
                    aosNewPaths.push_back(osFieldName);
                    aosNewPaths.push_back("coordinates");

                    AddGeomFieldDefn(osFieldName, wkbPoint, aosNewPaths, TRUE);
                }

                continue;
            }

            if( aosPath.size() == 0 && m_osMappingName == "FeatureCollection" && strcmp(it.key, "properties") == 0 )
            {
                std::vector<CPLString> aosNewPaths = aosPath;
                aosNewPaths.push_back(it.key);
                
                InitFeatureDefnFromMapping(it.val, pszPrefix, aosNewPaths);
                
                continue;
            }
            else if( m_poDS->m_bFlattenNestedAttributes )
            {
                std::vector<CPLString> aosNewPaths = aosPath;
                aosNewPaths.push_back(it.key);
                
                CPLString osPrefix;
                if( pszPrefix[0] )
                {
                    osPrefix = pszPrefix;
                    osPrefix += ".";
                }
                osPrefix += it.key;
                
                InitFeatureDefnFromMapping(it.val, osPrefix, aosNewPaths);
                
                continue;
            }
        }

        if( aosPath.size() == 0 && EQUAL(it.key, m_poDS->GetFID()) )
        {
            m_osFID = it.key;
        }
        else
        {
            CreateFieldFromSchema(it.key, pszPrefix, aosPath, it.val);
        }
    }

    if( aosPath.size() == 0 )
    {
        json_object* poMeta = json_object_object_get(poSchema, "_meta");
        if( poMeta && json_object_get_type(poMeta) == json_type_object )
        {
            json_object* poFID = json_object_object_get(poMeta, "fid");
            if( poFID && json_object_get_type(poFID) == json_type_string )
                m_osFID = json_object_get_string(poFID);

            json_object* poGeomFields = json_object_object_get(poMeta, "geomfields");
            if( poGeomFields && json_object_get_type(poGeomFields) == json_type_object )
            {
                for( int i=0; i< m_poFeatureDefn->GetGeomFieldCount(); i++ )
                {
                    json_object* poObj = json_object_object_get(poGeomFields,
                            m_poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
                    if( poObj && json_object_get_type(poObj) == json_type_string )
                    {
                        OGRwkbGeometryType eType = OGRFromOGCGeomType(json_object_get_string(poObj));
                        if( eType != wkbUnknown )
                            m_poFeatureDefn->GetGeomFieldDefn(i)->SetType(eType);
                    }
                }
            }

            json_object* poFields = json_object_object_get(poMeta, "fields");
            if( poFields && json_object_get_type(poFields) == json_type_object )
            {
                for( int i=0; i< m_poFeatureDefn->GetFieldCount(); i++ )
                {
                    json_object* poObj = json_object_object_get(poFields,
                            m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
                    if( poObj && json_object_get_type(poObj) == json_type_string )
                    {
                        for(int j=0; j<=OFTMaxType;j++)
                        {
                            if( EQUAL(OGR_GetFieldTypeName((OGRFieldType)j),
                                      json_object_get_string(poObj)) )
                            {
                                m_poFeatureDefn->GetFieldDefn(i)->SetType((OGRFieldType)j);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                        CreateFieldFromSchema()                       */
/************************************************************************/

void OGRElasticLayer::CreateFieldFromSchema(const char* pszName,
                                            const char* pszPrefix,
                                            std::vector<CPLString> aosPath,
                                            json_object* poObj)
{
    const char* pszType = "";
    json_object* poType = json_object_object_get(poObj, "type");
    if( poType && json_object_get_type(poType) == json_type_string )
    {
        pszType = json_object_get_string(poType);
    }

    CPLString osFieldName;
    if( pszPrefix[0] )
    {
        osFieldName = pszPrefix;
        osFieldName += ".";
    }
    osFieldName += pszName;

    if( EQUAL(pszType, "geo_point") || EQUAL(pszType, "geo_shape") )
    {
        if( m_poFeatureDefn->GetGeomFieldIndex(osFieldName) >= 0 )
            return;

        aosPath.push_back(pszName);
        AddGeomFieldDefn(osFieldName,
                         EQUAL(pszType, "geo_point") ? wkbPoint : wkbUnknown,
                         aosPath, EQUAL(pszType, "geo_point"));
    }
    else if( !( aosPath.size() == 0 && m_osMappingName == "FeatureCollection" ) )
    {
        if( m_poFeatureDefn->GetFieldIndex(osFieldName) >= 0 )
            return;

        OGRFieldType eType = OFTString;
        OGRFieldSubType eSubType = OFSTNone;
        if( EQUAL(pszType, "integer") )
            eType = OFTInteger;
        else if( EQUAL(pszType, "boolean") )
        {
            eType = OFTInteger;
            eSubType = OFSTBoolean;
        }
        else if( EQUAL(pszType, "long") )
            eType = OFTInteger64;
        else if( EQUAL(pszType, "float") )
            eType = OFTReal;
        else if( EQUAL(pszType, "double") )
            eType = OFTReal;
        else if( EQUAL(pszType, "date") )
        {
            eType = OFTDateTime;
            json_object* poFormat = json_object_object_get(poObj, "format");
            if( poFormat && json_object_get_type(poFormat) == json_type_string )
            {
                const char* pszFormat = json_object_get_string(poFormat);
                if( EQUAL(pszFormat, "HH:mm:ss.SSS") || EQUAL(pszFormat, "time") )
                    eType = OFTTime;
                else if( EQUAL(pszFormat, "yyyy/MM/dd") || EQUAL(pszFormat, "date") )
                    eType = OFTDate;
            }
        }
        else if( EQUAL(pszType, "binary") )
            eType = OFTBinary;

        aosPath.push_back( pszName );
        AddFieldDefn(osFieldName, eType, aosPath, eSubType);
    }
}

/************************************************************************/
/*                        FinalizeFeatureDefn()                         */
/************************************************************************/

void OGRElasticLayer::FinalizeFeatureDefn(int bReadFeatures)
{
    if( m_bFeatureDefnFinalized )
        return;
    
    m_bFeatureDefnFinalized = TRUE;
    
    int nFeatureCountToEstablishFeatureDefn = m_poDS->m_nFeatureCountToEstablishFeatureDefn;
    if( m_osESSearch.size() && nFeatureCountToEstablishFeatureDefn <= 0 )
        nFeatureCountToEstablishFeatureDefn = 1;
    std::set< std::pair<CPLString, CPLString> > oVisited;

    if( bReadFeatures && nFeatureCountToEstablishFeatureDefn != 0 )
    {
        //CPLDebug("ES", "Try to get %d features to establish feature definition",
        //         FeatureCountToEstablishFeatureDefn);
        int bFirst = TRUE;
        int nAlreadyQueried = 0;
        while( true )
        {
            json_object* poResponse;
            CPLString osRequest, osPostData;
            if( bFirst )
            {
                bFirst = FALSE;
                if(  m_osESSearch.size() )
                {
                    osRequest = CPLSPrintf("%s/_search?scroll=1m&size=%d",
                           m_poDS->GetURL(), m_poDS->m_nBatchSize);
                    osPostData = m_osESSearch;
                }
                else
                    osRequest = CPLSPrintf("%s/%s/%s/_search?scroll=1m&size=%d",
                           m_poDS->GetURL(), m_osIndexName.c_str(),
                           m_osMappingName.c_str(), m_poDS->m_nBatchSize);
            }
            else
            {
                if( m_osScrollID.size() == 0 )
                    break;
                osRequest = CPLSPrintf("%s/_search/scroll?scroll=1m&size=%d&scroll_id=%s",
                               m_poDS->GetURL(), m_poDS->m_nBatchSize, m_osScrollID.c_str());
            }

            if( m_bAddPretty )
                osRequest += "&pretty";
            poResponse = m_poDS->RunRequest(osRequest, osPostData);
            if( poResponse == NULL )
            {
                break;
            }
            json_object* poScrollID = json_object_object_get(poResponse, "_scroll_id");
            if( poScrollID )
            {
                const char* pszScrollID = json_object_get_string(poScrollID);
                if( pszScrollID )
                    m_osScrollID = pszScrollID;
            }

            json_object* poHits = json_ex_get_object_by_path(poResponse, "hits.hits");
            if( poHits == NULL || json_object_get_type(poHits) != json_type_array )
            {
                json_object_put(poResponse);
                break;
            }
            int nHits = json_object_array_length(poHits);
            if( nHits == 0 )
            {
                m_osScrollID = "";
                json_object_put(poResponse);
                break;
            }
            for(int i=0;i<nHits;i++)
            {
                json_object* poHit = json_object_array_get_idx(poHits, i);
                if( poHit == NULL || json_object_get_type(poHit) != json_type_object )
                {
                    continue;
                }
                json_object* poSource = json_object_object_get(poHit, "_source");
                if( poSource == NULL || json_object_get_type(poSource) != json_type_object )
                {
                    continue;
                }

                if( m_osESSearch.size() )
                {
                    json_object* poIndex = json_object_object_get(poHit, "_index");
                    if( poIndex == NULL || json_object_get_type(poIndex) != json_type_string )
                        break;
                    json_object* poType = json_object_object_get(poHit, "_type");
                    if( poType == NULL || json_object_get_type(poType) != json_type_string )
                        break;
                    CPLString osIndex(json_object_get_string(poIndex));
                    m_osMappingName = json_object_get_string(poType);

                    if( oVisited.find( std::pair<CPLString,CPLString>(osIndex, m_osMappingName) ) == oVisited.end() )
                    {
                        oVisited.insert( std::pair<CPLString,CPLString>(osIndex, m_osMappingName) );

                        json_object* poMappingRes = m_poDS->RunRequest(
                            (m_poDS->GetURL() + CPLString("/") + osIndex + CPLString("/_mapping/") + m_osMappingName + CPLString("?pretty")).c_str());
                        if( poMappingRes )
                        {
                            json_object* poLayerObj = json_object_object_get(poMappingRes, osIndex);
                            json_object* poMappings = NULL;
                            if( poLayerObj && json_object_get_type(poLayerObj) == json_type_object )
                                poMappings = json_object_object_get(poLayerObj, "mappings");
                            if( poMappings && json_object_get_type(poMappings) == json_type_object )
                            {
                                json_object* poMapping = json_object_object_get(poMappings, m_osMappingName);
                                if( poMapping)
                                {
                                    InitFeatureDefnFromMapping(poMapping, "", std::vector<CPLString>());
                                }
                            }
                            json_object_put(poMappingRes);
                        }
                    }
                }

                json_object_iter it;
                it.key = NULL;
                it.val = NULL;
                it.entry = NULL;
                json_object_object_foreachC( poSource, it )
                {
                    if( m_osFID.size() )
                    {
                        if( EQUAL(it.key, m_osFID) )
                            continue;
                    }
                    else if( EQUAL(it.key, m_poDS->GetFID()) )
                    {
                        m_osFID = it.key;
                        continue;
                    }

                    if( m_osMappingName == "FeatureCollection" )
                    {
                        if( strcmp(it.key, "properties") == 0 &&
                            json_object_get_type(it.val) == json_type_object )
                        {
                            json_object_iter it2;
                            it2.key = NULL;
                            it2.val = NULL;
                            it2.entry = NULL;
                            json_object_object_foreachC( it.val, it2 )
                            {
                                std::vector<CPLString> aosPath;
                                aosPath.push_back("properties");
                                AddOrUpdateField(it2.key, it2.key, it2.val, '.', aosPath);
                            }
                        }
                    }
                    else
                    {
                        std::vector<CPLString> aosPath;
                        AddOrUpdateField(it.key, it.key, it.val, '.', aosPath);
                    }
                }

                nAlreadyQueried ++;
                if( nFeatureCountToEstablishFeatureDefn > 0 &&
                    nAlreadyQueried >= nFeatureCountToEstablishFeatureDefn )
                {
                    break;
                }
            }

            json_object_put(poResponse);
            
            if( nFeatureCountToEstablishFeatureDefn > 0 &&
                nAlreadyQueried >= nFeatureCountToEstablishFeatureDefn )
            {
                break;
            }
        }
        
        ResetReading();
    }

    if( m_poDS->m_bJSonField )
    {
        AddFieldDefn("_json", OFTString, std::vector<CPLString>() );
    }
}

/************************************************************************/
/*                         BuildPathFromArray()                         */
/************************************************************************/

CPLString OGRElasticLayer::BuildPathFromArray(const std::vector<CPLString>& aosPath)
{
    CPLString osPath(aosPath[0]);
    for(size_t i=1;i<aosPath.size();i++)
    {
        osPath += ".";
        osPath += aosPath[i];
    }
    return osPath;
}

/************************************************************************/
/*                         GetOGRGeomTypeFromES()                       */
/************************************************************************/

static OGRwkbGeometryType GetOGRGeomTypeFromES(const char* pszType)
{
    if( EQUAL( pszType, "envelope") )
        return wkbPolygon;
    if( EQUAL( pszType, "circle") )
        return wkbPolygon;
    return OGRFromOGCGeomType(pszType);
}

/************************************************************************/
/*                         AddOrUpdateField()                           */
/************************************************************************/

void OGRElasticLayer::AddOrUpdateField(const char* pszAttrName,
                                       const char* pszKey,
                                       json_object* poObj,
                                       char chNestedAttributeSeparator,
                                       std::vector<CPLString>& aosPath)
{
    json_type eJSONType = json_object_get_type(poObj);
    if( eJSONType == json_type_null )
        return;

    if( eJSONType == json_type_object )
    {
        json_object* poType = json_object_object_get(poObj, "type");
        OGRwkbGeometryType eGeomType;
        if( poType && json_object_get_type(poType) == json_type_string &&
            (eGeomType = GetOGRGeomTypeFromES(json_object_get_string(poType))) != wkbUnknown &&
            json_object_object_get(poObj, (eGeomType == wkbGeometryCollection) ? "geometries" : "coordinates") )
        {
            int nIndex = m_poFeatureDefn->GetGeomFieldIndex(pszAttrName);
            if( nIndex < 0 )
            {
                aosPath.push_back(pszKey);
                AddGeomFieldDefn( pszAttrName, eGeomType, aosPath, FALSE );
            }
            else
            {
                OGRGeomFieldDefn* poFDefn = m_poFeatureDefn->GetGeomFieldDefn(nIndex);
                if( poFDefn->GetType() != eGeomType )
                    poFDefn->SetType(wkbUnknown);
            }
        }
        else if( m_poDS->m_bFlattenNestedAttributes )
        {
            if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
                return;
            aosPath.push_back(pszKey);
            
            json_object_iter it;
            it.key = NULL;
            it.val = NULL;
            it.entry = NULL;
            json_object_object_foreachC( poObj, it )
            {
                char szSeparator[2];
                szSeparator[0] = chNestedAttributeSeparator;
                szSeparator[1] = 0;
                CPLString osAttrName(CPLSPrintf("%s%s%s", pszAttrName, szSeparator,
                                                it.key));

                std::vector<CPLString> aosNewPaths(aosPath);
                AddOrUpdateField(osAttrName, it.key, it.val, chNestedAttributeSeparator,
                                 aosNewPaths);
            }
            return;
        }
    }
    /*else if( eJSONType == json_type_array )
    {
        if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
            return;
    }*/
    
    if( m_poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
        return;

    OGRFieldSubType eNewSubType;
    OGRFieldType eNewType = GeoJSONPropertyToFieldType( poObj, eNewSubType );

    int nIndex = m_poFeatureDefn->GetFieldIndex(pszAttrName);
    OGRFieldDefn* poFDefn = NULL;
    if( nIndex >= 0 )
        poFDefn = m_poFeatureDefn->GetFieldDefn(nIndex);
    if( (poFDefn == NULL && eNewType == OFTString) ||
        (poFDefn != NULL &&
         (poFDefn->GetType() == OFTDate || poFDefn->GetType() == OFTDateTime || poFDefn->GetType() == OFTTime) ) )
    {
        int nYear, nMonth, nDay, nHour, nMinute;
        float fSecond;
        if( sscanf(json_object_get_string(poObj),
                   "%04d/%02d/%02d %02d:%02d",
                   &nYear, &nMonth, &nDay, &nHour, &nMinute) == 5 ||
            sscanf(json_object_get_string(poObj),
                   "%04d-%02d-%02dT%02d:%02d",
                   &nYear, &nMonth, &nDay, &nHour, &nMinute) == 5 )
        {
            eNewType = OFTDateTime;
        }
        else if( sscanf(json_object_get_string(poObj),
                    "%04d/%02d/%02d",
                    &nYear, &nMonth, &nDay) == 3 ||
                 sscanf(json_object_get_string(poObj),
                    "%04d-%02d-%02d",
                    &nYear, &nMonth, &nDay) == 3)
        {
            eNewType = OFTDate;
        }
        else if( sscanf(json_object_get_string(poObj),
                    "%02d:%02d:%f",
                    &nHour, &nMinute, &fSecond) == 3 )
        {
            eNewType = OFTTime;
        }
    }

    if( poFDefn == NULL )
    {
        aosPath.push_back(pszKey);
        AddFieldDefn( pszAttrName, eNewType, aosPath, eNewSubType );
    }
    else
    {
        OGRUpdateFieldType(poFDefn, eNewType, eNewSubType);
    }
}

/************************************************************************/
/*                              SyncToDisk()                            */
/************************************************************************/

OGRErr OGRElasticLayer::SyncToDisk()
{
    if( WriteMapIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    
    if( !PushIndex() )
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}


/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRElasticLayer::GetLayerDefn() {
    
    FinalizeFeatureDefn();
    
    return m_poFeatureDefn;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char* OGRElasticLayer::GetFIDColumn()
{
    GetLayerDefn();
    return m_osFID.c_str();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRElasticLayer::ResetReading()
{
    if( m_osScrollID.size() )
    {
        char** papszOptions = CSLAddNameValue(NULL, "CUSTOMREQUEST", "DELETE");
        CPLHTTPResult* psResult = CPLHTTPFetch((m_poDS->GetURL() + CPLString("/_search/scroll?scroll_id=") + m_osScrollID).c_str(), papszOptions);
        CSLDestroy(papszOptions);
        CPLHTTPDestroyResult(psResult);

        m_osScrollID = "";
    }
    for(int i=0;i<(int)m_apoCachedFeatures.size();i++)
        delete m_apoCachedFeatures[i];
    m_apoCachedFeatures.resize(0);
    m_iCurID = 0;
    m_iCurFeatureInPage = 0;
    m_bEOF = FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRElasticLayer::GetNextFeature()

{
    FinalizeFeatureDefn();

    while( true )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRElasticLayer::GetNextRawFeature()
{
    json_object* poResponse = NULL;

    if( m_bEOF )
        return NULL;

    if( m_iCurFeatureInPage < (int)m_apoCachedFeatures.size() )
    {
        OGRFeature* poRet = m_apoCachedFeatures[m_iCurFeatureInPage];
        m_apoCachedFeatures[m_iCurFeatureInPage] = NULL;
        m_iCurFeatureInPage ++;
        return poRet;
    }

    for(int i=0;i<(int)m_apoCachedFeatures.size();i++)
        delete m_apoCachedFeatures[i];
    m_apoCachedFeatures.resize(0);
    m_iCurFeatureInPage = 0;

    CPLString osRequest, osPostData;
    if( m_osScrollID.size() == 0 )
    {
        if( m_osESSearch.size() )
        {
           osRequest = CPLSPrintf("%s/_search?scroll=1m&size=%d",
                           m_poDS->GetURL(), m_poDS->m_nBatchSize);
            osPostData = m_osESSearch;
        }
        else if( m_poSpatialFilter && m_osJSONFilter.size() == 0 )
        {
            CPLString osFilter = CPLSPrintf("{ \"query\": { \"filtered\" : { \"query\" : { \"match_all\" : {} }, \"filter\": %s } } }",
                                            json_object_to_json_string( m_poSpatialFilter ));
            osRequest = CPLSPrintf("%s/%s/%s/_search?scroll=1m&size=%d",
                           m_poDS->GetURL(), m_osIndexName.c_str(),
                           m_osMappingName.c_str(), m_poDS->m_nBatchSize);
            osPostData = osFilter;
        }
        else
        {
            osRequest =
                CPLSPrintf("%s/%s/%s/_search?scroll=1m&size=%d",
                           m_poDS->GetURL(), m_osIndexName.c_str(),
                           m_osMappingName.c_str(), m_poDS->m_nBatchSize);
            osPostData = m_osJSONFilter;
        }
    }
    else
    {
        osRequest =
            CPLSPrintf("%s/_search/scroll?scroll=1m&size=%d&scroll_id=%s",
                       m_poDS->GetURL(), m_poDS->m_nBatchSize, m_osScrollID.c_str());
    }

    if( m_bAddPretty )
        osRequest += "&pretty";
    poResponse = m_poDS->RunRequest(osRequest, osPostData);
    if( poResponse == NULL )
    {
        m_bEOF = TRUE;
        return NULL;
    }
    json_object* poScrollID = json_object_object_get(poResponse, "_scroll_id");
    if( poScrollID )
    {
        const char* pszScrollID = json_object_get_string(poScrollID);
        if( pszScrollID )
            m_osScrollID = pszScrollID;
    }

    json_object* poHits = json_object_object_get(poResponse, "hits");
    if( poHits == NULL || json_object_get_type(poHits) != json_type_object )
    {
        m_bEOF = TRUE;
        json_object_put(poResponse);
        return NULL;
    }
    poHits = json_object_object_get(poHits, "hits");
    if( poHits == NULL || json_object_get_type(poHits) != json_type_array )
    {
        m_bEOF = TRUE;
        json_object_put(poResponse);
        return NULL;
    }
    int nHits = json_object_array_length(poHits);
    if( nHits == 0 )
    {
        m_osScrollID = "";
        m_bEOF = TRUE;
        json_object_put(poResponse);
        return NULL;
    }
    for(int i=0;i<nHits;i++)
    {
        json_object* poHit = json_object_array_get_idx(poHits, i);
        if( poHit == NULL || json_object_get_type(poHit) != json_type_object )
        {
            continue;
        }
        json_object* poSource = json_object_object_get(poHit, "_source");
        if( poSource == NULL || json_object_get_type(poSource) != json_type_object )
        {
            continue;
        }
        
        const char* pszId = NULL;
        json_object* poId = json_object_object_get(poHit, "_id");
        if( poId != NULL && json_object_get_type(poId) == json_type_string )
            pszId = json_object_get_string(poId);
        
        OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
        if( pszId )
            poFeature->SetField("_id", pszId);
        
        if( m_osESSearch.size() )
        {
            json_object* poIndex = json_object_object_get(poHit, "_index");
            if( poIndex != NULL && json_object_get_type(poIndex) == json_type_string )
                poFeature->SetField("_index", json_object_get_string(poIndex));

            json_object* poType = json_object_object_get(poHit, "_type");
            if( poType != NULL && json_object_get_type(poType) == json_type_string )
                poFeature->SetField("_type", json_object_get_string(poType));
        }
        
        if( m_poDS->m_bJSonField )
            poFeature->SetField("_json", json_object_to_json_string(poSource));
        
        BuildFeature(poFeature, poSource, CPLString());
        if( poFeature->GetFID() < 0 )
            poFeature->SetFID( ++m_iCurID );
        m_apoCachedFeatures.push_back(poFeature);
    }

    json_object_put(poResponse);
    if( m_apoCachedFeatures.size() )
    {
        OGRFeature* poRet = m_apoCachedFeatures[ 0 ];
        m_apoCachedFeatures[ 0 ] = NULL;
        m_iCurFeatureInPage ++;
        return poRet;
    }
    return NULL;
}

/************************************************************************/
/*                      decode_geohash_bbox()                           */
/************************************************************************/

/* Derived from routine from https://github.com/davetroy/geohash/blob/master/ext/geohash_native.c */
/* (c) 2008-2010 David Troy, davetroy@gmail.com, (The MIT License) */

static const char BASE32[] = "0123456789bcdefghjkmnpqrstuvwxyz";

static void decode_geohash_bbox(const char *geohash, double lat[2], double lon[2])
{
    int i, j, hashlen;
    char c, cd, mask, is_even=1;
    static const char bits[] = {16,8,4,2,1};
    lat[0] = -90.0; lat[1] = 90.0;
    lon[0] = -180.0; lon[1] = 180.0;
    hashlen = static_cast<int>(strlen(geohash));
    for (i=0; i<hashlen; i++) {
        c = static_cast<char>(tolower(geohash[i]));
        cd = static_cast<char>(strchr(BASE32, c)-BASE32);
        for (j=0; j<5; j++) {
            mask = bits[j];
            if (is_even) {
                lon[!(cd&mask)] = (lon[0] + lon[1])/2;
            } else {
                lat[!(cd&mask)] = (lat[0] + lat[1])/2;
            }
            is_even = !is_even;
        }
    }
}

/************************************************************************/
/*                            BuildFeature()                            */
/************************************************************************/

void OGRElasticLayer::BuildFeature(OGRFeature* poFeature, json_object* poSource,
                                   CPLString osPath)
{
    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    CPLString osCurPath;
    json_object_object_foreachC( poSource, it )
    {
        if( osPath.size() == 0 &&
            m_osFID.size() && EQUAL(m_osFID, it.key) )
        {
            json_type eJSONType = json_object_get_type(it.val);
            if( eJSONType == json_type_int )
            {
                poFeature->SetFID((GIntBig)json_object_get_int64(it.val));
                continue;
            }
        }
        
        if( osPath.size() )
            osCurPath = osPath + "." + it.key;
        else
            osCurPath = it.key;
        std::map<CPLString,int>::iterator oIter = m_aosMapToFieldIndex.find(osCurPath);
        if( oIter != m_aosMapToFieldIndex.end() )
        {
            switch( json_object_get_type(it.val) )
            {
                case json_type_boolean:
                    poFeature->SetField( oIter->second, json_object_get_boolean(it.val));
                    break;
                case json_type_int:
                    poFeature->SetField( oIter->second, (GIntBig)json_object_get_int64(it.val));
                    break;
                case json_type_double:
                    poFeature->SetField( oIter->second, json_object_get_double(it.val));
                    break;
                case json_type_array:
                {
                    if( m_poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTIntegerList )
                    {
                        std::vector<int> anValues;
                        int nLength = json_object_array_length(it.val);
                        for(int i=0;i<nLength;i++)
                        {
                            anValues.push_back( json_object_get_int( json_object_array_get_idx( it.val, i ) ) );
                        }
                        if( nLength )
                            poFeature->SetField( oIter->second, nLength, &anValues[0] );
                    }
                    else if( m_poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTInteger64List )
                    {
                        std::vector<GIntBig> anValues;
                        int nLength = json_object_array_length(it.val);
                        for(int i=0;i<nLength;i++)
                        {
                            anValues.push_back( json_object_get_int64( json_object_array_get_idx( it.val, i ) ) );
                        }
                        if( nLength )
                            poFeature->SetField( oIter->second, nLength, &anValues[0] );
                    }
                    else if( m_poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTRealList )
                    {
                        std::vector<double> adfValues;
                        int nLength = json_object_array_length(it.val);
                        for(int i=0;i<nLength;i++)
                        {
                            adfValues.push_back( json_object_get_double( json_object_array_get_idx( it.val, i ) ) );
                        }
                        if( nLength )
                            poFeature->SetField( oIter->second, nLength, &adfValues[0] );
                    }
                    else if( m_poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTStringList )
                    {
                        std::vector<char*> apszValues;
                        int nLength = json_object_array_length(it.val);
                        for(int i=0;i<nLength;i++)
                        {
                            apszValues.push_back( CPLStrdup(json_object_get_string( json_object_array_get_idx( it.val, i ) )) );
                        }
                        apszValues.push_back( NULL);
                        poFeature->SetField( oIter->second, &apszValues[0] );
                        for(int i=0;i<nLength;i++)
                        {
                            CPLFree(apszValues[i]);
                        }
                    }
                    break;
                }
                default:
                {
                    if( m_poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTBinary )
                    {
                        GByte* pabyBase64 = (GByte*) CPLStrdup( json_object_get_string(it.val) );
                        int nBytes = CPLBase64DecodeInPlace( pabyBase64 );
                        poFeature->SetField( oIter->second, nBytes, pabyBase64 );
                        CPLFree(pabyBase64);
                    }
                    else
                    {
                        poFeature->SetField( oIter->second, json_object_get_string(it.val));
                    }
                    break;
                }
            }
        }
        else if( ( oIter = m_aosMapToGeomFieldIndex.find(osCurPath) ) != m_aosMapToGeomFieldIndex.end() )
        {
            OGRGeometry* poGeom = NULL;
            if( m_abIsGeoPoint[oIter->second] )
            {
                json_type eJSONType = json_object_get_type(it.val);
                if( eJSONType == json_type_array &&
                    json_object_array_length(it.val) == 2 )
                {
                    json_object* poX = json_object_array_get_idx(it.val, 0);
                    json_object* poY = json_object_array_get_idx(it.val, 1);
                    if( poX != NULL && poY != NULL )
                    {
                        poGeom = new OGRPoint( json_object_get_double(poX),
                                               json_object_get_double(poY) );
                    }
                }
                else if( eJSONType == json_type_object )
                {
                    json_object* poX = json_object_object_get(it.val, "lon");
                    json_object* poY = json_object_object_get(it.val, "lat");
                    if( poX != NULL && poY != NULL )
                    {
                        poGeom = new OGRPoint( json_object_get_double(poX),
                                               json_object_get_double(poY) );
                    }
                }
                else if( eJSONType == json_type_string )
                {
                    const char* pszLatLon = json_object_get_string(it.val);
                    char** papszTokens = CSLTokenizeString2(pszLatLon, ",", 0);
                    if( CSLCount(papszTokens) == 2 )
                    {
                        poGeom = new OGRPoint( CPLAtof(papszTokens[1]),
                                               CPLAtof(papszTokens[0]) );
                    }
                    else
                    {
                        double lat[2], lon[2];
                        decode_geohash_bbox(pszLatLon, lat, lon);
                        poGeom = new OGRPoint( (lon[0] + lon[1]) / 2,
                                               (lat[0] + lat[1]) / 2 );
                    }

                    CSLDestroy(papszTokens);
                }
            }
            else if( json_object_get_type(it.val) == json_type_object )
            {
                json_object* poType = json_object_object_get(it.val, "type");
                json_object* poRadius = json_object_object_get(it.val, "radius");
                json_object* poCoordinates = json_object_object_get(it.val, "coordinates");
                if( poType && poRadius && poCoordinates &&
                    json_object_get_type(poType) == json_type_string &&
                    EQUAL( json_object_get_string(poType), "circle" ) &&
                    (json_object_get_type(poRadius) == json_type_string ||
                     json_object_get_type(poRadius) == json_type_double ||
                     json_object_get_type(poRadius) == json_type_int ) &&
                    json_object_get_type(poCoordinates) == json_type_array &&
                    json_object_array_length(poCoordinates) == 2 )
                {
                    const char* pszRadius = json_object_get_string(poRadius);
                    double dfX = json_object_get_double(json_object_array_get_idx(poCoordinates, 0));
                    double dfY = json_object_get_double(json_object_array_get_idx(poCoordinates, 1));
                    int nRadiusLength = (int)strlen(pszRadius);
                    double dfRadius = CPLAtof(pszRadius);
                    double dfUnit = 0.0;
                    if( nRadiusLength >= 1 && pszRadius[nRadiusLength-1] == 'm' )
                    {
                        if( nRadiusLength >= 2 && pszRadius[nRadiusLength-2] == 'k' )
                            dfUnit = 1000;
                        else if( nRadiusLength >= 2 &&
                                 pszRadius[nRadiusLength-2] >= '0' &&
                                 pszRadius[nRadiusLength-2] <= '9' )
                            dfUnit = 1;
                    }
                    else if ( nRadiusLength >= 1 &&
                                 pszRadius[nRadiusLength-1] >= '0' &&
                                 pszRadius[nRadiusLength-1] <= '9' )
                    {
                        dfUnit = 1;
                    }

                    if( dfRadius == 0 )
                        CPLError(CE_Warning, CPLE_AppDefined, "Unknown unit in %s", pszRadius);
                    else
                    {
                        dfRadius *= dfUnit;
                        OGRLinearRing* poRing = new OGRLinearRing();
                        for(double dfStep = 0; dfStep <= 360; dfStep += 4 )
                        {
                            double dfLat, dfLon;
                            OGRXPlane_ExtendPosition( dfY, dfX, dfRadius, dfStep, &dfLat, &dfLon);
                            poRing->addPoint(dfLon, dfLat);
                        }
                        OGRPolygon* poPoly = new OGRPolygon();
                        poPoly->addRingDirectly(poRing);
                        poGeom = poPoly;
                    }
                }
                else if( poType && poCoordinates &&
                    json_object_get_type(poType) == json_type_string &&
                    EQUAL( json_object_get_string(poType), "envelope" ) &&
                    json_object_get_type(poCoordinates) == json_type_array &&
                    json_object_array_length(poCoordinates) == 2 )
                {
                    json_object* poCorner1 = json_object_array_get_idx(poCoordinates, 0);
                    json_object* poCorner2 = json_object_array_get_idx(poCoordinates, 1);
                    if( poCorner1 && poCorner2 && 
                        json_object_get_type(poCorner1) == json_type_array &&
                        json_object_array_length(poCorner1) == 2 && 
                        json_object_get_type(poCorner2) == json_type_array &&
                        json_object_array_length(poCorner2) == 2 )
                    {
                        double dfX1 = json_object_get_double(json_object_array_get_idx(poCorner1, 0));
                        double dfY1 = json_object_get_double(json_object_array_get_idx(poCorner1, 1));
                        double dfX2 = json_object_get_double(json_object_array_get_idx(poCorner2, 0));
                        double dfY2 = json_object_get_double(json_object_array_get_idx(poCorner2, 1));
                        OGRLinearRing* poRing = new OGRLinearRing();
                        poRing->addPoint(dfX1, dfY1);
                        poRing->addPoint(dfX2, dfY1);
                        poRing->addPoint(dfX2, dfY2);
                        poRing->addPoint(dfX1, dfY2);
                        poRing->addPoint(dfX1, dfY1);
                        OGRPolygon* poPoly = new OGRPolygon();
                        poPoly->addRingDirectly(poRing);
                        poGeom = poPoly;
                    }
                }
                else
                {
                    poGeom = OGRGeoJSONReadGeometry( it.val );
                }
            }

            if( poGeom != NULL )
            {
                poGeom->assignSpatialReference( m_poFeatureDefn->GetGeomFieldDefn(oIter->second)->GetSpatialRef() );
                poFeature->SetGeomFieldDirectly( oIter->second, poGeom );
            }
        }
        else if( json_object_get_type(it.val) == json_type_object &&
                 (m_poDS->m_bFlattenNestedAttributes ||
                  (osPath.size() == 0 && m_osMappingName == "FeatureCollection" && strcmp(it.key, "properties") == 0)) )
        {
            BuildFeature(poFeature, it.val, osCurPath);
        }
        else if( json_object_get_type(it.val) == json_type_object && 
                 !m_poDS->m_bFlattenNestedAttributes )
        {
            if( ( oIter = m_aosMapToGeomFieldIndex.find(osCurPath + ".coordinates") ) != m_aosMapToGeomFieldIndex.end() )
            {
                BuildFeature(poFeature, it.val, osCurPath);
            }
        }
    }
}

/************************************************************************/
/*                            AppendGroup()                             */
/************************************************************************/

static
json_object *AppendGroup(json_object *parent, const CPLString &name) {
    json_object *obj = json_object_new_object();
    json_object *properties = json_object_new_object();
    json_object_object_add(parent, name, obj);
    json_object_object_add(obj, "properties", properties);
    return properties;
}

/************************************************************************/
/*                           AddPropertyMap()                           */
/************************************************************************/

static json_object* AddPropertyMap(const CPLString &type) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "type", json_object_new_string(type.c_str()));
    return obj;
}

/************************************************************************/
/*                      GetContainerForMapping()                        */
/************************************************************************/

static json_object* GetContainerForMapping( json_object* poContainer,
                                            const std::vector<CPLString>& aosPath,
                                            std::map< std::vector<CPLString>, json_object* >& oMap )
{
    std::vector<CPLString> aosSubPath;
    for(int j=0;j<(int)aosPath.size()-1;j++)
    {
        aosSubPath.push_back(aosPath[j]);
        std::map< std::vector<CPLString>, json_object* >::iterator oIter = oMap.find(aosSubPath);
        if( oIter == oMap.end() )
        {
            json_object* poNewContainer = json_object_new_object();
            json_object* poProperties = json_object_new_object();
            json_object_object_add(poContainer, aosPath[j], poNewContainer);
            json_object_object_add(poNewContainer, "properties", poProperties);
            oMap[aosSubPath] = poProperties;
            poContainer = poProperties;
        }
        else
        {
            poContainer = oIter->second;
        }
    }
    return poContainer;
}


/************************************************************************/
/*                             BuildMap()                               */
/************************************************************************/

CPLString OGRElasticLayer::BuildMap() {
    json_object *map = json_object_new_object();

    std::map< std::vector<CPLString>, json_object* > oMap;

    json_object *poMapping = json_object_new_object();
    json_object *poMappingProperties = json_object_new_object();
    json_object_object_add(map, m_osMappingName, poMapping);
    json_object_object_add(poMapping, "properties", poMappingProperties);

    if( m_osMappingName == "FeatureCollection" )
    {
        json_object_object_add(poMappingProperties, "type", AddPropertyMap("string"));

        std::vector<CPLString> aosPath;
        aosPath.push_back("properties");
        aosPath.push_back("dummy");
        GetContainerForMapping(poMappingProperties, aosPath, oMap);
    }

    /* skip _id field */
    for(int i=1;i<m_poFeatureDefn->GetFieldCount();i++)
    {
        OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        
        json_object* poContainer = GetContainerForMapping(poMappingProperties,
                                                          m_aaosFieldPaths[i],
                                                          oMap);
        const char* pszLastComponent = m_aaosFieldPaths[i][(int)m_aaosFieldPaths[i].size()-1];

        const char* pszType = "string";
        const char* pszFormat = NULL;

        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            case OFTIntegerList:
            {
                if( poFieldDefn->GetSubType() == OFSTBoolean )
                    pszType = "boolean";
                else
                    pszType = "integer";
                break;
            }
            case OFTInteger64:
            case OFTInteger64List:
                pszType = "long";
                break;
            case OFTReal:
            case OFTRealList:
                pszType = "double";
                break;
            case OFTDateTime:
            case OFTDate:
                pszType = "date";
                pszFormat = "yyyy/MM/dd HH:mm:ss.SSSZZ||yyyy/MM/dd HH:mm:ss.SSS||yyyy/MM/dd";
                break;
            case OFTTime:
                pszType = "date";
                pszFormat = "HH:mm:ss.SSS";
                break;
            case OFTBinary:
                pszType = "binary";
                break;
            default:
                break;
        }

        json_object* poPropertyMap = json_object_new_object();
        json_object_object_add(poPropertyMap, "type", json_object_new_string(pszType));
        if( pszFormat )
            json_object_object_add(poPropertyMap, "format", json_object_new_string(pszFormat));
        if( m_bStoreFields || CSLFindString(m_papszStoredFields, poFieldDefn->GetNameRef()) >= 0 )
            json_object_object_add(poPropertyMap, "store", json_object_new_string("yes"));
        if( CSLFindString(m_papszNotAnalyzedFields, poFieldDefn->GetNameRef()) >= 0 )
            json_object_object_add(poPropertyMap, "index", json_object_new_string("not_analyzed"));
        else if( CSLFindString(m_papszNotIndexedFields, poFieldDefn->GetNameRef()) >= 0 )
            json_object_object_add(poPropertyMap, "index", json_object_new_string("no"));

        json_object_object_add(poContainer, pszLastComponent, poPropertyMap);

    }

    for(int i=0;i<m_poFeatureDefn->GetGeomFieldCount();i++)
    {
        std::vector<CPLString> aosPath = m_aaosGeomFieldPaths[i];
        int bAddGeoJSONType = FALSE;
        if( m_abIsGeoPoint[i] &&
            aosPath.size() >= 2 &&
            aosPath[(int)aosPath.size()-1] == "coordinates" )
        {
            bAddGeoJSONType = TRUE;
            aosPath.resize( (int)aosPath.size() - 1 );
        }

        json_object* poContainer = GetContainerForMapping(poMappingProperties,
                                                        aosPath,
                                                        oMap);
        const char* pszLastComponent = aosPath[(int)aosPath.size()-1];

        if( m_abIsGeoPoint[i] )
        {
            json_object* geo_point = AddPropertyMap("geo_point");
            if( bAddGeoJSONType )
            {
                json_object *geometry = AppendGroup(poContainer, pszLastComponent);
                json_object_object_add(geometry, "type", AddPropertyMap("string"));
                json_object_object_add(geometry, "coordinates", geo_point);
            }
            else
            {
                json_object_object_add(poContainer, pszLastComponent, geo_point);
            }
            if( m_osPrecision.size() )
            {
                json_object* field_data = json_object_new_object();
                json_object_object_add(geo_point, "fielddata", field_data);
                json_object_object_add(field_data, "format", json_object_new_string("compressed"));
                json_object_object_add(field_data, "precision", json_object_new_string(m_osPrecision.c_str()));
            }
        }
        else
        {
            json_object *geometry = json_object_new_object();
            json_object_object_add(poContainer,
                                pszLastComponent,
                                geometry);
            json_object_object_add(geometry, "type", json_object_new_string("geo_shape"));
            if( m_osPrecision.size() )
                json_object_object_add(geometry, "precision", json_object_new_string(m_osPrecision.c_str()));
        }
    }

    json_object* poMeta = NULL;
    json_object* poGeomFields = NULL;
    json_object* poFields = NULL;
    if( m_osFID.size() )
    {
        poMeta = json_object_new_object();
        json_object_object_add(poMeta, "fid",
                               json_object_new_string(m_osFID.c_str()));
    }
    for( int i=0; i < m_poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(i);
        if( !m_abIsGeoPoint[i] &&
            poGeomFieldDefn->GetType() != wkbUnknown )
        {
            if( poMeta == NULL )
                poMeta = json_object_new_object();
            if( poGeomFields == NULL )
            {
                poGeomFields = json_object_new_object();
                json_object_object_add(poMeta, "geomfields", poGeomFields);
            }
            json_object_object_add(poGeomFields,
                                   poGeomFieldDefn->GetNameRef(),
                                   json_object_new_string(OGRToOGCGeomType(poGeomFieldDefn->GetType())));
        }
    }
    for( int i=0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        OGRFieldType eType = poFieldDefn->GetType();
        if( eType == OFTIntegerList || eType == OFTInteger64List ||
            eType == OFTRealList || eType == OFTStringList )
        {
            if( poMeta == NULL )
                poMeta = json_object_new_object();
            if( poFields == NULL )
            {
                poFields = json_object_new_object();
                json_object_object_add(poMeta, "fields", poFields);
            }
            json_object_object_add(poFields,
                                   poFieldDefn->GetNameRef(),
                                   json_object_new_string(OGR_GetFieldTypeName(eType)));
        }
    }
    if( poMeta )
        json_object_object_add(poMapping, "_meta", poMeta );

    CPLString jsonMap(json_object_to_json_string(map));
    json_object_put(map);

    // Got personally caught by that...
    if( CSLCount(m_papszStoredFields) == 1 &&
        (EQUAL(m_papszStoredFields[0], "YES") || EQUAL(m_papszStoredFields[0], "TRUE")) &&
        m_poFeatureDefn->GetFieldIndex(m_papszStoredFields[0]) < 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "STORED_FIELDS=%s was specified. Perhaps you meant STORE_FIELDS=%s instead?",
                 m_papszStoredFields[0], m_papszStoredFields[0]);
    }

    return jsonMap;
}

/************************************************************************/
/*                       BuildGeoJSONGeometry()                         */
/************************************************************************/

static void BuildGeoJSONGeometry(json_object* geometry, OGRGeometry* poGeom)
{
    const int nPrecision = 10;
    double dfEps = pow(10.0, -(double)nPrecision);
    const char* pszGeomType = "";
    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
        case wkbPoint: pszGeomType = "point"; break;
        case wkbLineString: pszGeomType = "linestring"; break;
        case wkbPolygon: pszGeomType = "polygon"; break;
        case wkbMultiPoint: pszGeomType = "multipoint"; break;
        case wkbMultiLineString: pszGeomType = "multilinestring"; break;
        case wkbMultiPolygon: pszGeomType = "multipolygon"; break;
        case wkbGeometryCollection: pszGeomType = "geometrycollection"; break;
        default: break;
    }
    json_object_object_add(geometry, "type", json_object_new_string(pszGeomType));
    
    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
        case wkbPoint:
        {
            OGRPoint* poPoint = (OGRPoint*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            json_object_array_add(coordinates, json_object_new_double_with_precision(poPoint->getX(), nPrecision));
            json_object_array_add(coordinates, json_object_new_double_with_precision(poPoint->getY(), nPrecision));
            break;
        }
        
        case wkbLineString:
        {
            OGRLineString* poLS = (OGRLineString*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<poLS->getNumPoints();i++)
            {
                json_object *point = json_object_new_array();
                json_object_array_add(coordinates, point);
                json_object_array_add(point, json_object_new_double_with_precision(poLS->getX(i), nPrecision));
                json_object_array_add(point, json_object_new_double_with_precision(poLS->getY(i), nPrecision));
            }
            break;
        }

        case wkbPolygon:
        {
            OGRPolygon* poPoly = (OGRPolygon*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<1+poPoly->getNumInteriorRings();i++)
            {
                json_object *ring = json_object_new_array();
                json_object_array_add(coordinates, ring);
                OGRLineString* poLS = (i==0)?poPoly->getExteriorRing():poPoly->getInteriorRing(i-1);
                for(int j=0;j<poLS->getNumPoints();j++)
                {
                    if( j > 0 && fabs(poLS->getX(j) - poLS->getX(j-1)) < dfEps &&
                        fabs(poLS->getY(j) - poLS->getY(j-1)) < dfEps )
                        continue;
                    json_object *point = json_object_new_array();
                    json_object_array_add(ring, point);
                    json_object_array_add(point, json_object_new_double_with_precision(poLS->getX(j), nPrecision));
                    json_object_array_add(point, json_object_new_double_with_precision(poLS->getY(j), nPrecision));
                }
            }
            break;
        }
        
        case wkbMultiPoint:
        {
            OGRMultiPoint* poMP = (OGRMultiPoint*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<poMP->getNumGeometries();i++)
            {
                json_object *point = json_object_new_array();
                json_object_array_add(coordinates, point);
                OGRPoint* poPoint = (OGRPoint*) poMP->getGeometryRef(i);
                json_object_array_add(point, json_object_new_double_with_precision(poPoint->getX(), nPrecision));
                json_object_array_add(point, json_object_new_double_with_precision(poPoint->getY(), nPrecision));
            }
            break;
        }
        
        case wkbMultiLineString:
        {
            OGRMultiLineString* poMLS = (OGRMultiLineString*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<poMLS->getNumGeometries();i++)
            {
                json_object *ls = json_object_new_array();
                json_object_array_add(coordinates, ls);
                OGRLineString* poLS = (OGRLineString*) poMLS->getGeometryRef(i);
                for(int j=0;j<poLS->getNumPoints();j++)
                {
                    json_object *point = json_object_new_array();
                    json_object_array_add(ls, point);
                    json_object_array_add(point, json_object_new_double_with_precision(poLS->getX(j), nPrecision));
                    json_object_array_add(point, json_object_new_double_with_precision(poLS->getY(j), nPrecision));
                }
            }
            break;
        }
                
        case wkbMultiPolygon:
        {
            OGRMultiPolygon* poMP = (OGRMultiPolygon*)poGeom;
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for(int i=0;i<poMP->getNumGeometries();i++)
            {
                json_object *poly = json_object_new_array();
                json_object_array_add(coordinates, poly);
                OGRPolygon* poPoly = (OGRPolygon*) poMP->getGeometryRef(i);
                for(int j=0;j<1+poPoly->getNumInteriorRings();j++)
                {
                    json_object *ring = json_object_new_array();
                    json_object_array_add(poly, ring);
                    OGRLineString* poLS = (j==0)?poPoly->getExteriorRing():poPoly->getInteriorRing(j-1);
                    for(int k=0;k<poLS->getNumPoints();k++)
                    {
                        if( k > 0 && fabs(poLS->getX(k)- poLS->getX(k-1)) < dfEps &&
                            fabs(poLS->getY(k) - poLS->getY(k-1)) < dfEps )
                            continue;
                        json_object *point = json_object_new_array();
                        json_object_array_add(ring, point);
                        json_object_array_add(point, json_object_new_double_with_precision(poLS->getX(k), nPrecision));
                        json_object_array_add(point, json_object_new_double_with_precision(poLS->getY(k), nPrecision));
                    }
                }
            }
            break;
        }
                        
        case wkbGeometryCollection:
        {
            OGRGeometryCollection* poGC = (OGRGeometryCollection*)poGeom;
            json_object *geometries = json_object_new_array();
            json_object_object_add(geometry, "geometries", geometries);
            for(int i=0;i<poGC->getNumGeometries();i++)
            {
                json_object *subgeom = json_object_new_object();
                json_object_array_add(geometries, subgeom);
                BuildGeoJSONGeometry(subgeom, poGC->getGeometryRef(i));
            }
            break;
        }
        
        default:
            break;
    }
    
}

/************************************************************************/
/*                       WriteMapIfNecessary()                          */
/************************************************************************/

OGRErr OGRElasticLayer::WriteMapIfNecessary()
{
    if( m_bManualMapping )
        return OGRERR_NONE;
    
    // Check to see if the user has elected to only write out the mapping file
    // This method will only write out one layer from the vector file in cases where there are multiple layers
    if (m_osWriteMapFilename.size()) {
        if (m_bSerializeMapping) {
            m_bSerializeMapping = FALSE;
            CPLString map = BuildMap();

            // Write the map to a file
            VSILFILE *f = VSIFOpenL(m_osWriteMapFilename, "wb");
            if (f) {
                VSIFWriteL(map.c_str(), 1, map.length(), f);
                VSIFCloseL(f);
            }
        }
        return OGRERR_NONE;
    }

    // Check to see if we have any fields to upload to this index
    if (m_osWriteMapFilename.size() == 0 && m_bSerializeMapping ) {
        m_bSerializeMapping = FALSE;
        if( !m_poDS->UploadFile(CPLSPrintf("%s/%s/%s/_mapping", m_poDS->GetURL(), m_osIndexName.c_str(), m_osMappingName.c_str()), BuildMap()) )
        {
            return OGRERR_FAILURE;
        }
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                      GetContainerForFeature()                        */
/************************************************************************/

static json_object* GetContainerForFeature( json_object* poContainer,
                                            const std::vector<CPLString>& aosPath,
                                            std::map< std::vector<CPLString>, json_object* >& oMap )
{
    std::vector<CPLString> aosSubPath;
    for(int j=0;j<(int)aosPath.size()-1;j++)
    {
        aosSubPath.push_back(aosPath[j]);
         std::map< std::vector<CPLString>, json_object* >::iterator oIter = oMap.find(aosSubPath);
         if( oIter == oMap.end() )
         {
             json_object* poNewContainer = json_object_new_object();
             json_object_object_add(poContainer, aosPath[j], poNewContainer);
             oMap[aosSubPath] = poNewContainer;
             poContainer = poNewContainer;
         }
         else
         {
             poContainer = oIter->second;
         }
    }
    return poContainer;
}


/************************************************************************/
/*                        BuildJSonFromFeature()                        */
/************************************************************************/

CPLString OGRElasticLayer::BuildJSonFromFeature(OGRFeature *poFeature)
{
    
    CPLString fields;
    int nJSonFieldIndex = m_poFeatureDefn->GetFieldIndex("_json");
    if( nJSonFieldIndex >= 0 && poFeature->IsFieldSet(nJSonFieldIndex) )
    {
        fields = poFeature->GetFieldAsString(nJSonFieldIndex);
    }
    else
    {
        json_object *fieldObject = json_object_new_object();

        if( poFeature->GetFID() >= 0 && m_osFID.size() )
        {
            json_object_object_add(fieldObject,
                                   m_osFID.c_str(),
                                   json_object_new_int64(poFeature->GetFID()) );
        }

        std::map< std::vector<CPLString>, json_object* > oMap;

        for(int i=0;i<poFeature->GetGeomFieldCount();i++)
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom != NULL && !poGeom->IsEmpty() )
            {
                OGREnvelope env;
                poGeom->getEnvelope(&env);

                if( m_apoCT[i] != NULL )
                    poGeom->transform( m_apoCT[i] );
                else if( env.MinX < -180 || env.MinY < -90 ||
                         env.MaxX > 180 || env.MaxY > 90 )
                {
                    static int bHasWarned = FALSE;
                    if( !bHasWarned )
                    {
                        bHasWarned = TRUE;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "At least one geometry has a bounding box outside "
                                 "of [-180,180] longitude range and/or [-90,90] latitude range. Undefined behaviour");
                    }
                }

                std::vector<CPLString> aosPath = m_aaosGeomFieldPaths[i];
                int bAddGeoJSONType = FALSE;
                if( m_abIsGeoPoint[i] &&
                    aosPath.size() >= 2 &&
                    aosPath[(int)aosPath.size()-1] == "coordinates" )
                {
                    bAddGeoJSONType = TRUE;
                    aosPath.resize( (int)aosPath.size() - 1 );
                }

                json_object* poContainer = GetContainerForFeature(fieldObject, aosPath, oMap);
                const char* pszLastComponent = aosPath[(int)aosPath.size()-1];

                if( m_abIsGeoPoint[i] )
                {
                    json_object *coordinates = json_object_new_array();
                    const int nPrecision = 10;
                    json_object_array_add(coordinates, json_object_new_double_with_precision((env.MaxX + env.MinX)*0.5, nPrecision));
                    json_object_array_add(coordinates, json_object_new_double_with_precision((env.MaxY + env.MinY)*0.5, nPrecision));

                    if( bAddGeoJSONType )
                    {
                        json_object *geometry = json_object_new_object();
                        json_object_object_add(poContainer, pszLastComponent, geometry);
                        json_object_object_add(geometry, "type", json_object_new_string("POINT"));
                        json_object_object_add(geometry, "coordinates", coordinates);
                    }
                    else
                    {
                        json_object_object_add(poContainer, pszLastComponent, coordinates);
                    }
                }
                else
                {
                    json_object *geometry = json_object_new_object();
                    json_object_object_add(poContainer, pszLastComponent, geometry);
                    BuildGeoJSONGeometry(geometry, poGeom);
                }
            }
        }

        if( m_osMappingName == "FeatureCollection" )
        {
            if( poFeature->GetGeomFieldCount() == 1 &&
                poFeature->GetGeomFieldRef(0) )
            {
                json_object_object_add(fieldObject, "type", json_object_new_string("Feature"));
            }

            std::vector<CPLString> aosPath;
            aosPath.push_back("properties");
            aosPath.push_back("dummy");
            GetContainerForFeature(fieldObject, aosPath, oMap);
        }

        // For every field (except _id)
        int fieldCount = m_poFeatureDefn->GetFieldCount();
        for (int i = 1; i < fieldCount; i++)
        {
            if(!poFeature->IsFieldSet( i ) ) {
                    continue;
            }
            
            json_object* poContainer = GetContainerForFeature(fieldObject, m_aaosFieldPaths[i], oMap);
            const char* pszLastComponent = m_aaosFieldPaths[i][(int)m_aaosFieldPaths[i].size()-1];
            
            switch (m_poFeatureDefn->GetFieldDefn(i)->GetType()) {
                case OFTInteger:
                    if( m_poFeatureDefn->GetFieldDefn(i)->GetSubType() == OFSTBoolean )
                        json_object_object_add(poContainer,
                            pszLastComponent,
                            json_object_new_boolean(poFeature->GetFieldAsInteger(i)));
                    else
                        json_object_object_add(poContainer,
                            pszLastComponent,
                            json_object_new_int(poFeature->GetFieldAsInteger(i)));
                    break;
                case OFTInteger64:
                    json_object_object_add(poContainer,
                            pszLastComponent,
                            json_object_new_int64(poFeature->GetFieldAsInteger64(i)));
                    break;
                case OFTReal:
                    json_object_object_add(poContainer,
                            pszLastComponent,
                            json_object_new_double(poFeature->GetFieldAsDouble(i)));
                    break;
                case OFTIntegerList:
                {
                    int nCount;
                    const int* panValues = poFeature->GetFieldAsIntegerList(i, &nCount);
                    json_object* poArray = json_object_new_array();
                    for(int j=0;j<nCount;j++)
                        json_object_array_add(poArray, json_object_new_int(panValues[j]));
                    json_object_object_add(poContainer,
                            pszLastComponent, poArray);
                    break;
                }
                case OFTInteger64List:
                {
                    int nCount;
                    const GIntBig* panValues = poFeature->GetFieldAsInteger64List(i, &nCount);
                    json_object* poArray = json_object_new_array();
                    for(int j=0;j<nCount;j++)
                        json_object_array_add(poArray, json_object_new_int64(panValues[j]));
                    json_object_object_add(poContainer,
                            pszLastComponent, poArray);
                    break;
                }
                case OFTRealList:
                {
                    int nCount;
                    const double* padfValues = poFeature->GetFieldAsDoubleList(i, &nCount);
                    json_object* poArray = json_object_new_array();
                    for(int j=0;j<nCount;j++)
                        json_object_array_add(poArray, json_object_new_double(padfValues[j]));
                    json_object_object_add(poContainer,
                            pszLastComponent, poArray);
                    break;
                }
                case OFTStringList:
                {
                    char** papszValues = poFeature->GetFieldAsStringList(i);
                    json_object* poArray = json_object_new_array();
                    for(int j=0;papszValues[j]!= NULL;j++)
                        json_object_array_add(poArray, json_object_new_string(papszValues[j]));
                    json_object_object_add(poContainer,
                            pszLastComponent, poArray);
                    break;
                }
                case OFTBinary:
                {
                    int nCount;
                    GByte* pabyVal = poFeature->GetFieldAsBinary(i, &nCount);
                    char* pszVal = CPLBase64Encode(nCount, pabyVal);
                    json_object_object_add(poContainer,
                            pszLastComponent,
                            json_object_new_string(pszVal));
                    CPLFree(pszVal);
                    break;
                }
                case OFTDateTime:
                {
                    int nYear, nMonth, nDay, nHour, nMin, nTZ;
                    float fSec;
                    poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay,
                                                  &nHour, &nMin, &fSec, &nTZ);
                    if( nTZ == 0 )
                    {
                        json_object_object_add(poContainer,
                                pszLastComponent,
                                json_object_new_string(
                                    CPLSPrintf("%04d/%02d/%02d %02d:%02d:%06.3f",
                                            nYear, nMonth, nDay, nHour, nMin, fSec)));
                    }
                    else
                    {
                        int TZOffset = ABS(nTZ - 100) * 15;
                        int TZHour = TZOffset / 60;
                        int TZMinute = TZOffset - TZHour * 60;
                        json_object_object_add(poContainer,
                                pszLastComponent,
                                json_object_new_string(
                                    CPLSPrintf("%04d/%02d/%02d %02d:%02d:%06.3f%c%02d:%02d",
                                            nYear, nMonth, nDay, nHour, nMin, fSec,
                                            (nTZ >= 100) ? '+' : '-', TZHour, TZMinute)));
                    }
                    break;
                }
                default:
                {
                    const char* pszVal = poFeature->GetFieldAsString(i);
                    json_object_object_add(poContainer,
                            pszLastComponent,
                            json_object_new_string(pszVal));
                }
            }
        }

        // Build the field string
        fields = json_object_to_json_string(fieldObject);
        json_object_put(fieldObject);
    }
    
    return fields;
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

OGRErr OGRElasticLayer::ICreateFeature(OGRFeature *poFeature)
{
    FinalizeFeatureDefn();

    if( WriteMapIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    
    if (m_osWriteMapFilename.size())
        return OGRERR_NONE;

    if( poFeature->GetFID() < 0 )
    {
        if( m_nNextFID < 0 )
            m_nNextFID = GetFeatureCount(FALSE);
        poFeature->SetFID(++m_nNextFID);
    }

    CPLString osFields(BuildJSonFromFeature(poFeature));
    
    const char* pszId = NULL;
    if( poFeature->IsFieldSet(0) && !m_bIgnoreSourceID )
        pszId = poFeature->GetFieldAsString(0);

    // Check to see if we're using bulk uploading
    if (m_nBulkUpload > 0) {
        m_osBulkContent += CPLSPrintf("{\"index\" :{\"_index\":\"%s\", \"_type\":\"%s\"", m_osIndexName.c_str(), m_osMappingName.c_str());
        if( pszId )
            m_osBulkContent += CPLSPrintf(",\"_id\":\"%s\"", pszId);
        m_osBulkContent += "}}\n" + osFields + "\n\n";

        // Only push the data if we are over our bulk upload limit
        if ((int) m_osBulkContent.length() > m_nBulkUpload) {
            if( !PushIndex() )
            {
                return OGRERR_FAILURE;
            }
        }

    } else { // Fall back to using single item upload for every feature
        CPLString osURL(CPLSPrintf("%s/%s/%s/", m_poDS->GetURL(), m_osIndexName.c_str(), m_osMappingName.c_str()));
        if( pszId )
            osURL += pszId;
        json_object* poRes = m_poDS->RunRequest(osURL, osFields);
        if( poRes == NULL )
        {
            return OGRERR_FAILURE;
        }
        if( pszId == NULL )
        {
            json_object* poId = json_object_object_get(poRes, "_id");
            if( poId != NULL && json_object_get_type(poId) == json_type_string )
            {
                pszId = json_object_get_string(poId);
                poFeature->SetField(0, pszId);
            }
        
        }
        json_object_put(poRes);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ISetFeature()                              */
/************************************************************************/

OGRErr OGRElasticLayer::ISetFeature(OGRFeature *poFeature)
{
    FinalizeFeatureDefn();

    if( !poFeature->IsFieldSet(0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "_id field not set");
        return OGRERR_FAILURE;
    }
    if( poFeature->GetFID() < 0 && m_osFID.size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid FID");
        return OGRERR_FAILURE;
    }
    
    if( WriteMapIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    PushIndex();
    
    CPLString osFields(BuildJSonFromFeature(poFeature));
    
    // TODO? we should theoretically detect if the provided _id doesn't exist
    CPLString osURL(CPLSPrintf("%s/%s/%s/%s",
                               m_poDS->GetURL(), m_osIndexName.c_str(),
                               m_osMappingName.c_str(),poFeature->GetFieldAsString(0)));
    json_object* poRes = m_poDS->RunRequest(osURL, osFields);
    if( poRes == NULL )
    {
        return OGRERR_FAILURE;
    }
    //CPLDebug("ES", "SetFeature(): %s", json_object_to_json_string(poRes));
    json_object_put(poRes);
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                             PushIndex()                              */
/************************************************************************/

int OGRElasticLayer::PushIndex() {
    if (m_osBulkContent.empty()) {
        return TRUE;
    }

    int bRet = m_poDS->UploadFile(CPLSPrintf("%s/_bulk", m_poDS->GetURL()), m_osBulkContent);
    m_osBulkContent.clear();
    
    return bRet;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRElasticLayer::CreateField(OGRFieldDefn *poFieldDefn,
                                    CPL_UNUSED int bApproxOK)
{
    FinalizeFeatureDefn();
    ResetReading();

    if( m_poFeatureDefn->GetFieldIndex(poFieldDefn->GetNameRef()) >= 0 )
    {
        if( !EQUAL(poFieldDefn->GetNameRef(), "_id") &&
            !EQUAL(poFieldDefn->GetNameRef(), "_json") )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CreateField() called with an already existing field name: %s",
                      poFieldDefn->GetNameRef());
        }
        return OGRERR_FAILURE;
    }
    
    std::vector<CPLString> aosPath;
    if( m_osMappingName == "FeatureCollection" )
        aosPath.push_back("properties");
    
    if( m_bDotAsNestedField )
    {
        char** papszTokens = CSLTokenizeString2(poFieldDefn->GetNameRef(), ".", 0);
        for(int i=0; papszTokens[i]; i++ )
            aosPath.push_back(papszTokens[i]);
        CSLDestroy(papszTokens);
    }
    else
        aosPath.push_back(poFieldDefn->GetNameRef());
    
    AddFieldDefn( poFieldDefn->GetNameRef(),
                  poFieldDefn->GetType(),
                  aosPath,
                  poFieldDefn->GetSubType() );

    m_bSerializeMapping = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRElasticLayer::CreateGeomField( OGRGeomFieldDefn *poFieldIn, CPL_UNUSED int bApproxOK )

{
    FinalizeFeatureDefn();
    ResetReading();

    if( m_poFeatureDefn->GetGeomFieldIndex(poFieldIn->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateGeomField() called with an already existing field name: %s",
                  poFieldIn->GetNameRef());
        return OGRERR_FAILURE;
    }
    
    if( m_eGeomTypeMapping == ES_GEOMTYPE_GEO_POINT &&
        m_poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ES_GEOM_TYPE=GEO_POINT only supported for single geometry field");
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oFieldDefn(poFieldIn);
    if( EQUAL(oFieldDefn.GetNameRef(), "") )
        oFieldDefn.SetName("geometry");

    std::vector<CPLString> aosPath;
    if( m_bDotAsNestedField )
    {
        char** papszTokens = CSLTokenizeString2(oFieldDefn.GetNameRef(), ".", 0);
        for(int i=0; papszTokens[i]; i++ )
            aosPath.push_back(papszTokens[i]);
        CSLDestroy(papszTokens);
    }
    else
        aosPath.push_back(oFieldDefn.GetNameRef());

    if( m_eGeomTypeMapping == ES_GEOMTYPE_GEO_SHAPE ||
        (m_eGeomTypeMapping == ES_GEOMTYPE_AUTO &&
         poFieldIn->GetType() != wkbPoint) ||
        m_poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        m_abIsGeoPoint.push_back(FALSE);
    }
    else
    {
        m_abIsGeoPoint.push_back(TRUE);
        aosPath.push_back("coordinates");
    }
    
    m_aaosGeomFieldPaths.push_back(aosPath);
    
    m_aosMapToGeomFieldIndex[ BuildPathFromArray(aosPath) ] = m_poFeatureDefn->GetGeomFieldCount();

    m_poFeatureDefn->AddGeomFieldDefn( &oFieldDefn );

    OGRCoordinateTransformation* poCT = NULL;
    if( oFieldDefn.GetSpatialRef() != NULL )
    {
        OGRSpatialReference oSRS_WGS84;
        oSRS_WGS84.SetFromUserInput(SRS_WKT_WGS84);
        if( !oSRS_WGS84.IsSame(oFieldDefn.GetSpatialRef()) )
        {
            poCT = OGRCreateCoordinateTransformation( oFieldDefn.GetSpatialRef(), &oSRS_WGS84 );
            if( poCT == NULL )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "On-the-fly reprojection to WGS84 long/lat would be "
                          "needed, but instantiation of transformer failed");
            }
        }
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "No SRS given for geometry column %s. SRS is assumed to "
                  "be EPSG:4326 (WGS84 long/lat)",
                  oFieldDefn.GetNameRef());
    }

    m_apoCT.push_back(poCT);

    m_bSerializeMapping = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRElasticLayer::TestCapability(const char * pszCap) {
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poAttrQuery == NULL && m_poFilterGeom == NULL;

    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;

    else if (EQUAL(pszCap, OLCSequentialWrite) ||
             EQUAL(pszCap, OLCRandomWrite)
    )
        return TRUE;
    else if (EQUAL(pszCap, OLCCreateField) ||
             EQUAL(pszCap, OLCCreateGeomField) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRElasticLayer::GetFeatureCount(int bForce)
{
    if( m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount(bForce);

    json_object* poResponse;
    if( m_osESSearch.size() )
    {
        poResponse = m_poDS->RunRequest(
            CPLSPrintf("%s/_search?search_type=count&pretty", m_poDS->GetURL()),
            m_osESSearch.c_str());
    }
    else if( m_poSpatialFilter )
    {
        CPLString osFilter = CPLSPrintf("{ \"query\": { \"filtered\" : { \"query\" : { \"match_all\" : {} }, \"filter\": %s } } }",
                                        json_object_to_json_string( m_poSpatialFilter ));
        poResponse = m_poDS->RunRequest(
            CPLSPrintf("%s/%s/%s/_search?search_type=count&pretty", m_poDS->GetURL(), m_osIndexName.c_str(), m_osMappingName.c_str()),
            osFilter.c_str());
    }
    else
    {
        poResponse = m_poDS->RunRequest(
            CPLSPrintf("%s/%s/%s/_search?search_type=count&pretty", m_poDS->GetURL(), m_osIndexName.c_str(), m_osMappingName.c_str()),
            m_osJSONFilter.c_str());
    }

    json_object* poCount = json_ex_get_object_by_path(poResponse, "hits.count");
    if( poCount == NULL ) 
        poCount = json_ex_get_object_by_path(poResponse, "hits.total");
    if( poCount == NULL || json_object_get_type(poCount) != json_type_int )
    {
        json_object_put(poResponse);
        return OGRLayer::GetFeatureCount(bForce);
    }

    GIntBig nCount = json_object_get_int64(poCount);
    json_object_put(poResponse);
    return nCount;
}

/************************************************************************/
/*                          SetAttributeFilter()                        */
/************************************************************************/

OGRErr OGRElasticLayer::SetAttributeFilter(const char* pszFilter)
{
    if( pszFilter != NULL && pszFilter[0] == '{' )
    {
        if( m_osESSearch.size() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Setting an ElasticSearch filter on a resulting layer is not supported");
            return OGRERR_FAILURE;
        }
        OGRLayer::SetAttributeFilter(NULL);
        m_osJSONFilter = pszFilter;
        return OGRERR_NONE;
    }
    else
    {
        m_osJSONFilter = "";
        return OGRLayer::SetAttributeFilter(pszFilter);
    }
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRElasticLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    FinalizeFeatureDefn();

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return;
    }
    m_iGeomFieldFilter = iGeomField;
    
    InstallFilter( poGeomIn );
    
    json_object_put(m_poSpatialFilter);
    m_poSpatialFilter = NULL;

    if( poGeomIn == NULL )
        return;

    if( m_osESSearch.size() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Setting a spatial filter on a resulting layer is not supported");
        return;
    }

    OGREnvelope sEnvelope;
    poGeomIn->getEnvelope(&sEnvelope);

    if( sEnvelope.MinX < -180 )
        sEnvelope.MinX = -180;
    if( sEnvelope.MinX > 180 )
        sEnvelope.MinX = 180;

    if( sEnvelope.MinY < -90 )
        sEnvelope.MinY = -90;
    if( sEnvelope.MinY > 90 )
        sEnvelope.MinY = 90;

    if( sEnvelope.MaxX > 180 )
        sEnvelope.MaxX = 180;
    if( sEnvelope.MaxX < -180 )
        sEnvelope.MaxX = -180;

    if( sEnvelope.MaxY > 90 )
        sEnvelope.MaxY = 90;
    if( sEnvelope.MaxY < -90 )
        sEnvelope.MaxY = -90;

    if( sEnvelope.MinX == -180 && sEnvelope.MinY == -90 &&
        sEnvelope.MaxX == 180 && sEnvelope.MaxY == 90 )
    {
        return;
    }

    m_poSpatialFilter = json_object_new_object();
    
    if( m_abIsGeoPoint[iGeomField] )
    {
        json_object* geo_bounding_box = json_object_new_object();
        json_object_object_add(m_poSpatialFilter, "geo_bounding_box", geo_bounding_box);
        
        CPLString osPath = BuildPathFromArray(m_aaosGeomFieldPaths[iGeomField]);

        json_object* field = json_object_new_object();
        json_object_object_add(geo_bounding_box, osPath.c_str(), field);

        json_object* top_left = json_object_new_object();
        json_object_object_add(field, "top_left", top_left);
        json_object_object_add(top_left, "lat", json_object_new_double(sEnvelope.MaxY));
        json_object_object_add(top_left, "lon", json_object_new_double(sEnvelope.MinX));
        
        json_object* bottom_right = json_object_new_object();
        json_object_object_add(field, "bottom_right", bottom_right);
        json_object_object_add(bottom_right, "lat", json_object_new_double(sEnvelope.MinY));
        json_object_object_add(bottom_right, "lon", json_object_new_double(sEnvelope.MaxX));
    }
    else
    {
        json_object* geo_shape = json_object_new_object();
        json_object_object_add(m_poSpatialFilter, "geo_shape", geo_shape);
        
        CPLString osPath = BuildPathFromArray(m_aaosGeomFieldPaths[iGeomField]);
        
        json_object* field = json_object_new_object();
        json_object_object_add(geo_shape, osPath.c_str(), field);
        
        json_object* shape = json_object_new_object();
        json_object_object_add(field, "shape", shape);
        
        json_object_object_add(shape, "type", json_object_new_string("envelope"));
        
        json_object* coordinates = json_object_new_array();
        json_object_object_add(shape, "coordinates", coordinates);
        
        json_object* top_left = json_object_new_array();
        json_object_array_add(top_left, json_object_new_double(sEnvelope.MinX));
        json_object_array_add(top_left, json_object_new_double(sEnvelope.MaxY));
        json_object_array_add(coordinates, top_left);
        
        json_object* bottom_right = json_object_new_array();
        json_object_array_add(bottom_right, json_object_new_double(sEnvelope.MaxX));
        json_object_array_add(bottom_right, json_object_new_double(sEnvelope.MinY));
        json_object_array_add(coordinates, bottom_right);
    }
}

/************************************************************************/
/*                            GetExtent()                                */
/************************************************************************/

OGRErr OGRElasticLayer::GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce )
{
    FinalizeFeatureDefn();

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }
    
    if( !m_abIsGeoPoint[iGeomField] )
        return OGRLayer::GetExtentInternal(iGeomField, psExtent, bForce);

    json_object* poResponse;
    CPLString osFilter = CPLSPrintf("{ \"aggs\" : { \"bbox\" : { \"geo_bounds\" : { \"field\" : \"%s\" } } } }",
                                    BuildPathFromArray(m_aaosGeomFieldPaths[iGeomField]).c_str() );
    poResponse = m_poDS->RunRequest(
        CPLSPrintf("%s/%s/%s/_search?search_type=count&pretty",
                    m_poDS->GetURL(), m_osIndexName.c_str(), m_osMappingName.c_str()),
        osFilter.c_str());

    json_object* poBounds = json_ex_get_object_by_path(poResponse, "aggregations.bbox.bounds");
    json_object* poTopLeft = json_ex_get_object_by_path(poBounds, "top_left");
    json_object* poBottomRight = json_ex_get_object_by_path(poBounds, "bottom_right");
    json_object* poTopLeftLon = json_ex_get_object_by_path(poTopLeft, "lon");
    json_object* poTopLeftLat = json_ex_get_object_by_path(poTopLeft, "lat");
    json_object* poBottomRightLon = json_ex_get_object_by_path(poBottomRight, "lon");
    json_object* poBottomRightLat = json_ex_get_object_by_path(poBottomRight, "lat");
    
    OGRErr eErr;
    if( poTopLeftLon == NULL || poTopLeftLat == NULL ||
        poBottomRightLon == NULL || poBottomRightLat == NULL )
    {
        eErr = OGRLayer::GetExtentInternal(iGeomField, psExtent, bForce);
    }
    else
    {
        double dfMinX = json_object_get_double( poTopLeftLon );
        double dfMaxY = json_object_get_double( poTopLeftLat );
        double dfMaxX = json_object_get_double( poBottomRightLon );
        double dfMinY = json_object_get_double( poBottomRightLat );
        
        psExtent->MinX = dfMinX;
        psExtent->MaxY = dfMaxY;
        psExtent->MaxX = dfMaxX;
        psExtent->MinY = dfMinY;

        eErr = OGRERR_NONE;
    }
    json_object_put(poResponse);

    return eErr;
}
