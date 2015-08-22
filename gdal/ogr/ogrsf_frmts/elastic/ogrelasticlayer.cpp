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
                                 const char* pszESSearch) {
    osIndexName = pszIndexName ? pszIndexName : "";
    osMappingName = pszMappingName ? pszMappingName : "";
    osESSearch = pszESSearch ? pszESSearch : "";
    this->poDS = poDS;

    eGeomTypeMapping = ES_GEOMTYPE_AUTO;
    const char* pszESGeomType = CSLFetchNameValue(papszOptions, "GEOM_MAPPING_TYPE");
    if( pszESGeomType != NULL )
    {
        if( EQUAL(pszESGeomType, "GEO_POINT") )
            eGeomTypeMapping = ES_GEOMTYPE_GEO_POINT;
        else if( EQUAL(pszESGeomType, "GEO_SHAPE") )
            eGeomTypeMapping = ES_GEOMTYPE_GEO_SHAPE;
    }
    nBulkUpload = poDS->nBulkUpload;
    if( CSLFetchBoolean(papszOptions, "BULK_INSERT", TRUE) )
    {
        nBulkUpload = atoi(CSLFetchNameValueDef(papszOptions, "BULK_SIZE", "1000000"));
    }
    
    osPrecision = CSLFetchNameValueDef(papszOptions, "GEOM_PRECISION", "");

    poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
    
    AddFieldDefn("_id", OFTString, std::vector<CPLString>());
    
    if( osESSearch.size() )
    {
        AddFieldDefn("_index", OFTString, std::vector<CPLString>());
        AddFieldDefn("_type", OFTString, std::vector<CPLString>());
    }
    
    bFeatureDefnFinalized = FALSE;
    bMappingWritten = TRUE;
    bManualMapping = FALSE;
    bDotAsNestedField = TRUE;

    iCurID = 0;
    iCurFeatureInPage = 0;
    bEOF = FALSE;
    m_poSpatialFilter = NULL;
    bIgnoreSourceID = FALSE;

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

    poFeatureDefn->Release();
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
        m_aosMapToFieldIndex[ BuildPathFromArray(aosPath) ] = poFeatureDefn->GetFieldCount();
    poFeatureDefn->AddFieldDefn(&oFieldDefn);
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
    m_aosMapToGeomFieldIndex[ BuildPathFromArray(aosPath) ] = poFeatureDefn->GetGeomFieldCount();
    m_abIsGeoPoint.push_back(bIsGeoPoint);

    OGRSpatialReference* poSRS_WGS84 = new OGRSpatialReference();
    poSRS_WGS84->SetFromUserInput(SRS_WKT_WGS84);
    oFieldDefn.SetSpatialRef(poSRS_WGS84);
    poSRS_WGS84->Dereference();
    
    poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);

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
            json_object* poCoordinates = json_object_object_get(poProperties, "coordinates");
            if( poCoordinates && json_object_get_type(poCoordinates) == json_type_object )
            {
                json_object* poType = json_object_object_get(poCoordinates, "type");
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
                    
                    if( poFeatureDefn->GetGeomFieldIndex(osFieldName) < 0 )
                    {
                        std::vector<CPLString> aosNewPaths = aosPath;
                        aosNewPaths.push_back(osFieldName);
                        aosNewPaths.push_back("coordinates");

                        AddGeomFieldDefn(osFieldName, wkbPoint, aosNewPaths, TRUE);
                    }

                    continue;
                }
            }

            if( aosPath.size() == 0 && osMappingName == "FeatureCollection" && strcmp(it.key, "properties") == 0 )
            {
                std::vector<CPLString> aosNewPaths = aosPath;
                aosNewPaths.push_back(it.key);
                
                InitFeatureDefnFromMapping(it.val, pszPrefix, aosNewPaths);
                
                continue;
            }
            else if( poDS->bFlattenNestedAttributes )
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

        CreateFieldFromSchema(it.key, pszPrefix, aosPath, it.val);
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
        if( poFeatureDefn->GetGeomFieldIndex(osFieldName) >= 0 )
            return;

        aosPath.push_back(pszName);
        AddGeomFieldDefn(osFieldName,
                         EQUAL(pszType, "geo_point") ? wkbPoint : wkbUnknown,
                         aosPath, EQUAL(pszType, "geo_point"));
    }
    else if( !( aosPath.size() == 0 && osMappingName == "FeatureCollection" ) )
    {
        if( poFeatureDefn->GetFieldIndex(osFieldName) >= 0 )
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
                if( EQUAL(pszFormat, "HH:mm:ss.SSS") )
                    eType = OFTTime;
                else if( EQUAL(pszFormat, "yyyy/MM/dd") )
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
    if( bFeatureDefnFinalized )
        return;
    
    bFeatureDefnFinalized = TRUE;
    
    int nFeatureCountToEstablishFeatureDefn = poDS->nFeatureCountToEstablishFeatureDefn;
    if( osESSearch.size() && nFeatureCountToEstablishFeatureDefn <= 0 )
        nFeatureCountToEstablishFeatureDefn = 1;
    std::set< std::pair<CPLString, CPLString> > oVisited;

    if( bReadFeatures && nFeatureCountToEstablishFeatureDefn != 0 )
    {
        //CPLDebug("ES", "Try to get %d features to establish feature definition",
        //         FeatureCountToEstablishFeatureDefn);
        int bFirst = TRUE;
        int nAlreadyQueried = 0;
        while( TRUE )
        {
            json_object* poResponse;
            if( bFirst )
            {
                bFirst = FALSE;
                if(  osESSearch.size() )
                    poResponse = poDS->RunRequest(
                    CPLSPrintf("%s/_search?scroll=1m&size=%d&pretty",
                           poDS->GetURL(), poDS->nBatchSize),
                           osESSearch.c_str());
                else
                    poResponse = poDS->RunRequest(
                    CPLSPrintf("%s/%s/%s/_search?scroll=1m&size=%d&pretty",
                           poDS->GetURL(), osIndexName.c_str(),
                           osMappingName.c_str(), poDS->nBatchSize));
            }
            else
            {
                if( osScrollID.size() == 0 )
                    break;
                poResponse = poDS->RunRequest(
                    CPLSPrintf("%s/_search/scroll?scroll=1m&size=%d&scroll_id=%s&pretty",
                               poDS->GetURL(), poDS->nBatchSize, osScrollID.c_str()));
            }

            if( poResponse == NULL )
            {
                break;
            }
            json_object* poScrollID = json_object_object_get(poResponse, "_scroll_id");
            if( poScrollID )
            {
                const char* pszScrollID = json_object_get_string(poScrollID);
                if( pszScrollID )
                    osScrollID = pszScrollID;
            }

            json_object* poHits = json_object_object_get(poResponse, "hits");
            if( poHits == NULL || json_object_get_type(poHits) != json_type_object )
            {
                json_object_put(poResponse);
                break;
            }
            poHits = json_object_object_get(poHits, "hits");
            if( poHits == NULL || json_object_get_type(poHits) != json_type_array )
            {
                json_object_put(poResponse);
                break;
            }
            int nHits = json_object_array_length(poHits);
            if( nHits == 0 )
            {
                osScrollID = "";
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

                if( osESSearch.size() )
                {
                    json_object* poIndex = json_object_object_get(poHit, "_index");
                    if( poIndex == NULL || json_object_get_type(poIndex) != json_type_string )
                        break;
                    json_object* poType = json_object_object_get(poHit, "_type");
                    if( poType == NULL || json_object_get_type(poType) != json_type_string )
                        break;
                    CPLString osIndex(json_object_get_string(poIndex));
                    osMappingName = json_object_get_string(poType);

                    if( oVisited.find( std::pair<CPLString,CPLString>(osIndex, osMappingName) ) == oVisited.end() )
                    {
                        oVisited.insert( std::pair<CPLString,CPLString>(osIndex, osMappingName) );

                        json_object* poMappingRes = poDS->RunRequest(
                            (poDS->GetURL() + CPLString("/") + osIndex + CPLString("/_mapping/") + osMappingName + CPLString("?pretty")).c_str());
                        if( poMappingRes )
                        {
                            json_object* poLayerObj = json_object_object_get(poMappingRes, osIndex);
                            json_object* poMappings = NULL;
                            if( poLayerObj && json_object_get_type(poLayerObj) == json_type_object )
                                poMappings = json_object_object_get(poLayerObj, "mappings");
                            if( poMappings && json_object_get_type(poMappings) == json_type_object )
                            {
                                json_object* poMapping = json_object_object_get(poMappings, osMappingName);
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
                
                    if( osMappingName == "FeatureCollection" )
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

    if( poDS->bJSonField )
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
            int nIndex = poFeatureDefn->GetGeomFieldIndex(pszAttrName);
            if( nIndex < 0 )
            {
                aosPath.push_back(pszKey);
                AddGeomFieldDefn( pszAttrName, eGeomType, aosPath, FALSE );
            }
            else
            {
                OGRGeomFieldDefn* poFDefn = poFeatureDefn->GetGeomFieldDefn(nIndex);
                if( poFDefn->GetType() != eGeomType )
                    poFDefn->SetType(wkbUnknown);
            }
        }
        else if( poDS->bFlattenNestedAttributes )
        {
            if( poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
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
        if( poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
            return;
    }*/
    
    if( poFeatureDefn->GetGeomFieldIndex(pszAttrName) >= 0 )
        return;

    OGRFieldSubType eNewSubType;
    OGRFieldType eNewType = GeoJSONPropertyToFieldType( poObj, eNewSubType );
    if( eNewType == OFTString )
    {
        int nYear, nMonth, nDay, nHour, nMinute;
        float fSecond;
        if( sscanf(json_object_get_string(poObj),
                    "%04d/%02d/%02d %02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute) == 5 )
        {
            eNewType = OFTDateTime;
        }
        else if( sscanf(json_object_get_string(poObj),
                    "%04d/%02d/%02d",
                    &nYear, &nMonth, &nDay) == 3 )
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
    
    int nIndex = poFeatureDefn->GetFieldIndex(pszAttrName);
    if( nIndex < 0 )
    {
        aosPath.push_back(pszKey);
        AddFieldDefn( pszAttrName, eNewType, aosPath, eNewSubType );
    }
    else
    {
        OGRFieldDefn* poFDefn = poFeatureDefn->GetFieldDefn(nIndex);
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
    
    return poFeatureDefn;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRElasticLayer::ResetReading()
{
    if( osScrollID.size() )
    {
        char** papszOptions = CSLAddNameValue(NULL, "CUSTOMREQUEST", "DELETE");
        CPLHTTPResult* psResult = CPLHTTPFetch((poDS->GetURL() + CPLString("/_search/scroll?scroll_id=") + osScrollID).c_str(), papszOptions);
        CSLDestroy(papszOptions);
        CPLHTTPDestroyResult(psResult);

        osScrollID = "";
    }
    for(int i=0;i<(int)apoCachedFeatures.size();i++)
        delete apoCachedFeatures[i];
    apoCachedFeatures.resize(0);
    iCurID = 0;
    iCurFeatureInPage = 0;
    bEOF = FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRElasticLayer::GetNextFeature()

{
    FinalizeFeatureDefn();

    for( ; TRUE; )
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

    if( bEOF )
        return NULL;

    if( iCurFeatureInPage < (int)apoCachedFeatures.size() )
    {
        OGRFeature* poRet = apoCachedFeatures[iCurFeatureInPage];
        apoCachedFeatures[iCurFeatureInPage] = NULL;
        iCurFeatureInPage ++;
        return poRet;
    }

    for(int i=0;i<(int)apoCachedFeatures.size();i++)
        delete apoCachedFeatures[i];
    apoCachedFeatures.resize(0);
    iCurFeatureInPage = 0;

    if( osScrollID.size() == 0 )
    {
        if( osESSearch.size() )
        {
            poResponse = poDS->RunRequest(
                CPLSPrintf("%s/_search?scroll=1m&size=%d&pretty",
                           poDS->GetURL(), poDS->nBatchSize),
                osESSearch.c_str());
        }
        else if( m_poSpatialFilter && m_osJSONFilter.size() == 0 )
        {
            CPLString osFilter = CPLSPrintf("{ \"query\": { \"filtered\" : { \"query\" : { \"match_all\" : {} }, \"filter\": %s } } }",
                                            json_object_to_json_string( m_poSpatialFilter ));
            poResponse = poDS->RunRequest(
                CPLSPrintf("%s/%s/%s/_search?scroll=1m&size=%d&pretty",
                           poDS->GetURL(), osIndexName.c_str(),
                           osMappingName.c_str(), poDS->nBatchSize),
                osFilter.c_str());
        }
        else
        {
            poResponse = poDS->RunRequest(
                CPLSPrintf("%s/%s/%s/_search?scroll=1m&size=%d&pretty",
                           poDS->GetURL(), osIndexName.c_str(),
                           osMappingName.c_str(), poDS->nBatchSize),
                m_osJSONFilter.c_str());
        }
    }
    else
    {
        poResponse = poDS->RunRequest(
            CPLSPrintf("%s/_search/scroll?scroll=1m&size=%d&scroll_id=%s&pretty",
                       poDS->GetURL(), poDS->nBatchSize, osScrollID.c_str()));
    }

    if( poResponse == NULL )
    {
        bEOF = TRUE;
        return NULL;
    }
    json_object* poScrollID = json_object_object_get(poResponse, "_scroll_id");
    if( poScrollID )
    {
        const char* pszScrollID = json_object_get_string(poScrollID);
        if( pszScrollID )
            osScrollID = pszScrollID;
    }

    json_object* poHits = json_object_object_get(poResponse, "hits");
    if( poHits == NULL || json_object_get_type(poHits) != json_type_object )
    {
        bEOF = TRUE;
        json_object_put(poResponse);
        return NULL;
    }
    poHits = json_object_object_get(poHits, "hits");
    if( poHits == NULL || json_object_get_type(poHits) != json_type_array )
    {
        bEOF = TRUE;
        json_object_put(poResponse);
        return NULL;
    }
    int nHits = json_object_array_length(poHits);
    if( nHits == 0 )
    {
        osScrollID = "";
        bEOF = TRUE;
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
        
        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
        if( pszId )
            poFeature->SetField("_id", pszId);
        
        if( osESSearch.size() )
        {
            json_object* poIndex = json_object_object_get(poHit, "_index");
            if( poIndex != NULL && json_object_get_type(poIndex) == json_type_string )
                poFeature->SetField("_index", json_object_get_string(poIndex));

            json_object* poType = json_object_object_get(poHit, "_type");
            if( poType != NULL && json_object_get_type(poType) == json_type_string )
                poFeature->SetField("_type", json_object_get_string(poType));
        }
        
        if( poDS->bJSonField )
            poFeature->SetField("_json", json_object_to_json_string(poSource));
        
        BuildFeature(poFeature, poSource, CPLString());
        poFeature->SetFID( ++iCurID );
        apoCachedFeatures.push_back(poFeature);
    }

    json_object_put(poResponse);
    if( apoCachedFeatures.size() )
    {
        OGRFeature* poRet = apoCachedFeatures[ 0 ];
        apoCachedFeatures[ 0 ] = NULL;
        iCurFeatureInPage ++;
        return poRet;
    }
    return NULL;
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
                    if( poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTIntegerList )
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
                    else if( poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTInteger64List )
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
                    else if( poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTRealList )
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
                    else if( poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTStringList )
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
                    if( poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTBinary )
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
                    // TODO decode if stored as a geohash string ?

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
                poGeom->assignSpatialReference( poFeatureDefn->GetGeomFieldDefn(oIter->second)->GetSpatialRef() );
                poFeature->SetGeomFieldDirectly( oIter->second, poGeom );
            }
        }
        else if( json_object_get_type(it.val) == json_type_object &&
                 (poDS->bFlattenNestedAttributes ||
                  (osPath.size() == 0 && osMappingName == "FeatureCollection" && strcmp(it.key, "properties") == 0)) )
        {
            BuildFeature(poFeature, it.val, osCurPath);
        }
        else if( json_object_get_type(it.val) == json_type_object && 
                 !poDS->bFlattenNestedAttributes )
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

json_object *AddPropertyMap(const CPLString &type, const CPLString &format = "") {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "store", json_object_new_string("yes"));
    json_object_object_add(obj, "type", json_object_new_string(type.c_str()));
    if (!format.empty()) {
        json_object_object_add(obj, "format", json_object_new_string(format.c_str()));
    }
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

    json_object *Feature = AppendGroup(map, osMappingName);
    if( osMappingName == "FeatureCollection" )
    {
        json_object_object_add(Feature, "type", AddPropertyMap("string"));

        std::vector<CPLString> aosPath;
        aosPath.push_back("properties");
        aosPath.push_back("dummy");
        GetContainerForMapping(Feature, aosPath, oMap);
    }

    /* skip _id field */
    for(int i=1;i<poFeatureDefn->GetFieldCount();i++)
    {
        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        
        json_object* poContainer = GetContainerForMapping(Feature,
                                                          m_aaosFieldPaths[i],
                                                          oMap);
        const char* pszLastComponent = m_aaosFieldPaths[i][(int)m_aaosFieldPaths[i].size()-1];
        
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            case OFTIntegerList:
            {
                if( poFieldDefn->GetSubType() == OFSTBoolean )
                    json_object_object_add(poContainer, pszLastComponent, AddPropertyMap("boolean"));
                else
                    json_object_object_add(poContainer, pszLastComponent, AddPropertyMap("integer"));
                break;
            }
            case OFTInteger64:
            case OFTInteger64List:
                json_object_object_add(poContainer, pszLastComponent, AddPropertyMap("long"));
                break;
            case OFTReal:
            case OFTRealList:
                json_object_object_add(poContainer, pszLastComponent, AddPropertyMap("double"));
                break;
            case OFTString:
            case OFTStringList:
                json_object_object_add(poContainer, pszLastComponent, AddPropertyMap("string"));
                break;
            case OFTDateTime:
            case OFTDate:
                json_object_object_add(poContainer, pszLastComponent, AddPropertyMap("date", "yyyy/MM/dd HH:mm:ss.SSS||yyyy/MM/dd"));
                break;
            case OFTTime:
                json_object_object_add(poContainer, pszLastComponent, AddPropertyMap("date", "HH:mm:ss.SSS"));
                break;
            case OFTBinary:
                json_object_object_add(poContainer, pszLastComponent, AddPropertyMap("binary"));
                break;
            default:
                break;
        }
    }

    for(int i=0;i<poFeatureDefn->GetGeomFieldCount();i++)
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

        json_object* poContainer = GetContainerForMapping(Feature,
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
            if( osPrecision.size() )
            {
                json_object* field_data = json_object_new_object();
                json_object_object_add(geo_point, "fielddata", field_data);
                json_object_object_add(field_data, "format", json_object_new_string("compressed"));
                json_object_object_add(field_data, "precision", json_object_new_string(osPrecision.c_str()));
            }
        }
        else
        {
            json_object *geometry = json_object_new_object();
            json_object_object_add(poContainer,
                                pszLastComponent,
                                geometry);
            json_object_object_add(geometry, "type", json_object_new_string("geo_shape"));
            if( osPrecision.size() )
                json_object_object_add(geometry, "precision", json_object_new_string(osPrecision.c_str()));
        }
    }

    CPLString jsonMap(json_object_to_json_string(map));
    json_object_put(map);

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
    if( bManualMapping )
        return OGRERR_NONE;
    
    // Check to see if the user has elected to only write out the mapping file
    // This method will only write out one layer from the vector file in cases where there are multiple layers
    if (poDS->pszWriteMap != NULL) {
        if (!bMappingWritten) {
            bMappingWritten = TRUE;
            CPLString map = BuildMap();

            // Write the map to a file
            VSILFILE *f = VSIFOpenL(poDS->pszWriteMap, "wb");
            if (f) {
                VSIFWriteL(map.c_str(), 1, map.length(), f);
                VSIFCloseL(f);
            }
        }
        return OGRERR_NONE;
    }

    // Check to see if we have any fields to upload to this index
    if (poDS->pszMapping == NULL && !bMappingWritten ) {
        bMappingWritten = TRUE;
        if( !poDS->UploadFile(CPLSPrintf("%s/%s/%s/_mapping", poDS->GetURL(), osIndexName.c_str(), osMappingName.c_str()), BuildMap()) )
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
    int nJSonFieldIndex = poFeatureDefn->GetFieldIndex("_json");
    if( nJSonFieldIndex >= 0 && poFeature->IsFieldSet(nJSonFieldIndex) )
    {
        fields = poFeature->GetFieldAsString(nJSonFieldIndex);
    }
    else
    {
        json_object *fieldObject = json_object_new_object();

        std::map< std::vector<CPLString>, json_object* > oMap;

        for(int i=0;i<poFeature->GetGeomFieldCount();i++)
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                if( m_apoCT[i] != NULL )
                    poGeom->transform( m_apoCT[i] );

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
                    OGREnvelope env;
                    poGeom->getEnvelope(&env);

                    json_object *coordinates = json_object_new_array();
                    json_object_array_add(coordinates, json_object_new_double((env.MaxX + env.MinX)*0.5));
                    json_object_array_add(coordinates, json_object_new_double((env.MaxY + env.MinY)*0.5));

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

        if( osMappingName == "FeatureCollection" )
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
        int fieldCount = poFeatureDefn->GetFieldCount();
        for (int i = 1; i < fieldCount; i++)
        {
            if(!poFeature->IsFieldSet( i ) ) {
                    continue;
            }
            
            json_object* poContainer = GetContainerForFeature(fieldObject, m_aaosFieldPaths[i], oMap);
            const char* pszLastComponent = m_aaosFieldPaths[i][(int)m_aaosFieldPaths[i].size()-1];
            
            switch (poFeatureDefn->GetFieldDefn(i)->GetType()) {
                case OFTInteger:
                    if( poFeatureDefn->GetFieldDefn(i)->GetSubType() == OFSTBoolean )
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
    
    CPLString osFields(BuildJSonFromFeature(poFeature));
    
    const char* pszId = NULL;
    if( poFeature->IsFieldSet(0) && !bIgnoreSourceID )
        pszId = poFeature->GetFieldAsString(0);

    // Check to see if we're using bulk uploading
    if (nBulkUpload > 0) {
        sIndex += CPLSPrintf("{\"index\" :{\"_index\":\"%s\", \"_type\":\"%s\"", osIndexName.c_str(), osMappingName.c_str());
        if( pszId )
            sIndex += CPLSPrintf(",\"_id\":\"%s\"", pszId);
        sIndex += "}}\n" + osFields + "\n\n";

        // Only push the data if we are over our bulk upload limit
        if ((int) sIndex.length() > nBulkUpload) {
            if( !PushIndex() )
            {
                return OGRERR_FAILURE;
            }
        }

    } else { // Fall back to using single item upload for every feature
        CPLString osURL(CPLSPrintf("%s/%s/%s/", poDS->GetURL(), osIndexName.c_str(), osMappingName.c_str()));
        if( pszId )
            osURL += pszId;
        json_object* poRes = poDS->RunRequest(osURL, osFields);
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
    
    if( WriteMapIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    PushIndex();
    
    CPLString osFields(BuildJSonFromFeature(poFeature));
    
    // TODO? we should theoretically detect if the provided _id doesn't exist
    CPLString osURL(CPLSPrintf("%s/%s/%s/%s",
                               poDS->GetURL(), osIndexName.c_str(),
                               osMappingName.c_str(),poFeature->GetFieldAsString(0)));
    json_object* poRes = poDS->RunRequest(osURL, osFields);
    if( poRes == NULL )
    {
        return OGRERR_FAILURE;
    }
    json_object_put(poRes);
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                             PushIndex()                              */
/************************************************************************/

int OGRElasticLayer::PushIndex() {
    if (sIndex.empty()) {
        return TRUE;
    }

    int bRet = poDS->UploadFile(CPLSPrintf("%s/_bulk", poDS->GetURL()), sIndex);
    sIndex.clear();
    
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

    if( poFeatureDefn->GetFieldIndex(poFieldDefn->GetNameRef()) >= 0 )
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
    if( osMappingName == "FeatureCollection" )
        aosPath.push_back("properties");
    
    if( bDotAsNestedField )
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

    bMappingWritten = FALSE;
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRElasticLayer::CreateGeomField( OGRGeomFieldDefn *poFieldIn, CPL_UNUSED int bApproxOK )

{
    FinalizeFeatureDefn();
    ResetReading();

    if( poFeatureDefn->GetGeomFieldIndex(poFieldIn->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateGeomField() called with an already existing field name: %s",
                  poFieldIn->GetNameRef());
        return OGRERR_FAILURE;
    }
    
    if( eGeomTypeMapping == ES_GEOMTYPE_GEO_POINT &&
        poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ES_GEOM_TYPE=GEO_POINT only supported for single geometry field");
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oFieldDefn(poFieldIn);
    if( EQUAL(oFieldDefn.GetNameRef(), "") )
        oFieldDefn.SetName("geometry");

    std::vector<CPLString> aosPath;
    if( bDotAsNestedField )
    {
        char** papszTokens = CSLTokenizeString2(oFieldDefn.GetNameRef(), ".", 0);
        for(int i=0; papszTokens[i]; i++ )
            aosPath.push_back(papszTokens[i]);
        CSLDestroy(papszTokens);
    }
    else
        aosPath.push_back(oFieldDefn.GetNameRef());

    if( eGeomTypeMapping == ES_GEOMTYPE_GEO_SHAPE ||
        (eGeomTypeMapping == ES_GEOMTYPE_AUTO &&
         poFieldIn->GetType() != wkbPoint) ||
        poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        m_abIsGeoPoint.push_back(FALSE);
    }
    else
    {
        m_abIsGeoPoint.push_back(TRUE);
        aosPath.push_back("coordinates");
    }
    
    m_aaosGeomFieldPaths.push_back(aosPath);
    
    m_aosMapToGeomFieldIndex[ BuildPathFromArray(aosPath) ] = poFeatureDefn->GetGeomFieldCount();

    poFeatureDefn->AddGeomFieldDefn( &oFieldDefn );

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
                CPLError(CE_Warning, CPLE_AppDefined,
                         "On-the-fly reprojection to WGS84 longlat would be needed, but instanciation of transformer failed");
            }
        }
    }
    m_apoCT.push_back(poCT);
    
    bMappingWritten = FALSE;

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
    if( osESSearch.size() )
    {
        poResponse = poDS->RunRequest(
            CPLSPrintf("%s/_search?search_type=count&pretty", poDS->GetURL()),
            osESSearch.c_str());
    }
    else if( m_poSpatialFilter )
    {
        CPLString osFilter = CPLSPrintf("{ \"query\": { \"filtered\" : { \"query\" : { \"match_all\" : {} }, \"filter\": %s } } }",
                                        json_object_to_json_string( m_poSpatialFilter ));
        poResponse = poDS->RunRequest(
            CPLSPrintf("%s/%s/%s/_search?search_type=count&pretty", poDS->GetURL(), osIndexName.c_str(), osMappingName.c_str()),
            osFilter.c_str());
    }
    else
    {
        poResponse = poDS->RunRequest(
            CPLSPrintf("%s/%s/%s/_search?search_type=count&pretty", poDS->GetURL(), osIndexName.c_str(), osMappingName.c_str()),
            m_osJSONFilter.c_str());
    }

    if( poResponse == NULL )
        return OGRLayer::GetFeatureCount(bForce);
    //CPLDebug("ES", "Response: %s", json_object_to_json_string(poResponse));

    json_object* poHits = json_object_object_get(poResponse, "hits");
    if( poHits == NULL || json_object_get_type(poHits) != json_type_object )
    {
        json_object_put(poResponse);
        return OGRLayer::GetFeatureCount(bForce);
    }
    
    json_object* poCount = json_object_object_get(poHits, "count");
    if( poCount == NULL ) 
        poCount = json_object_object_get(poHits, "total");
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
        if( osESSearch.size() )
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

    if( osESSearch.size() )
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
