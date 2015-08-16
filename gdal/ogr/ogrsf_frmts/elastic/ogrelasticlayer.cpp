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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRElasticLayer()                          */
/************************************************************************/

OGRElasticLayer::OGRElasticLayer(const char* pszLayerName,
                                 const char* pszIndexName,
                                 const char* pszMappingName,
                                 OGRElasticDataSource* poDS,
                                 char** papszOptions) {
    osIndexName = pszIndexName;
    osMappingName = pszMappingName;
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
    if( CSLFetchBoolean(papszOptions, "BULK_INSERT", FALSE) )
        nBulkUpload = 10000;
    osPrecision = CSLFetchNameValueDef(papszOptions, "GEOM_PRECISION", "");

    poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
    bMappingWritten = TRUE;

    iCurID = 0;
    iCurFeatureInPage = 0;
    bEOF = FALSE;
    m_poSpatialFilter = NULL;

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
/*                               BuildSchema()                          */
/************************************************************************/

void OGRElasticLayer::BuildSchema(json_object* poSchema)
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
        json_object* poType = json_object_object_get(it.val, "type");
        if( poType && json_object_get_type(poType) == json_type_string &&
            strcmp(json_object_get_string(poType), "geo_shape") == 0 )
        {
            OGRGeomFieldDefn oFieldDefn(it.key, wkbUnknown);
            OGRSpatialReference* poSRS_WGS84 = new OGRSpatialReference();
            poSRS_WGS84->SetFromUserInput(SRS_WKT_WGS84);
            oFieldDefn.SetSpatialRef(poSRS_WGS84);
            poSRS_WGS84->Dereference();

            std::vector<CPLString> aosPath;
            aosPath.push_back(oFieldDefn.GetNameRef());
            m_aaosGeomFieldPaths.push_back(aosPath);
            m_aosMapToGeomFieldIndex[ aosPath[0] ] = poFeatureDefn->GetGeomFieldCount();

            m_abIsGeoPoint.push_back(FALSE);

            poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);

            m_apoCT.push_back(NULL);

            continue;
        }

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
                    OGRGeomFieldDefn oFieldDefn(it.key, wkbPoint);
                    OGRSpatialReference* poSRS_WGS84 = new OGRSpatialReference();
                    poSRS_WGS84->SetFromUserInput(SRS_WKT_WGS84);
                    oFieldDefn.SetSpatialRef(poSRS_WGS84);
                    poSRS_WGS84->Dereference();

                    std::vector<CPLString> aosPath;
                    aosPath.push_back(oFieldDefn.GetNameRef());
                    m_aaosGeomFieldPaths.push_back(aosPath);
                    m_aosMapToGeomFieldIndex[ aosPath[0] ] = poFeatureDefn->GetGeomFieldCount();
                    
                    m_abIsGeoPoint.push_back(TRUE);

                    poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);

                    m_apoCT.push_back(NULL);

                    continue;
                }
            }

            if( osMappingName == "FeatureCollection" && strcmp(it.key, "properties") == 0 )
            {
                json_object_iter it2;
                it2.key = NULL;
                it2.val = NULL;
                it2.entry = NULL;
                json_object_object_foreachC( poProperties, it2 )
                {
                    if( json_object_get_type(it2.val) == json_type_object )
                    {
                        std::vector<CPLString> aosPath;
                        aosPath.push_back("properties");
                        CreateFieldFromSchema(it2.key, aosPath, it2.val);
                    }
                }
            }
        }

        if( osMappingName != "FeatureCollection" )
        {
            std::vector<CPLString> aosPath;
            CreateFieldFromSchema(it.key, aosPath, it.val);
        }
    }
}

/************************************************************************/
/*                        CreateFieldFromSchema()                       */
/************************************************************************/

void OGRElasticLayer::CreateFieldFromSchema(const char* pszName,
                                            std::vector<CPLString> aosPath,
                                            json_object* poObj)
{
    json_object* poType = json_object_object_get(poObj, "type");
    if( poType && json_object_get_type(poType) == json_type_string )
    {
        const char* pszType = json_object_get_string(poType);
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
        OGRFieldDefn oFieldDefn(pszName, eType);
        oFieldDefn.SetSubType(eSubType);

        aosPath.push_back( oFieldDefn.GetNameRef() );
        m_aaosFieldPaths.push_back(aosPath);
        
        CPLString osPath = aosPath[0];
        for(size_t i=1;i<aosPath.size();i++)
        {
            osPath += ".";
            osPath += aosPath[i];
        }
        
        m_aosMapToFieldIndex[ osPath ] = poFeatureDefn->GetFieldCount();

        poFeatureDefn->AddFieldDefn(&oFieldDefn);
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
        if( m_poSpatialFilter )
        {
            CPLString osFilter = CPLSPrintf("{ \"query\": { \"filtered\" : { \"query\" : { \"match_all\" : {} }, \"filter\": %s } } }",
                                            json_object_to_json_string( m_poSpatialFilter ));
            poResponse = poDS->RunRequest(
                CPLSPrintf("%s/%s/%s/_search?scroll=1m&size=100&pretty", poDS->GetURL(), osIndexName.c_str(), osMappingName.c_str()),
                osFilter.c_str());
        }
        else
        {
            poResponse = poDS->RunRequest(
                CPLSPrintf("%s/%s/%s/_search?scroll=1m&size=100&pretty", poDS->GetURL(), osIndexName.c_str(), osMappingName.c_str()));
        }
    }
    else
    {
        poResponse = poDS->RunRequest(
            CPLSPrintf("%s/_search/scroll?scroll=1m&size=100&scroll_id=%s&pretty", poDS->GetURL(), osScrollID.c_str()));
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
        OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
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
            if( json_object_get_type(it.val) == json_type_object )
            {
                OGRGeometry* poGeom = OGRGeoJSONReadGeometry( it.val );
                if( poGeom )
                {
                    poGeom->assignSpatialReference( poFeatureDefn->GetGeomFieldDefn(oIter->second)->GetSpatialRef() );
                    poFeature->SetGeomFieldDirectly( oIter->second, poGeom );
                }
            }
        }
        else if( json_object_get_type(it.val) == json_type_object )
        {
            BuildFeature(poFeature, it.val, osCurPath);
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
/*                             BuildMap()                               */
/************************************************************************/

CPLString OGRElasticLayer::BuildMap() {
    json_object *map = json_object_new_object();
    json_object *properties = NULL;

    json_object *Feature = AppendGroup(map, osMappingName);
    if( osMappingName == "FeatureCollection" )
    {
        json_object_object_add(Feature, "type", AddPropertyMap("string"));
        json_object* top_properties = json_object_new_object();
        json_object_object_add(Feature, "properties", top_properties);
        if( poFeatureDefn->GetFieldCount() )
        {
            properties = json_object_new_object();
            json_object_object_add(top_properties, "properties", properties);
        }
    }
    else
        properties = Feature;

    for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            case OFTIntegerList:
            {
                if( poFieldDefn->GetSubType() == OFSTBoolean )
                    json_object_object_add(properties, poFieldDefn->GetNameRef(), AddPropertyMap("boolean"));
                else
                    json_object_object_add(properties, poFieldDefn->GetNameRef(), AddPropertyMap("integer"));
                break;
            }
            case OFTInteger64:
            case OFTInteger64List:
                json_object_object_add(properties, poFieldDefn->GetNameRef(), AddPropertyMap("long"));
                break;
            case OFTReal:
            case OFTRealList:
                json_object_object_add(properties, poFieldDefn->GetNameRef(), AddPropertyMap("double"));
                break;
            case OFTString:
            case OFTStringList:
                json_object_object_add(properties, poFieldDefn->GetNameRef(), AddPropertyMap("string"));
                break;
            case OFTDateTime:
            case OFTDate:
                json_object_object_add(properties, poFieldDefn->GetNameRef(), AddPropertyMap("date", "yyyy/MM/dd HH:mm:ss.SSS||yyyy/MM/dd"));
                break;
            case OFTTime:
                json_object_object_add(properties, poFieldDefn->GetNameRef(), AddPropertyMap("date", "HH:mm:ss.SSS"));
                break;
            case OFTBinary:
                json_object_object_add(properties, poFieldDefn->GetNameRef(), AddPropertyMap("binary"));
                break;
            default:
                break;
        }
    }

    if( poFeatureDefn->GetGeomFieldCount() == 1 &&
        (eGeomTypeMapping == ES_GEOMTYPE_GEO_POINT ||
         (eGeomTypeMapping == ES_GEOMTYPE_AUTO && wkbFlatten(poFeatureDefn->GetGeomType()) == wkbPoint)) )
    {
        json_object *geometry = AppendGroup(Feature, "geometry");
        json_object_object_add(geometry, "type", AddPropertyMap("string"));
        json_object* geo_point = AddPropertyMap("geo_point");
        if( osPrecision.size() )
        {
            json_object* field_data = json_object_new_object();
            json_object_object_add(geo_point, "fielddata", field_data);
            json_object_object_add(field_data, "format", json_object_new_string("compressed"));
            json_object_object_add(field_data, "precision", json_object_new_string(osPrecision.c_str()));
        }
        json_object_object_add(geometry, "coordinates", geo_point);
    }
    else if( poFeatureDefn->GetGeomFieldCount() > 0 &&
        (eGeomTypeMapping == ES_GEOMTYPE_GEO_SHAPE ||
         poFeatureDefn->GetGeomFieldCount() > 1 ||
         (eGeomTypeMapping == ES_GEOMTYPE_AUTO && wkbFlatten(poFeatureDefn->GetGeomType()) != wkbPoint)) )
    {
        for(int i=0;i<poFeatureDefn->GetGeomFieldCount();i++)
        {
            m_abIsGeoPoint[i] = TRUE;

            json_object *geometry = json_object_new_object();
            json_object_object_add(Feature,
                                   poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef(),
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

    // Check to see if the user has elected to only write out the mapping file
    // This method will only write out one layer from the vector file in cases where there are multiple layers
    if (poDS->pszWriteMap != NULL) {
        if (!bMappingWritten) {
            bMappingWritten = TRUE;
            CPLString map = BuildMap();

            // Write the map to a file
            FILE *f = fopen(poDS->pszWriteMap, "wb");
            if (f) {
                fwrite(map.c_str(), 1, map.length(), f);
                fclose(f);
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
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRElasticLayer::ICreateFeature(OGRFeature *poFeature)
{
    if( WriteMapIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    json_object *fieldObject = json_object_new_object();

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( poGeom &&
        poFeatureDefn->GetGeomFieldCount() == 1 &&
        (eGeomTypeMapping == ES_GEOMTYPE_GEO_POINT ||
         (eGeomTypeMapping == ES_GEOMTYPE_AUTO && wkbFlatten(poFeatureDefn->GetGeomType()) == wkbPoint)) )
    {
        // Get the center point of the geometry
        OGREnvelope env;
        poGeom->getEnvelope(&env);

        json_object *geometry = json_object_new_object();
        json_object *coordinates = json_object_new_array();

        json_object_object_add(fieldObject, "geometry", geometry);
        json_object_object_add(geometry, "type", json_object_new_string("POINT"));
        json_object_object_add(geometry, "coordinates", coordinates);
        json_object_array_add(coordinates, json_object_new_double((env.MaxX + env.MinX)*0.5));
        json_object_array_add(coordinates, json_object_new_double((env.MaxY + env.MinY)*0.5));

        json_object_object_add(fieldObject, "type", json_object_new_string("Feature"));
    }
    else if( poFeatureDefn->GetGeomFieldCount() > 0 &&
            (eGeomTypeMapping == ES_GEOMTYPE_GEO_SHAPE ||
             poFeatureDefn->GetGeomFieldCount() > 1 ||
             (eGeomTypeMapping == ES_GEOMTYPE_AUTO && wkbFlatten(poFeatureDefn->GetGeomType()) != wkbPoint)) )
    {
        for(int i=0;i<poFeature->GetGeomFieldCount();i++)
        {
            poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                if( m_apoCT[i] != NULL )
                    poGeom->transform( m_apoCT[i] );

                json_object *geometry = json_object_new_object();
                json_object_object_add(fieldObject, poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef(), geometry);
                BuildGeoJSONGeometry(geometry, poGeom);
            }
        }
    }

    json_object *properties;
    if( osMappingName == "FeatureCollection" )
    {
        properties = json_object_new_object();
        json_object_object_add(fieldObject, "properties", properties);
    }
    else
        properties = fieldObject;

    // For every field that
    int fieldCount = poFeatureDefn->GetFieldCount();
    for (int i = 0; i < fieldCount; i++)
    {
        if(!poFeature->IsFieldSet( i ) ) {
                continue;
        }
        switch (poFeatureDefn->GetFieldDefn(i)->GetType()) {
            case OFTInteger:
                if( poFeatureDefn->GetFieldDefn(i)->GetSubType() == OFSTBoolean )
                    json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_boolean(poFeature->GetFieldAsInteger(i)));
                else
                    json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_int(poFeature->GetFieldAsInteger(i)));
                break;
            case OFTInteger64:
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_int64(poFeature->GetFieldAsInteger64(i)));
                break;
            case OFTReal:
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_double(poFeature->GetFieldAsDouble(i)));
                break;
            case OFTIntegerList:
            {
                int nCount;
                const int* panValues = poFeature->GetFieldAsIntegerList(i, &nCount);
                json_object* poArray = json_object_new_array();
                for(int j=0;j<nCount;j++)
                    json_object_array_add(poArray, json_object_new_int(panValues[j]));
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(), poArray);
                break;
            }
            case OFTInteger64List:
            {
                int nCount;
                const GIntBig* panValues = poFeature->GetFieldAsInteger64List(i, &nCount);
                json_object* poArray = json_object_new_array();
                for(int j=0;j<nCount;j++)
                    json_object_array_add(poArray, json_object_new_int64(panValues[j]));
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(), poArray);
                break;
            }
            case OFTRealList:
            {
                int nCount;
                const double* padfValues = poFeature->GetFieldAsDoubleList(i, &nCount);
                json_object* poArray = json_object_new_array();
                for(int j=0;j<nCount;j++)
                    json_object_array_add(poArray, json_object_new_double(padfValues[j]));
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(), poArray);
                break;
            }
            case OFTStringList:
            {
                char** papszValues = poFeature->GetFieldAsStringList(i);
                json_object* poArray = json_object_new_array();
                for(int j=0;papszValues[j]!= NULL;j++)
                    json_object_array_add(poArray, json_object_new_string(papszValues[j]));
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(), poArray);
                break;
            }
            case OFTBinary:
            {
                int nCount;
                GByte* pabyVal = poFeature->GetFieldAsBinary(i, &nCount);
                char* pszVal = CPLBase64Encode(nCount, pabyVal);
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_string(pszVal));
                CPLFree(pszVal);
                break;
            }
            default:
            {
                const char* pszVal = poFeature->GetFieldAsString(i);
                json_object_object_add(properties,
                        poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                        json_object_new_string(pszVal));
            }
        }
    }

    // Build the field string
    CPLString fields(json_object_to_json_string(fieldObject));
    json_object_put(fieldObject);

    // Check to see if we're using bulk uploading
    if (nBulkUpload > 0) {
        sIndex += CPLSPrintf("{\"index\" :{\"_index\":\"%s\", \"_type\":\"%s\"}}\n", osIndexName.c_str(), osMappingName.c_str()) +
                fields + "\n\n";

        // Only push the data if we are over our bulk upload limit
        if ((int) sIndex.length() > nBulkUpload) {
            if( !PushIndex() )
            {
                return OGRERR_FAILURE;
            }
        }

    } else { // Fall back to using single item upload for every feature
        if( !poDS->UploadFile(CPLSPrintf("%s/%s/%s/", poDS->GetURL(), osIndexName.c_str(), osMappingName.c_str()), fields) )
        {
            return OGRERR_FAILURE;
        }
    }

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
    if( poFeatureDefn->GetFieldIndex(poFieldDefn->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateField() called with an already existing field name: %s",
                  poFieldDefn->GetNameRef());
        return OGRERR_FAILURE;
    }
    
    std::vector<CPLString> aosPath;
    aosPath.push_back("properties");
    aosPath.push_back(poFieldDefn->GetNameRef());
    m_aaosFieldPaths.push_back(aosPath);
    m_aosMapToFieldIndex[ aosPath[0] + "." + aosPath[1] ] = poFeatureDefn->GetFieldCount();

    poFeatureDefn->AddFieldDefn(poFieldDefn);
    
    bMappingWritten = FALSE;
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRElasticLayer::CreateGeomField( OGRGeomFieldDefn *poFieldIn, CPL_UNUSED int bApproxOK )

{
    if( poFeatureDefn->GetGeomFieldIndex(poFieldIn->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateGeomField() called with an already existing field name: %s",
                  poFieldIn->GetNameRef());
        return OGRERR_FAILURE;
    }
    
    if( eGeomTypeMapping == ES_GEOMTYPE_GEO_POINT &&
        poFeatureDefn->GetGeomFieldCount() > 1 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ES_GEOM_TYPE=GEO_POINT only supported for single geometry field");
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oFieldDefn(poFieldIn);
    if( EQUAL(oFieldDefn.GetNameRef(), "") )
        oFieldDefn.SetName("geometry");

    std::vector<CPLString> aosPath;
    aosPath.push_back(oFieldDefn.GetNameRef());
    m_aaosGeomFieldPaths.push_back(aosPath);
    m_aosMapToGeomFieldIndex[ aosPath[0] ] = poFeatureDefn->GetGeomFieldCount();

    poFeatureDefn->AddGeomFieldDefn( &oFieldDefn );
    
    if( eGeomTypeMapping == ES_GEOMTYPE_GEO_SHAPE ||
        (eGeomTypeMapping == ES_GEOMTYPE_AUTO &&
         poFieldIn->GetType() != wkbPoint) )
    {
        m_abIsGeoPoint.push_back(TRUE);
    }
    else
    {
        m_abIsGeoPoint.push_back(FALSE);
    }

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

    else if (EQUAL(pszCap, OLCSequentialWrite))
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
    if( m_poSpatialFilter )
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
            CPLSPrintf("%s/%s/%s/_search?search_type=count&pretty", poDS->GetURL(), osIndexName.c_str(), osMappingName.c_str()));
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
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRElasticLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
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
        
        CPLString osPath = m_aaosGeomFieldPaths[iGeomField][0];
        for(size_t i=1;i<m_aaosGeomFieldPaths[iGeomField].size();i++)
        {
            osPath += ".";
            osPath += m_aaosGeomFieldPaths[iGeomField][i];
        }
        osPath += ".";
        osPath += "coordinates";

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
        
        CPLString osPath = m_aaosGeomFieldPaths[iGeomField][0];
        for(size_t i=1;i<m_aaosGeomFieldPaths[iGeomField].size();i++)
        {
            osPath += ".";
            osPath += m_aaosGeomFieldPaths[iGeomField][i];
        }
        
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
