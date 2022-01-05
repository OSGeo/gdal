/******************************************************************************
 *
 * Project:  Elasticsearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 * Copyright (c) 2012-2016, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_swq.h"
#include "../geojson/ogrgeojsonwriter.h"
#include "../geojson/ogrgeojsonreader.h"
#include "../geojson/ogrgeojsonutils.h"
#include "ogr_geo_utils.h"

#include <cstdlib>
#include <set>


/************************************************************************/
/*                        CPLGettimeofday()                             */
/************************************************************************/

#if defined(_WIN32) && !defined(__CYGWIN__)
#  include <sys/timeb.h>

struct CPLTimeVal
{
  time_t  tv_sec;         /* seconds */
  long    tv_usec;        /* and microseconds */
};

static void CPLGettimeofday(struct CPLTimeVal* tp, void* /* timezonep*/ )
{
  struct _timeb theTime;

  _ftime(&theTime);
  tp->tv_sec = static_cast<time_t>(theTime.time);
  tp->tv_usec = theTime.millitm * 1000;
}
#else
#  include <sys/time.h>     /* for gettimeofday() */
#  define  CPLTimeVal timeval
#  define  CPLGettimeofday(t,u) gettimeofday(t,u)
#endif

static double GetTimestamp()
{
    struct CPLTimeVal tv;
    CPLGettimeofday(&tv, nullptr);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRElasticLayer()                          */
/************************************************************************/

OGRElasticLayer::OGRElasticLayer( const char* pszLayerName,
                                  const char* pszIndexName,
                                  const char* pszMappingName,
                                  OGRElasticDataSource* poDS,
                                  char** papszOptions,
                                  const char* pszESSearch ) :

    m_poDS(poDS),
    m_osIndexName(pszIndexName ? pszIndexName : ""),
    // Types are no longer supported in Elasticsearch 7+.
    m_osMappingName(poDS->m_nMajorVersion < 7
                    ? pszMappingName ? pszMappingName : ""
                    : ""),
    m_poFeatureDefn(new OGRFeatureDefn(pszLayerName)),
    m_osWriteMapFilename(
        CSLFetchNameValueDef(papszOptions, "WRITE_MAPPING",
                             poDS->m_pszWriteMap ? poDS->m_pszWriteMap : "")),
    m_bStoreFields(CPLFetchBool(papszOptions, "STORE_FIELDS", false)),
    m_osESSearch(pszESSearch ? pszESSearch : ""),
    m_nBulkUpload(poDS->m_nBulkUpload),
    m_osPrecision(CSLFetchNameValueDef(papszOptions, "GEOM_PRECISION", "")),
    // Undocumented. Only useful for developers.
    m_bAddPretty(CPLTestBool(CPLGetConfigOption("ES_ADD_PRETTY", "FALSE"))),
    m_bGeoShapeAsGeoJSON(EQUAL(CSLFetchNameValueDef(papszOptions, "GEO_SHAPE_ENCODING", "GeoJSON"), "GeoJSON"))
{
    const char* pszESGeomType = CSLFetchNameValue(papszOptions, "GEOM_MAPPING_TYPE");
    if( pszESGeomType != nullptr )
    {
        if( EQUAL(pszESGeomType, "GEO_POINT") )
            m_eGeomTypeMapping = ES_GEOMTYPE_GEO_POINT;
        else if( EQUAL(pszESGeomType, "GEO_SHAPE") )
            m_eGeomTypeMapping = ES_GEOMTYPE_GEO_SHAPE;
    }

    if( CPLFetchBool(papszOptions, "BULK_INSERT", true) )
    {
        m_nBulkUpload = atoi(CSLFetchNameValueDef(papszOptions, "BULK_SIZE", "1000000"));
    }

    const char* pszStoredFields = CSLFetchNameValue(papszOptions, "STORED_FIELDS");
    if( pszStoredFields )
        m_papszStoredFields = CSLTokenizeString2(pszStoredFields, ",", 0);

    const char* pszNotAnalyzedFields = CSLFetchNameValue(papszOptions, "NOT_ANALYZED_FIELDS");
    if( pszNotAnalyzedFields )
        m_papszNotAnalyzedFields = CSLTokenizeString2(pszNotAnalyzedFields, ",", 0);

    const char* pszNotIndexedFields = CSLFetchNameValue(papszOptions, "NOT_INDEXED_FIELDS");
    if( pszNotIndexedFields )
        m_papszNotIndexedFields = CSLTokenizeString2(pszNotIndexedFields, ",", 0);

    const char* pszFieldsWithRawValue = CSLFetchNameValue(papszOptions,
                                                    "FIELDS_WITH_RAW_VALUE");
    if( pszFieldsWithRawValue )
        m_papszFieldsWithRawValue = CSLTokenizeString2(
                                            pszFieldsWithRawValue, ",", 0);

    const char* pszSingleQueryTimeout = CSLFetchNameValue(papszOptions, "SINGLE_QUERY_TIMEOUT");
    if( pszSingleQueryTimeout )
    {
        m_dfSingleQueryTimeout = CPLAtof(pszSingleQueryTimeout);
        if( m_dfSingleQueryTimeout < 1 && m_dfSingleQueryTimeout >= 1e-3 )
            m_osSingleQueryTimeout = CPLSPrintf("%dms", static_cast<int>(m_dfSingleQueryTimeout * 1000));
        else if( m_dfSingleQueryTimeout >= 1 )
            m_osSingleQueryTimeout = CPLSPrintf("%ds", static_cast<int>(m_dfSingleQueryTimeout));
    }

    m_osSingleQueryTerminateAfter = CSLFetchNameValueDef(
        papszOptions, "SINGLE_QUERY_TERMINATE_AFTER", "");
    m_nSingleQueryTerminateAfter = CPLAtoGIntBig(m_osSingleQueryTerminateAfter);

    const char* pszFeatureIterationTimeout = CSLFetchNameValue(papszOptions, "FEATURE_ITERATION_TIMEOUT");
    if( pszFeatureIterationTimeout )
    {
        m_dfFeatureIterationTimeout = CPLAtof(pszFeatureIterationTimeout);
    }
    m_nFeatureIterationTerminateAfter = CPLAtoGIntBig(CSLFetchNameValueDef(
        papszOptions, "FEATURE_ITERATION_TERMINATE_AFTER", ""));

    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType(wkbNone);

    AddFieldDefn("_id", OFTString, std::vector<CPLString>());

    if( !m_osESSearch.empty() )
    {
        AddFieldDefn("_index", OFTString, std::vector<CPLString>());
        AddFieldDefn("_type", OFTString, std::vector<CPLString>());
    }

    OGRElasticLayer::ResetReading();
}

/************************************************************************/
/*                           OGRElasticLayer()                          */
/************************************************************************/

OGRElasticLayer::OGRElasticLayer( const char* pszLayerName,
                                  OGRElasticLayer* poReferenceLayer ):
    OGRElasticLayer(pszLayerName,
                    pszLayerName,
                    poReferenceLayer->m_osMappingName,
                    poReferenceLayer->m_poDS,
                    nullptr)
{
    m_bAddSourceIndexName = poReferenceLayer->m_poDS->m_bAddSourceIndexName;

    poReferenceLayer->CopyMembersTo(this);
    auto poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    if( m_bAddSourceIndexName )
    {
        OGRFieldDefn oFieldDefn("_index", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
        m_aaosFieldPaths.insert(m_aaosFieldPaths.begin(), std::vector<CPLString>());

        for( auto& kv: m_aosMapToFieldIndex )
        {
            kv.second ++;
        }
    }

    {
        const int nFieldCount = m_poFeatureDefn->GetFieldCount();
        for( int i = 0; i < nFieldCount; i++ )
            poFeatureDefn->AddFieldDefn( m_poFeatureDefn->GetFieldDefn( i ) );
    }

    {
        // Remove the default geometry field created instantiation.
        poFeatureDefn->DeleteGeomFieldDefn(0);
        const int nGeomFieldCount = m_poFeatureDefn->GetGeomFieldCount();
        for( int i = 0; i < nGeomFieldCount; i++ )
            poFeatureDefn->AddGeomFieldDefn( m_poFeatureDefn->GetGeomFieldDefn( i ) );
    }

    m_poFeatureDefn->Release();
    m_poFeatureDefn = poFeatureDefn;
    m_poFeatureDefn->Reference();

    CPLAssert(static_cast<int>(m_aaosFieldPaths.size()) == m_poFeatureDefn->GetFieldCount());
    CPLAssert(static_cast<int>(m_aaosGeomFieldPaths.size()) == m_poFeatureDefn->GetGeomFieldCount());
}

/************************************************************************/
/*                            CopyMembersTo()                           */
/************************************************************************/

void OGRElasticLayer::CopyMembersTo(OGRElasticLayer* poNew)
{
    FinalizeFeatureDefn();

    poNew->m_poFeatureDefn->Release();
    poNew->m_poFeatureDefn =
        const_cast<OGRElasticLayer*>(this)->GetLayerDefn()->Clone();
    poNew->m_poFeatureDefn->Reference();
    poNew->m_bFeatureDefnFinalized = true;
    poNew->m_osBulkContent = m_osBulkContent;
    poNew->m_nBulkUpload = m_nBulkUpload;
    poNew->m_osFID = m_osFID;
    poNew->m_aaosFieldPaths = m_aaosFieldPaths;
    poNew->m_aosMapToFieldIndex = m_aosMapToFieldIndex;
    poNew->m_aaosGeomFieldPaths = m_aaosGeomFieldPaths;
    poNew->m_aosMapToGeomFieldIndex = m_aosMapToGeomFieldIndex;
    poNew->m_abIsGeoPoint = m_abIsGeoPoint;
    poNew->m_eGeomTypeMapping = m_eGeomTypeMapping;
    poNew->m_osPrecision = m_osPrecision;
    poNew->m_papszNotAnalyzedFields = CSLDuplicate(m_papszNotAnalyzedFields);
    poNew->m_papszNotIndexedFields = CSLDuplicate(m_papszNotIndexedFields);
    poNew->m_papszFieldsWithRawValue = CSLDuplicate(m_papszFieldsWithRawValue);
    poNew->m_bGeoShapeAsGeoJSON = m_bGeoShapeAsGeoJSON;
    poNew->m_osSingleQueryTimeout = m_osSingleQueryTimeout;
    poNew->m_dfSingleQueryTimeout = m_dfSingleQueryTimeout;
    poNew->m_dfFeatureIterationTimeout = m_dfFeatureIterationTimeout;
    poNew->m_nSingleQueryTerminateAfter = m_nSingleQueryTerminateAfter;
    poNew->m_nFeatureIterationTerminateAfter = m_nFeatureIterationTerminateAfter;
    poNew->m_osSingleQueryTerminateAfter = m_osSingleQueryTerminateAfter;
}

/************************************************************************/
/*                              Clone()                                 */
/************************************************************************/

OGRElasticLayer* OGRElasticLayer::Clone()
{
    OGRElasticLayer* poNew = new OGRElasticLayer(m_poFeatureDefn->GetName(),
                                                 m_osIndexName,
                                                 m_osMappingName,
                                                 m_poDS,
                                                 nullptr);
    CopyMembersTo(poNew);
    return poNew;
}

/************************************************************************/
/*                         ~OGRElasticLayer()                           */
/************************************************************************/

OGRElasticLayer::~OGRElasticLayer() {
    OGRElasticLayer::SyncToDisk();

    OGRElasticLayer::ResetReading();

    json_object_put(m_poSpatialFilter);
    json_object_put(m_poJSONFilter);

    for(int i=0;i<(int)m_apoCT.size();i++)
        delete m_apoCT[i];

    m_poFeatureDefn->Release();

    CSLDestroy(m_papszStoredFields);
    CSLDestroy(m_papszNotAnalyzedFields);
    CSLDestroy(m_papszNotIndexedFields);
    CSLDestroy(m_papszFieldsWithRawValue);
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
    if( !aosPath.empty() )
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
    poSRS_WGS84->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
    poSRS_WGS84->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    oFieldDefn.SetSpatialRef(poSRS_WGS84);
    poSRS_WGS84->Dereference();

    m_poFeatureDefn->AddGeomFieldDefn(&oFieldDefn);

    m_apoCT.push_back(nullptr);
}

/************************************************************************/
/*                       GetGeomFieldProperties()                       */
/************************************************************************/

void OGRElasticLayer::GetGeomFieldProperties( int iGeomField,
                                              std::vector<CPLString>& aosPath,
                                              bool& bIsGeoPoint )
{
    aosPath = m_aaosGeomFieldPaths[iGeomField];
    bIsGeoPoint = CPL_TO_BOOL(m_abIsGeoPoint[iGeomField]);
}

/************************************************************************/
/*                     InitFeatureDefnFromMapping()                     */
/************************************************************************/

void OGRElasticLayer::InitFeatureDefnFromMapping(json_object* poSchema,
                                                 const char* pszPrefix,
                                                 const std::vector<CPLString>& aosPath)
{
    json_object* poTopProperties = CPL_json_object_object_get(poSchema, "properties");
    if( poTopProperties == nullptr || json_object_get_type(poTopProperties) != json_type_object )
        return;
    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    json_object_object_foreachC( poTopProperties, it )
    {
        json_object* poProperties = CPL_json_object_object_get(it.val, "properties");
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

            if( aosPath.empty() && m_osMappingName == "FeatureCollection" && strcmp(it.key, "properties") == 0 )
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

        if( aosPath.empty() && EQUAL(it.key, m_poDS->GetFID()) )
        {
            m_osFID = it.key;
        }
        else
        {
            CreateFieldFromSchema(it.key, pszPrefix, aosPath, it.val);
        }
    }

    if( aosPath.empty() )
    {
        json_object* poMeta = CPL_json_object_object_get(poSchema, "_meta");
        if( poMeta && json_object_get_type(poMeta) == json_type_object )
        {
            json_object* poFID = CPL_json_object_object_get(poMeta, "fid");
            if( poFID && json_object_get_type(poFID) == json_type_string )
                m_osFID = json_object_get_string(poFID);

            json_object* poGeomFields = CPL_json_object_object_get(poMeta, "geomfields");
            if( poGeomFields && json_object_get_type(poGeomFields) == json_type_object )
            {
                for( int i=0; i< m_poFeatureDefn->GetGeomFieldCount(); i++ )
                {
                    json_object* poObj = CPL_json_object_object_get(poGeomFields,
                            m_poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
                    if( poObj && json_object_get_type(poObj) == json_type_string )
                    {
                        OGRwkbGeometryType eType = OGRFromOGCGeomType(json_object_get_string(poObj));
                        if( eType != wkbUnknown )
                            m_poFeatureDefn->GetGeomFieldDefn(i)->SetType(eType);
                    }
                }
            }

            json_object* poFields = CPL_json_object_object_get(poMeta, "fields");
            if( poFields && json_object_get_type(poFields) == json_type_object )
            {
                for( int i=0; i< m_poFeatureDefn->GetFieldCount(); i++ )
                {
                    json_object* poObj = CPL_json_object_object_get(poFields,
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
    json_object* poType = CPL_json_object_object_get(poObj, "type");
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
    else if( !( aosPath.empty() && m_osMappingName == "FeatureCollection" ) )
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
            json_object* poFormat = CPL_json_object_object_get(poObj, "format");
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
        else if( EQUAL(pszType, "string") ) // ES < 5.0
        {
            json_object* poIndex =  CPL_json_object_object_get(poObj, "index");
            if( poIndex && json_object_get_type(poIndex) == json_type_string )
            {
                const char* pszIndex = json_object_get_string(poIndex);
                if( EQUAL(pszIndex, "not_analyzed") )
                {
                    m_papszNotAnalyzedFields = CSLAddString(
                        m_papszNotAnalyzedFields, osFieldName);
                }
            }
        }
        else if( EQUAL(pszType, "keyword") ) // ES >= 5.0
        {
            m_papszNotAnalyzedFields = CSLAddString(
                        m_papszNotAnalyzedFields, osFieldName);
        }

        aosPath.push_back( pszName );
        AddFieldDefn(osFieldName, eType, aosPath, eSubType);

        json_object* poFields = CPL_json_object_object_get(poObj, "fields");
        if( poFields && json_object_get_type(poFields) == json_type_object )
        {
            json_object* poRaw = CPL_json_object_object_get(poFields, "raw");
            if( poRaw && json_object_get_type(poRaw) == json_type_object )
            {
                json_object* poRawType = CPL_json_object_object_get(poRaw,
                                                                    "type");
                if( poRawType && json_object_get_type(poRawType) ==
                                                            json_type_string )
                {
                    const char* pszRawType = json_object_get_string(poRawType);
                    if( EQUAL(pszRawType, "keyword") ) // ES >= 5.0
                    {
                        m_papszFieldsWithRawValue = CSLAddString(
                            m_papszFieldsWithRawValue, osFieldName);
                    }
                    else if( EQUAL(pszRawType, "string") ) // ES < 5.0
                    {
                        json_object* poRawIndex = CPL_json_object_object_get(
                            poRaw, "index");
                        if( poRawIndex && json_object_get_type(poRawIndex) ==
                                                            json_type_string )
                        {
                            const char* pszRawIndex =
                                json_object_get_string(poRawIndex);
                            if( EQUAL(pszRawIndex, "not_analyzed") )
                            {
                                m_papszFieldsWithRawValue = CSLAddString(
                                    m_papszFieldsWithRawValue, osFieldName);
                            }
                        }
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                        FinalizeFeatureDefn()                         */
/************************************************************************/

void OGRElasticLayer::FinalizeFeatureDefn(bool bReadFeatures)
{
    if( m_bFeatureDefnFinalized )
        return;

    m_bFeatureDefnFinalized = true;

    int nFeatureCountToEstablishFeatureDefn = m_poDS->m_nFeatureCountToEstablishFeatureDefn;
    if( !m_osESSearch.empty() && nFeatureCountToEstablishFeatureDefn <= 0 )
        nFeatureCountToEstablishFeatureDefn = 1;
    std::set< std::pair<CPLString, CPLString> > oVisited;

    if( bReadFeatures && nFeatureCountToEstablishFeatureDefn != 0 )
    {
        //CPLDebug("ES", "Try to get %d features to establish feature definition",
        //         FeatureCountToEstablishFeatureDefn);
        bool bFirst = true;
        int nAlreadyQueried = 0;
        while( true )
        {
            CPLString osRequest;
            CPLString osPostData;
            if( bFirst )
            {
                bFirst = false;
                if(  !m_osESSearch.empty() )
                {
                    osRequest = CPLSPrintf("%s/_search?scroll=1m&size=%d",
                           m_poDS->GetURL(), m_poDS->m_nBatchSize);
                    osPostData = m_osESSearch;
                }
                else
                {
                    osRequest = BuildMappingURL(false);
                    osRequest += CPLSPrintf("/_search?scroll=1m&size=%d", m_poDS->m_nBatchSize);
                }
            }
            else
            {
                if( m_osScrollID.empty() )
                    break;
                osRequest = CPLSPrintf("%s/_search/scroll?scroll=1m&scroll_id=%s",
                               m_poDS->GetURL(), m_osScrollID.c_str());
            }

            if( m_bAddPretty )
                osRequest += "&pretty";
            json_object* poResponse = m_poDS->RunRequest(osRequest, osPostData);
            if( poResponse == nullptr )
            {
                break;
            }
            json_object* poScrollID = CPL_json_object_object_get(poResponse, "_scroll_id");
            if( poScrollID )
            {
                const char* pszScrollID = json_object_get_string(poScrollID);
                if( pszScrollID )
                    m_osScrollID = pszScrollID;
            }

            json_object* poHits = json_ex_get_object_by_path(poResponse, "hits.hits");
            if( poHits == nullptr || json_object_get_type(poHits) != json_type_array )
            {
                json_object_put(poResponse);
                break;
            }
            const auto nHits = json_object_array_length(poHits);
            if( nHits == 0 )
            {
                m_osScrollID = "";
                json_object_put(poResponse);
                break;
            }
            for(auto i=decltype(nHits){0};i<nHits;i++)
            {
                json_object* poHit = json_object_array_get_idx(poHits, i);
                if( poHit == nullptr || json_object_get_type(poHit) != json_type_object )
                {
                    continue;
                }
                json_object* poSource = CPL_json_object_object_get(poHit, "_source");
                if( poSource == nullptr || json_object_get_type(poSource) != json_type_object )
                {
                    continue;
                }

                if( !m_osESSearch.empty() )
                {
                    json_object* poIndex = CPL_json_object_object_get(poHit, "_index");
                    if( poIndex == nullptr || json_object_get_type(poIndex) != json_type_string )
                        break;
                    if (m_poDS->m_nMajorVersion < 7)
                    {
                        json_object* poType = CPL_json_object_object_get(poHit, "_type");
                        if( poType == nullptr || json_object_get_type(poType) != json_type_string )
                            break;
                        m_osMappingName = json_object_get_string(poType);
                    }
                    CPLString osIndex(json_object_get_string(poIndex));

                    if( oVisited.find( std::pair<CPLString,CPLString>(osIndex, m_osMappingName) ) == oVisited.end() )
                    {
                        oVisited.insert( std::pair<CPLString,CPLString>(osIndex, m_osMappingName) );

                        CPLString osURL = CPLSPrintf("%s/%s/_mapping", m_poDS->GetURL(), osIndex.c_str());
                        if (m_poDS->m_nMajorVersion < 7)
                            osURL += CPLSPrintf("/%s", m_osMappingName.c_str());
                        osURL += "?pretty";

                        json_object* poMappingRes = m_poDS->RunRequest(osURL);
                        if( poMappingRes )
                        {
                            json_object* poLayerObj = CPL_json_object_object_get(poMappingRes, osIndex);
                            json_object* poMappings = nullptr;
                            if( poLayerObj && json_object_get_type(poLayerObj) == json_type_object )
                                poMappings = CPL_json_object_object_get(poLayerObj, "mappings");
                            if( poMappings && json_object_get_type(poMappings) == json_type_object )
                            {
                                json_object* poMapping = m_poDS->m_nMajorVersion < 7
                                    ? CPL_json_object_object_get(poMappings, m_osMappingName)
                                    : poMappings;
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
                it.key = nullptr;
                it.val = nullptr;
                it.entry = nullptr;
                json_object_object_foreachC( poSource, it )
                {
                    if( !m_osFID.empty() )
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
                            it2.key = nullptr;
                            it2.val = nullptr;
                            it2.entry = nullptr;
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
        json_object* poType = CPL_json_object_object_get(poObj, "type");
        OGRwkbGeometryType eGeomType;
        if( poType && json_object_get_type(poType) == json_type_string &&
            (eGeomType = GetOGRGeomTypeFromES(json_object_get_string(poType))) != wkbUnknown &&
            CPL_json_object_object_get(poObj, (eGeomType == wkbGeometryCollection) ? "geometries" : "coordinates") )
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
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
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
    OGRFieldDefn* poFDefn = nullptr;
    if( nIndex >= 0 )
        poFDefn = m_poFeatureDefn->GetFieldDefn(nIndex);
    if( (poFDefn == nullptr && eNewType == OFTString) ||
        (poFDefn != nullptr &&
         (poFDefn->GetType() == OFTDate || poFDefn->GetType() == OFTDateTime || poFDefn->GetType() == OFTTime) ) )
    {
        int nYear = 0;
        int nMonth = 0;
        int nDay = 0;
        int nHour = 0;
        int nMinute = 0;
        float fSecond = 0.0f;
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

    if( poFDefn == nullptr )
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
    if( !m_osScrollID.empty() )
    {
        char** papszOptions = CSLAddNameValue(nullptr, "CUSTOMREQUEST", "DELETE");
        CPLHTTPResult* psResult = m_poDS->HTTPFetch((m_poDS->GetURL() + CPLString("/_search/scroll?scroll_id=") + m_osScrollID).c_str(), papszOptions);
        CSLDestroy(papszOptions);
        CPLHTTPDestroyResult(psResult);

        m_osScrollID = "";
    }
    for(int i=0;i<(int)m_apoCachedFeatures.size();i++)
        delete m_apoCachedFeatures[i];
    m_apoCachedFeatures.resize(0);
    m_iCurID = 0;
    m_iCurFeatureInPage = 0;
    m_bEOF = false;

    m_nReadFeaturesSinceResetReading = 0;
    m_dfEndTimeStamp = 0;
    const double dfTimeout =
        m_bUseSingleQueryParams ? m_dfSingleQueryTimeout : m_dfFeatureIterationTimeout;
    if( dfTimeout > 0 )
        m_dfEndTimeStamp = GetTimestamp() + dfTimeout;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRElasticLayer::GetNextFeature()

{
    FinalizeFeatureDefn();

    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == nullptr )
            return nullptr;

        if( (m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                           BuildSort()                                */
/************************************************************************/

json_object* OGRElasticLayer::BuildSort()
{
    json_object* poRet = json_object_new_array();
    for( size_t i=0; i<m_aoSortColumns.size(); ++i)
    {
        const int nIdx =
            m_poFeatureDefn->GetFieldIndex(m_aoSortColumns[i].osColumn);
        CPLString osFieldName( nIdx == 0 ? "_uid" :
                            BuildPathFromArray(m_aaosFieldPaths[ nIdx ]));
        if( CSLFindString(m_papszFieldsWithRawValue,
                          m_aoSortColumns[i].osColumn) >= 0 )
        {
            osFieldName += ".raw";
        }
        json_object* poSortCol = json_object_new_object();
        json_object* poSortProp = json_object_new_object();
        json_object_array_add(poRet, poSortCol);
        json_object_object_add(poSortProp, "order",
            json_object_new_string(m_aoSortColumns[i].bAsc ? "asc" : "desc"));
        json_object_object_add(poSortCol, osFieldName, poSortProp);
    }
    return poRet;
}

/************************************************************************/
/*                           BuildQuery()                               */
/************************************************************************/

CPLString OGRElasticLayer::BuildQuery(bool bCountOnly)
{
    CPLString osRet = "{ ";
    if( bCountOnly &&
        (m_poDS->m_nMajorVersion < 5 || !m_osSingleQueryTimeout.empty()) )
    {
        osRet += "\"size\": 0, ";
    }
    if( m_poSpatialFilter && m_poJSONFilter)
    {
        osRet += CPLSPrintf(
            "\"query\": { \"constant_score\" : { \"filter\": "
            "{ \"bool\" : { \"must\" : [%s, %s] } } } }",
            json_object_to_json_string( m_poSpatialFilter ),
            json_object_to_json_string( m_poJSONFilter ));
    }
    else
    {
        osRet += CPLSPrintf(
            "\"query\": { \"constant_score\" : { \"filter\": %s } }",
            json_object_to_json_string( m_poSpatialFilter ?
                            m_poSpatialFilter : m_poJSONFilter ));
    }
    if( !bCountOnly && !m_aoSortColumns.empty() )
    {
        json_object* poSort = BuildSort();
        osRet += CPLSPrintf(", \"sort\" : %s",
                            json_object_to_json_string(poSort));
        json_object_put(poSort);
    }
    osRet += " }";
    return osRet;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRElasticLayer::GetNextRawFeature()
{
    json_object* poResponse = nullptr;

    if( m_dfEndTimeStamp > 0 && GetTimestamp() >= m_dfEndTimeStamp )
    {
        CPLDebug("ES", "Terminating request due to timeout");
        return nullptr;
    }
    const auto nTerminateAfter =
        m_bUseSingleQueryParams ? m_nSingleQueryTerminateAfter : m_nFeatureIterationTerminateAfter;
    if( nTerminateAfter > 0 && m_nReadFeaturesSinceResetReading >= nTerminateAfter )
    {
        CPLDebug("ES", "Terminating request due to terminate_after reached");
        return nullptr;
    }

    if( m_bEOF )
        return nullptr;

    if( m_iCurFeatureInPage < (int)m_apoCachedFeatures.size() )
    {
        OGRFeature* poRet = m_apoCachedFeatures[m_iCurFeatureInPage];
        m_apoCachedFeatures[m_iCurFeatureInPage] = nullptr;
        m_iCurFeatureInPage ++;
        m_nReadFeaturesSinceResetReading ++;
        return poRet;
    }

    for(int i=0;i<(int)m_apoCachedFeatures.size();i++)
        delete m_apoCachedFeatures[i];
    m_apoCachedFeatures.resize(0);
    m_iCurFeatureInPage = 0;

    CPLString osRequest, osPostData;
    if( m_nReadFeaturesSinceResetReading == 0 )
    {
        if( !m_osESSearch.empty() )
        {
            osRequest = CPLSPrintf("%s/_search?scroll=1m&size=%d",
                           m_poDS->GetURL(), m_poDS->m_nBatchSize);
            osPostData = m_osESSearch;
        }
        else if( (m_poSpatialFilter && m_osJSONFilter.empty()) || m_poJSONFilter )
        {
            osPostData = BuildQuery(false);
            osRequest = BuildMappingURL(false);
            osRequest += CPLSPrintf("/_search?scroll=1m&size=%d", m_poDS->m_nBatchSize);
        }
        else if( !m_aoSortColumns.empty() && m_osJSONFilter.empty() )
        {
            osRequest = BuildMappingURL(false);
            osRequest += CPLSPrintf("/_search?scroll=1m&size=%d", m_poDS->m_nBatchSize);
            json_object* poSort = BuildSort();
            osPostData = CPLSPrintf(
                "{ \"sort\": %s }",
                json_object_to_json_string(poSort));
            json_object_put(poSort);
        }
        else
        {
            osRequest = BuildMappingURL(false);
            osRequest += CPLSPrintf("/_search?scroll=1m&size=%d", m_poDS->m_nBatchSize);
            osPostData = m_osJSONFilter;
        }
    }
    else
    {
        if( m_osScrollID.empty() )
        {
            m_bEOF = true;
            return nullptr;
        }
        osRequest =
            CPLSPrintf("%s/_search/scroll?scroll=1m&scroll_id=%s",
                       m_poDS->GetURL(), m_osScrollID.c_str());
    }

    if( m_bAddPretty )
        osRequest += "&pretty";
    poResponse = m_poDS->RunRequest(osRequest, osPostData);
    if( poResponse == nullptr )
    {
        m_bEOF = true;
        return nullptr;
    }
    m_osScrollID.clear();
    json_object* poScrollID = CPL_json_object_object_get(poResponse, "_scroll_id");
    if( poScrollID )
    {
        const char* pszScrollID = json_object_get_string(poScrollID);
        if( pszScrollID )
            m_osScrollID = pszScrollID;
    }

    json_object* poHits = CPL_json_object_object_get(poResponse, "hits");
    if( poHits == nullptr || json_object_get_type(poHits) != json_type_object )
    {
        m_bEOF = true;
        json_object_put(poResponse);
        return nullptr;
    }
    poHits = CPL_json_object_object_get(poHits, "hits");
    if( poHits == nullptr || json_object_get_type(poHits) != json_type_array )
    {
        m_bEOF = true;
        json_object_put(poResponse);
        return nullptr;
    }
    const auto nHits = json_object_array_length(poHits);
    if( nHits == 0 )
    {
        m_osScrollID = "";
        m_bEOF = true;
        json_object_put(poResponse);
        return nullptr;
    }
    for(auto i=decltype(nHits){0};i<nHits;i++)
    {
        json_object* poHit = json_object_array_get_idx(poHits, i);
        if( poHit == nullptr || json_object_get_type(poHit) != json_type_object )
        {
            continue;
        }
        json_object* poSource = CPL_json_object_object_get(poHit, "_source");
        if( poSource == nullptr || json_object_get_type(poSource) != json_type_object )
        {
            continue;
        }

        const char* pszId = nullptr;
        json_object* poId = CPL_json_object_object_get(poHit, "_id");
        if( poId != nullptr && json_object_get_type(poId) == json_type_string )
            pszId = json_object_get_string(poId);

        OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
        if( pszId )
            poFeature->SetField("_id", pszId);

        if( m_bAddSourceIndexName )
        {
            json_object* poIndex = CPL_json_object_object_get(poHit, "_index");
            if( poId != nullptr && json_object_get_type(poId) == json_type_string )
                poFeature->SetField("_index", json_object_get_string(poIndex));
        }

        if( !m_osESSearch.empty() )
        {
            json_object* poIndex = CPL_json_object_object_get(poHit, "_index");
            if( poIndex != nullptr && json_object_get_type(poIndex) == json_type_string )
                poFeature->SetField("_index", json_object_get_string(poIndex));

            json_object* poType = CPL_json_object_object_get(poHit, "_type");
            if( poType != nullptr && json_object_get_type(poType) == json_type_string )
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
    if( !m_apoCachedFeatures.empty() )
    {
        OGRFeature* poRet = m_apoCachedFeatures[ 0 ];
        m_apoCachedFeatures[ 0 ] = nullptr;
        m_iCurFeatureInPage ++;
        m_nReadFeaturesSinceResetReading ++;
        return poRet;
    }
    return nullptr;
}

/************************************************************************/
/*                      decode_geohash_bbox()                           */
/************************************************************************/

/* Derived from routine from https://github.com/davetroy/geohash/blob/master/ext/geohash_native.c */
/* (c) 2008-2010 David Troy, davetroy@gmail.com, (The MIT License) */

static const char BASE32[] = "0123456789bcdefghjkmnpqrstuvwxyz";

static void decode_geohash_bbox( const char *geohash, double lat[2],
                                 double lon[2] )
{
    int i;
    int j;
    int hashlen;
    char c;
    char cd;
    char mask;
    char is_even = 1;
    static const char bits[] = {16,8,4,2,1};
    lat[0] = -90.0;
    lat[1] = 90.0;
    lon[0] = -180.0;
    lon[1] = 180.0;
    hashlen = static_cast<int>(strlen(geohash));
    for( i = 0; i<hashlen; i++ )
    {
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
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    CPLString osCurPath;
    json_object_object_foreachC( poSource, it )
    {
        if( osPath.empty() &&
            !m_osFID.empty() && EQUAL(m_osFID, it.key) )
        {
            json_type eJSONType = json_object_get_type(it.val);
            if( eJSONType == json_type_int )
            {
                poFeature->SetFID((GIntBig)json_object_get_int64(it.val));
                continue;
            }
        }

        if( !osPath.empty() )
            osCurPath = osPath + "." + it.key;
        else
            osCurPath = it.key;
        std::map<CPLString,int>::iterator oIter = m_aosMapToFieldIndex.find(osCurPath);
        if( oIter != m_aosMapToFieldIndex.end() )
        {
            switch( json_object_get_type(it.val) )
            {
                case json_type_null:
                    poFeature->SetFieldNull( oIter->second );
                    break;
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
                        const auto nLength = json_object_array_length(it.val);
                        for(auto i=decltype(nLength){0};i<nLength;i++)
                        {
                            anValues.push_back( json_object_get_int( json_object_array_get_idx( it.val, i ) ) );
                        }
                        if( nLength )
                            poFeature->SetField( oIter->second,
                                                 static_cast<int>(nLength),
                                                 &anValues[0] );
                    }
                    else if( m_poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTInteger64List )
                    {
                        std::vector<GIntBig> anValues;
                        const auto nLength = json_object_array_length(it.val);
                        for(auto i=decltype(nLength){0};i<nLength;i++)
                        {
                            anValues.push_back( json_object_get_int64( json_object_array_get_idx( it.val, i ) ) );
                        }
                        if( nLength )
                            poFeature->SetField( oIter->second,
                                                 static_cast<int>(nLength),
                                                 &anValues[0] );
                    }
                    else if( m_poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTRealList )
                    {
                        std::vector<double> adfValues;
                        const auto nLength = json_object_array_length(it.val);
                        for(auto i=decltype(nLength){0};i<nLength;i++)
                        {
                            adfValues.push_back( json_object_get_double( json_object_array_get_idx( it.val, i ) ) );
                        }
                        if( nLength )
                            poFeature->SetField( oIter->second,
                                                 static_cast<int>(nLength),
                                                 &adfValues[0] );
                    }
                    else if( m_poFeatureDefn->GetFieldDefn(oIter->second)->GetType() == OFTStringList )
                    {
                        std::vector<char*> apszValues;
                        const auto nLength = json_object_array_length(it.val);
                        for(auto i=decltype(nLength){0};i<nLength;i++)
                        {
                            apszValues.push_back( CPLStrdup(json_object_get_string( json_object_array_get_idx( it.val, i ) )) );
                        }
                        apszValues.push_back( nullptr);
                        poFeature->SetField( oIter->second, &apszValues[0] );
                        for(auto i=decltype(nLength){0};i<nLength;i++)
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
            OGRGeometry* poGeom = nullptr;
            if( m_abIsGeoPoint[oIter->second] )
            {
                json_type eJSONType = json_object_get_type(it.val);
                if( eJSONType == json_type_array &&
                    json_object_array_length(it.val) == 2 )
                {
                    json_object* poX = json_object_array_get_idx(it.val, 0);
                    json_object* poY = json_object_array_get_idx(it.val, 1);
                    if( poX != nullptr && poY != nullptr )
                    {
                        poGeom = new OGRPoint( json_object_get_double(poX),
                                               json_object_get_double(poY) );
                    }
                }
                else if( eJSONType == json_type_object )
                {
                    json_object* poX = CPL_json_object_object_get(it.val, "lon");
                    json_object* poY = CPL_json_object_object_get(it.val, "lat");
                    if( poX != nullptr && poY != nullptr )
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
                        double lat[2] = { 0.0, 0.0 };
                        double lon[2] = { 0.0, 0.0 };
                        decode_geohash_bbox(pszLatLon, lat, lon);
                        poGeom = new OGRPoint( (lon[0] + lon[1]) / 2,
                                               (lat[0] + lat[1]) / 2 );
                    }

                    CSLDestroy(papszTokens);
                }
            }
            else if( json_object_get_type(it.val) == json_type_object )
            {
                json_object* poType = CPL_json_object_object_get(it.val, "type");
                json_object* poRadius = CPL_json_object_object_get(it.val, "radius");
                json_object* poCoordinates = CPL_json_object_object_get(it.val, "coordinates");
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
                    const double dfX = json_object_get_double(json_object_array_get_idx(poCoordinates, 0));
                    const double dfY = json_object_get_double(json_object_array_get_idx(poCoordinates, 1));
                    const int nRadiusLength = (int)strlen(pszRadius);
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
                            double dfLat = 0.0;
                            double dfLon = 0.0;
                            OGR_GreatCircle_ExtendPosition(dfY, dfX, dfRadius,
                                                      dfStep, &dfLat, &dfLon);
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
                        const double dfX1 = json_object_get_double(json_object_array_get_idx(poCorner1, 0));
                        const double dfY1 = json_object_get_double(json_object_array_get_idx(poCorner1, 1));
                        const double dfX2 = json_object_get_double(json_object_array_get_idx(poCorner2, 0));
                        const double dfY2 = json_object_get_double(json_object_array_get_idx(poCorner2, 1));
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
            else if( json_object_get_type(it.val) == json_type_string )
            {
                // Assume this is WKT
                OGRGeometryFactory::createFromWkt(
                    json_object_get_string(it.val), nullptr, &poGeom);
            }

            if( poGeom != nullptr )
            {
                poGeom->assignSpatialReference( m_poFeatureDefn->GetGeomFieldDefn(oIter->second)->GetSpatialRef() );
                poFeature->SetGeomFieldDirectly( oIter->second, poGeom );
            }
        }
        else if( json_object_get_type(it.val) == json_type_object &&
                 (m_poDS->m_bFlattenNestedAttributes ||
                  (osPath.empty() && m_osMappingName == "FeatureCollection" && strcmp(it.key, "properties") == 0)) )
        {
            BuildFeature(poFeature, it.val, osCurPath);
        }
        else if( json_object_get_type(it.val) == json_type_object &&
                 !m_poDS->m_bFlattenNestedAttributes )
        {
            if( m_aosMapToGeomFieldIndex.find(osCurPath + ".coordinates")
                                            != m_aosMapToGeomFieldIndex.end() )
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

    json_object *poMapping;
    json_object *poMappingProperties = json_object_new_object();
    if (m_poDS->m_nMajorVersion < 7)
    {
        poMapping = json_object_new_object();
        json_object_object_add(map, m_osMappingName, poMapping);
    }
    else
    {
        poMapping = map;
    }
    json_object_object_add(poMapping, "properties", poMappingProperties);

    if( m_poDS->m_nMajorVersion < 7 && m_osMappingName == "FeatureCollection" )
    {
        json_object_object_add(poMappingProperties, "type", AddPropertyMap(
            m_poDS->m_nMajorVersion >= 5 ? "text" : "string"));

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
        const char* pszLastComponent = m_aaosFieldPaths[i].back();

        const char* pszType = "string";
        const char* pszFormat = nullptr;

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

        bool bAnalyzed = EQUAL(pszType, "string");
        json_object* poPropertyMap = json_object_new_object();
        if( m_poDS->m_nMajorVersion >= 5 && EQUAL(pszType, "string") )
        {
            if( CSLFindString(m_papszNotAnalyzedFields,
                              poFieldDefn->GetNameRef()) >= 0 ||
                (CSLCount(m_papszNotAnalyzedFields) == 1 &&
                 EQUAL(m_papszNotAnalyzedFields[0], "{ALL}")) )
            {
                bAnalyzed = false;
                pszType = "keyword";
            }
            else
                pszType = "text";
        }
        json_object_object_add(poPropertyMap, "type", json_object_new_string(pszType));
        if( pszFormat )
            json_object_object_add(poPropertyMap, "format", json_object_new_string(pszFormat));
        if( m_bStoreFields || CSLFindString(m_papszStoredFields, poFieldDefn->GetNameRef()) >= 0 )
            json_object_object_add(poPropertyMap, "store", json_object_new_string("yes"));
        if( m_poDS->m_nMajorVersion < 5 &&
            (CSLFindString(m_papszNotAnalyzedFields,
                          poFieldDefn->GetNameRef()) >= 0  ||
                (CSLCount(m_papszNotAnalyzedFields) == 1 &&
                 EQUAL(m_papszNotAnalyzedFields[0], "{ALL}"))) )
        {
            bAnalyzed = false;
            json_object_object_add(poPropertyMap, "index", json_object_new_string("not_analyzed"));
        }
        else if( CSLFindString(m_papszNotIndexedFields, poFieldDefn->GetNameRef()) >= 0 )
            json_object_object_add(poPropertyMap, "index", json_object_new_string("no"));

        if( bAnalyzed && (CSLFindString(m_papszFieldsWithRawValue,
                          poFieldDefn->GetNameRef()) >= 0  ||
                (CSLCount(m_papszFieldsWithRawValue) == 1 &&
                 EQUAL(m_papszFieldsWithRawValue[0], "{ALL}"))) )
        {
            json_object* poFields = json_object_new_object();
            json_object* poRaw = json_object_new_object();
            json_object_object_add(poFields, "raw", poRaw);
            if( m_poDS->m_nMajorVersion >= 5 )
            {
                json_object_object_add(poRaw, "type",
                                       json_object_new_string("keyword"));
            }
            else
            {
                json_object_object_add(poRaw, "type",
                                       json_object_new_string("string"));
                json_object_object_add(poRaw, "index",
                                       json_object_new_string("not_analyzed"));
            }
            json_object_object_add(poPropertyMap, "fields", poFields);
        }

        json_object_object_add(poContainer, pszLastComponent, poPropertyMap);
    }

    for(int i=0;i<m_poFeatureDefn->GetGeomFieldCount();i++)
    {
        std::vector<CPLString> aosPath = m_aaosGeomFieldPaths[i];
        bool bAddGeoJSONType = false;
        if( m_abIsGeoPoint[i] &&
            aosPath.size() >= 2 &&
            aosPath.back() == "coordinates" )
        {
            bAddGeoJSONType = true;
            aosPath.resize( (int)aosPath.size() - 1 );
        }

        json_object* poContainer = GetContainerForMapping(poMappingProperties,
                                                        aosPath,
                                                        oMap);
        const char* pszLastComponent = aosPath.back();

        if( m_abIsGeoPoint[i] )
        {
            json_object* geo_point = AddPropertyMap("geo_point");
            if( bAddGeoJSONType )
            {
                json_object *geometry = AppendGroup(poContainer, pszLastComponent);
                json_object_object_add(geometry, "type", AddPropertyMap(
                   m_poDS->m_nMajorVersion >= 5 ? "text" : "string"));
                json_object_object_add(geometry, "coordinates", geo_point);
            }
            else
            {
                json_object_object_add(poContainer, pszLastComponent, geo_point);
            }
            if( !m_osPrecision.empty() )
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
            if( !m_osPrecision.empty() )
                json_object_object_add(geometry, "precision", json_object_new_string(m_osPrecision.c_str()));
        }
    }

    json_object* poMeta = nullptr;
    json_object* poGeomFields = nullptr;
    json_object* poFields = nullptr;
    if( !m_osFID.empty() )
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
            if( poMeta == nullptr )
                poMeta = json_object_new_object();
            if( poGeomFields == nullptr )
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
            if( poMeta == nullptr )
                poMeta = json_object_new_object();
            if( poFields == nullptr )
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

static void BuildGeoJSONGeometry(json_object* geometry, const OGRGeometry* poGeom)
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
            const OGRPoint* poPoint = poGeom->toPoint();
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            json_object_array_add(coordinates, json_object_new_double_with_precision(poPoint->getX(), nPrecision));
            json_object_array_add(coordinates, json_object_new_double_with_precision(poPoint->getY(), nPrecision));
            break;
        }

        case wkbLineString:
        {
            const OGRLineString* poLS = poGeom->toLineString();
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
            const OGRPolygon* poPoly = poGeom->toPolygon();
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for( auto&& poLS: *poPoly )
            {
                json_object *ring = json_object_new_array();
                json_object_array_add(coordinates, ring);
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
            const OGRMultiPoint* poMP = poGeom->toMultiPoint();
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for( auto&& poPoint: *poMP )
            {
                json_object *point = json_object_new_array();
                json_object_array_add(coordinates, point);
                json_object_array_add(point, json_object_new_double_with_precision(poPoint->getX(), nPrecision));
                json_object_array_add(point, json_object_new_double_with_precision(poPoint->getY(), nPrecision));
            }
            break;
        }

        case wkbMultiLineString:
        {
            const OGRMultiLineString* poMLS = poGeom->toMultiLineString();
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for( auto&& poLS: *poMLS )
            {
                json_object *ls = json_object_new_array();
                json_object_array_add(coordinates, ls);
                for( auto&& oPoint: *poLS )
                {
                    json_object *point = json_object_new_array();
                    json_object_array_add(ls, point);
                    json_object_array_add(point, json_object_new_double_with_precision(oPoint.getX(), nPrecision));
                    json_object_array_add(point, json_object_new_double_with_precision(oPoint.getY(), nPrecision));
                }
            }
            break;
        }

        case wkbMultiPolygon:
        {
            const OGRMultiPolygon* poMP = poGeom->toMultiPolygon();
            json_object *coordinates = json_object_new_array();
            json_object_object_add(geometry, "coordinates", coordinates);
            for( auto&& poPoly: *poMP )
            {
                json_object *poly = json_object_new_array();
                json_object_array_add(coordinates, poly);
                for( auto&& poLS: *poPoly )
                {
                    json_object *ring = json_object_new_array();
                    json_object_array_add(poly, ring);
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
            const OGRGeometryCollection* poGC = poGeom->toGeometryCollection();
            json_object *geometries = json_object_new_array();
            json_object_object_add(geometry, "geometries", geometries);
            for( auto&& poSubGeom: *poGC )
            {
                json_object *subgeom = json_object_new_object();
                json_object_array_add(geometries, subgeom);
                BuildGeoJSONGeometry(subgeom, poSubGeom);
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
    if (!m_osWriteMapFilename.empty() ) {
        if( m_bSerializeMapping )
        {
            m_bSerializeMapping = false;
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
    if( m_osWriteMapFilename.empty() && m_bSerializeMapping )
    {
        m_bSerializeMapping = false;
        CPLString osURL = BuildMappingURL(true);
        if( !m_poDS->UploadFile(osURL.c_str(), BuildMap()) )
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
/*                        BuildMappingURL()                             */
/************************************************************************/
CPLString OGRElasticLayer::BuildMappingURL(bool bMappingApi)
{
    CPLString osURL = CPLSPrintf("%s/%s", m_poDS->GetURL(), m_osIndexName.c_str());
    if (bMappingApi)
        osURL += "/_mapping";
    if (m_poDS->m_nMajorVersion < 7)
        osURL += CPLSPrintf("/%s", m_osMappingName.c_str());
    return osURL;
}

/************************************************************************/
/*                        BuildJSonFromFeature()                        */
/************************************************************************/

CPLString OGRElasticLayer::BuildJSonFromFeature(OGRFeature *poFeature)
{

    CPLString fields;
    int nJSonFieldIndex = m_poFeatureDefn->GetFieldIndex("_json");
    if( nJSonFieldIndex >= 0 && poFeature->IsFieldSetAndNotNull(nJSonFieldIndex) )
    {
        fields = poFeature->GetFieldAsString(nJSonFieldIndex);
    }
    else
    {
        json_object *fieldObject = json_object_new_object();

        if( poFeature->GetFID() >= 0 && !m_osFID.empty() )
        {
            json_object_object_add(fieldObject,
                                   m_osFID.c_str(),
                                   json_object_new_int64(poFeature->GetFID()) );
        }

        std::map< std::vector<CPLString>, json_object* > oMap;

        for(int i=0;i<poFeature->GetGeomFieldCount();i++)
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom != nullptr && !poGeom->IsEmpty() )
            {
                OGREnvelope env;
                poGeom->getEnvelope(&env);

                if( m_apoCT[i] != nullptr )
                    poGeom->transform( m_apoCT[i] );
                else if( env.MinX < -180 || env.MinY < -90 ||
                         env.MaxX > 180 || env.MaxY > 90 )
                {
                    static bool bHasWarned = false;
                    if( !bHasWarned )
                    {
                        bHasWarned = true;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "At least one geometry has a bounding box outside "
                                 "of [-180,180] longitude range and/or [-90,90] latitude range. Undefined behavior");
                    }
                }

                std::vector<CPLString> aosPath = m_aaosGeomFieldPaths[i];
                bool bAddGeoJSONType = false;
                if( m_abIsGeoPoint[i] &&
                    aosPath.size() >= 2 &&
                    aosPath.back() == "coordinates" )
                {
                    bAddGeoJSONType = true;
                    aosPath.resize( (int)aosPath.size() - 1 );
                }

                json_object* poContainer = GetContainerForFeature(fieldObject, aosPath, oMap);
                const char* pszLastComponent = aosPath.back();

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
                        json_object_object_add(geometry, "type", json_object_new_string("Point"));
                        json_object_object_add(geometry, "coordinates", coordinates);
                    }
                    else
                    {
                        json_object_object_add(poContainer, pszLastComponent, coordinates);
                    }
                }
                else
                {
                    if( m_bGeoShapeAsGeoJSON )
                    {
                        json_object *geometry = json_object_new_object();
                        json_object_object_add(poContainer, pszLastComponent, geometry);
                        BuildGeoJSONGeometry(geometry, poGeom);
                    }
                    else
                    {
                        char* pszWKT = nullptr;
                        poGeom->exportToWkt(&pszWKT);
                        json_object_object_add(poContainer, pszLastComponent,
                                               json_object_new_string(pszWKT));
                        CPLFree(pszWKT);
                    }
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
            const char* pszLastComponent = m_aaosFieldPaths[i].back();

            if( poFeature->IsFieldNull(i) )
            {
                json_object_object_add(poContainer,
                                       pszLastComponent, nullptr);
                continue;
            }

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
                            json_object_new_double_with_significant_figures(poFeature->GetFieldAsDouble(i), -1));
                    break;
                case OFTIntegerList:
                {
                    int nCount = 0;
                    const int* panValues = poFeature->GetFieldAsIntegerList(i, &nCount);
                    json_object* poArray = json_object_new_array();
                    for( int j = 0; j < nCount; j++ )
                        json_object_array_add(poArray, json_object_new_int(panValues[j]));
                    json_object_object_add(poContainer,
                            pszLastComponent, poArray);
                    break;
                }
                case OFTInteger64List:
                {
                    int nCount = 0;
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
                    int nCount = 0;
                    const double* padfValues = poFeature->GetFieldAsDoubleList(i, &nCount);
                    json_object* poArray = json_object_new_array();
                    for(int j=0;j<nCount;j++)
                        json_object_array_add(poArray, json_object_new_double_with_significant_figures(padfValues[j], -1));
                    json_object_object_add(poContainer,
                            pszLastComponent, poArray);
                    break;
                }
                case OFTStringList:
                {
                    char** papszValues = poFeature->GetFieldAsStringList(i);
                    json_object* poArray = json_object_new_array();
                    for(int j=0;papszValues[j]!= nullptr;j++)
                        json_object_array_add(poArray, json_object_new_string(papszValues[j]));
                    json_object_object_add(poContainer,
                            pszLastComponent, poArray);
                    break;
                }
                case OFTBinary:
                {
                    int nCount = 0;
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
                    int nYear = 0;
                    int nMonth = 0;
                    int nDay = 0;
                    int nHour = 0;
                    int nMin = 0;
                    int nTZ = 0;
                    float fSec = 0.0f;
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
                        const int TZOffset = std::abs(nTZ - 100) * 15;
                        const int TZHour = TZOffset / 60;
                        const int TZMinute = TZOffset - TZHour * 60;
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
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    FinalizeFeatureDefn();

    if( WriteMapIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if (!m_osWriteMapFilename.empty() )
        return OGRERR_NONE;

    if( poFeature->GetFID() < 0 )
    {
        if( m_nNextFID < 0 )
            m_nNextFID = GetFeatureCount(FALSE);
        poFeature->SetFID(++m_nNextFID);
    }

    CPLString osFields(BuildJSonFromFeature(poFeature));

    const char* pszId = nullptr;
    if( poFeature->IsFieldSetAndNotNull(0) && !m_bIgnoreSourceID )
        pszId = poFeature->GetFieldAsString(0);

    // Check to see if we're using bulk uploading
    if (m_nBulkUpload > 0) {
        m_osBulkContent += CPLSPrintf("{\"index\" :{\"_index\":\"%s\"", m_osIndexName.c_str());
        if(m_poDS->m_nMajorVersion < 7)
            m_osBulkContent += CPLSPrintf(", \"_type\":\"%s\"", m_osMappingName.c_str());
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
    }
    else
    {
        // Fall back to using single item upload for every feature.
        CPLString osURL(BuildMappingURL(false));
        if ( pszId )
            osURL += CPLSPrintf("/%s", pszId);
        json_object* poRes = m_poDS->RunRequest(osURL, osFields);
        if( poRes == nullptr )
        {
            return OGRERR_FAILURE;
        }
        if( pszId == nullptr )
        {
            json_object* poId = CPL_json_object_object_get(poRes, "_id");
            if( poId != nullptr && json_object_get_type(poId) == json_type_string )
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
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    FinalizeFeatureDefn();

    if( !poFeature->IsFieldSetAndNotNull(0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "_id field not set");
        return OGRERR_FAILURE;
    }
    if( poFeature->GetFID() < 0 && !m_osFID.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid FID");
        return OGRERR_FAILURE;
    }

    if( WriteMapIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    PushIndex();

    CPLString osFields(BuildJSonFromFeature(poFeature));

    // TODO? we should theoretically detect if the provided _id doesn't exist
    CPLString osURL(CPLSPrintf("%s/%s",
                               m_poDS->GetURL(), m_osIndexName.c_str()));
    if(m_poDS->m_nMajorVersion < 7)
        osURL += CPLSPrintf("/%s", m_osMappingName.c_str());
    osURL += CPLSPrintf("/%s", poFeature->GetFieldAsString(0));
    json_object* poRes = m_poDS->RunRequest(osURL, osFields);
    if( poRes == nullptr )
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

bool OGRElasticLayer::PushIndex()
{
    if( m_osBulkContent.empty() )
    {
        return true;
    }

    const bool bRet =
      m_poDS->UploadFile(CPLSPrintf("%s/_bulk", m_poDS->GetURL()),
                         m_osBulkContent);
    m_osBulkContent.clear();

    return bRet;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRElasticLayer::CreateField(OGRFieldDefn *poFieldDefn,
                                    int /*bApproxOK*/)
{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

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

    m_bSerializeMapping = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRElasticLayer::CreateGeomField( OGRGeomFieldDefn *poFieldIn,
                                         int /*bApproxOK*/ )

{
    if( m_poDS->GetAccess() != GA_Update )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset opened in read-only mode");
        return OGRERR_FAILURE;
    }

    FinalizeFeatureDefn();
    ResetReading();

    if( m_poFeatureDefn->GetGeomFieldIndex(poFieldIn->GetNameRef()) >= 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CreateGeomField() called with an already existing field name: %s",
                  poFieldIn->GetNameRef());
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oFieldDefn(poFieldIn);
    if( oFieldDefn.GetSpatialRef() )
    {
        oFieldDefn.GetSpatialRef()->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
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
         poFieldIn->GetType() != wkbPoint))
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

    OGRCoordinateTransformation* poCT = nullptr;
    if( oFieldDefn.GetSpatialRef() != nullptr )
    {
        OGRSpatialReference oSRS_WGS84;
        oSRS_WGS84.SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
        oSRS_WGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( !oSRS_WGS84.IsSame(oFieldDefn.GetSpatialRef()) )
        {
            poCT = OGRCreateCoordinateTransformation( oFieldDefn.GetSpatialRef(), &oSRS_WGS84 );
            if( poCT == nullptr )
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

    m_bSerializeMapping = true;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRElasticLayer::TestCapability(const char * pszCap) {
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poAttrQuery == nullptr && m_poFilterGeom == nullptr;

    else if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;

    else if (EQUAL(pszCap, OLCSequentialWrite) ||
             EQUAL(pszCap, OLCRandomWrite) )
        return m_poDS->GetAccess() == GA_Update;
    else if (EQUAL(pszCap, OLCCreateField) ||
             EQUAL(pszCap, OLCCreateGeomField) )
        return m_poDS->GetAccess() == GA_Update;
    else
        return FALSE;
}

/************************************************************************/
/*                   AddTimeoutTerminateAfterToURL()                    */
/************************************************************************/

void OGRElasticLayer::AddTimeoutTerminateAfterToURL( CPLString& osURL )
{
    if( !m_osSingleQueryTimeout.empty() )
        osURL += "&timeout=" + m_osSingleQueryTimeout;
    if( !m_osSingleQueryTerminateAfter.empty() )
        osURL += "&terminate_after=" + m_osSingleQueryTerminateAfter;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRElasticLayer::GetFeatureCount( int bForce )
{
    if( m_bFilterMustBeClientSideEvaluated )
    {
        m_bUseSingleQueryParams = true;
        const auto nRet = OGRLayer::GetFeatureCount(bForce);
        m_bUseSingleQueryParams = false;
        return nRet;
    }

    json_object* poResponse = nullptr;
    CPLString osURL(CPLSPrintf("%s", m_poDS->GetURL()));
    CPLString osFilter = "";
    if( !m_osESSearch.empty() )
    {
        if( m_osESSearch[0] != '{' )
            return OGRLayer::GetFeatureCount(bForce);
        osURL += "/_search?pretty";
        osFilter = "{ \"size\": 0 ";
        if( m_osESSearch == "{}" )
            osFilter += '}';
        else
            osFilter += ", " + m_osESSearch.substr(1);
    }
    else if( (m_poSpatialFilter && m_osJSONFilter.empty()) || m_poJSONFilter )
    {
        osFilter = BuildQuery(true);
        osURL += CPLSPrintf("/%s", m_osIndexName.c_str());
        if (m_poDS->m_nMajorVersion < 7)
            osURL += CPLSPrintf("/%s", m_osMappingName.c_str());
        if( m_poDS->m_nMajorVersion >= 5 && m_osSingleQueryTimeout.empty() )
        {
            osURL += "/_count?pretty";
        }
        else
        {
            osURL += "/_search?pretty";
        }
    }
    else if( !m_osJSONFilter.empty() )
    {
        osURL += CPLSPrintf("/%s", m_osIndexName.c_str());
        if (m_poDS->m_nMajorVersion < 7)
            osURL += CPLSPrintf("/%s", m_osMappingName.c_str());
        osURL += "/_search?pretty";
        osFilter = ("{ \"size\": 0, " + m_osJSONFilter.substr(1));
    }
    else
    {
        osURL += CPLSPrintf("/%s", m_osIndexName.c_str());
        if (m_poDS->m_nMajorVersion < 7)
            osURL += CPLSPrintf("/%s", m_osMappingName.c_str());
        if( m_osSingleQueryTimeout.empty() )
        {
            osURL += "/_count?pretty";
        }
        else
        {
            osFilter = "{ \"size\": 0 }";
            osURL += CPLSPrintf("/_search?pretty");
        }
    }
    AddTimeoutTerminateAfterToURL(osURL);

    poResponse = m_poDS->RunRequest(osURL.c_str(), osFilter.c_str());

    json_object* poCount = json_ex_get_object_by_path(poResponse, "hits.count");
    if( poCount == nullptr )
    {
        // For _search request
        poCount = json_ex_get_object_by_path(poResponse, "hits.total");
        if( poCount && json_object_get_type(poCount) == json_type_object )
        {
            // Since ES 7.0
            poCount = json_ex_get_object_by_path(poCount, "value");
        }
    }
    if( poCount == nullptr )
    {
        // For _count request
        poCount = json_ex_get_object_by_path(poResponse, "count");
    }
    if( poCount == nullptr || json_object_get_type(poCount) != json_type_int )
    {
        json_object_put(poResponse);
        CPLDebug("ES",
                 "Cannot find hits in GetFeatureCount() response. "
                 "Falling back to slow implementation");
        m_bUseSingleQueryParams = true;
        const auto nRet = OGRLayer::GetFeatureCount(bForce);
        m_bUseSingleQueryParams = false;
        return nRet;
    }

    GIntBig nCount = json_object_get_int64(poCount);
    json_object_put(poResponse);
    return nCount;
}

/************************************************************************/
/*                            GetValue()                                */
/************************************************************************/

json_object* OGRElasticLayer::GetValue( int nFieldIdx,
                                        swq_expr_node* poValNode )
{
    json_object* poVal = nullptr;
    if (poValNode->field_type == SWQ_FLOAT)
        poVal = json_object_new_double(poValNode->float_value);
    else if (poValNode->field_type == SWQ_INTEGER ||
             poValNode->field_type == SWQ_INTEGER64)
        poVal = json_object_new_int64(poValNode->int_value);
    else if (poValNode->field_type == SWQ_STRING)
        poVal = json_object_new_string(poValNode->string_value);
    else if (poValNode->field_type == SWQ_TIMESTAMP)
    {
        int nYear = 0;
        int nMonth = 0;
        int nDay = 0;
        int nHour = 0;
        int nMinute = 0;
        float fSecond = 0;
        if( sscanf(poValNode->string_value,"%04d/%02d/%02d %02d:%02d:%f",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSecond) >= 3 ||
            sscanf(poValNode->string_value,"%04d-%02d-%02dT%02d:%02d:%f",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSecond) >= 3 )
        {
            OGRFieldType eType(m_poFeatureDefn->GetFieldDefn(
                    nFieldIdx)->GetType());
            if( eType == OFTDateTime )
                poVal = json_object_new_string(
                    CPLSPrintf("%04d/%02d/%02d %02d:%02d:%02.03f",
                        nYear, nMonth, nDay, nHour, nMinute, fSecond));
            else if( eType == OFTDate)
                poVal = json_object_new_string(
                    CPLSPrintf("%04d/%02d/%02d",
                        nYear, nMonth, nDay));
            else
                poVal = json_object_new_string(
                    CPLSPrintf("%02d:%02d:%02.03f",
                        nHour, nMinute, fSecond));
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unhandled type: %d",
                 poValNode->field_type);
    }
    return poVal;
}

/************************************************************************/
/*                      OGRESGetFieldIndexFromSQL()                     */
/************************************************************************/

static int OGRESGetFieldIndexFromSQL(const swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_COLUMN )
        return poNode->field_index;

    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_CAST &&
        poNode->nSubExprCount >= 1 &&
        poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN )
        return poNode->papoSubExpr[0]->field_index;

    return -1;
}

/************************************************************************/
/*                        TranslateSQLToFilter()                        */
/************************************************************************/

json_object* OGRElasticLayer::TranslateSQLToFilter(swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION )
    {
        int nFieldIdx;
        if( poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
        {
            // For AND, we can deal with a failure in one of the branch
            // since client-side will do that extra filtering
            json_object* poFilter1 = TranslateSQLToFilter(poNode->papoSubExpr[0]);
            json_object* poFilter2 = TranslateSQLToFilter(poNode->papoSubExpr[1]);
            if( poFilter1 && poFilter2 )
            {
                json_object* poRet = json_object_new_object();
                json_object* poBool = json_object_new_object();
                json_object_object_add(poRet, "bool", poBool);
                json_object* poMust = json_object_new_array();
                json_object_object_add(poBool, "must", poMust);
                json_object_array_add(poMust, poFilter1);
                json_object_array_add(poMust, poFilter2);
                return poRet;
            }
            else if( poFilter1 )
                return poFilter1;
            else
                return poFilter2;
        }
        else if( poNode->nOperation == SWQ_OR && poNode->nSubExprCount == 2 )
        {
            json_object* poFilter1 = TranslateSQLToFilter(poNode->papoSubExpr[0]);
            json_object* poFilter2 = TranslateSQLToFilter(poNode->papoSubExpr[1]);
            if( poFilter1 && poFilter2 )
            {
                json_object* poRet = json_object_new_object();
                json_object* poBool = json_object_new_object();
                json_object_object_add(poRet, "bool", poBool);
                json_object* poShould = json_object_new_array();
                json_object_object_add(poBool, "should", poShould);
                json_object_array_add(poShould, poFilter1);
                json_object_array_add(poShould, poFilter2);
                return poRet;
            }
            else
            {
                json_object_put(poFilter1);
                json_object_put(poFilter2);
                return nullptr;
            }
        }
        else if( poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 )
        {
            if( poNode->papoSubExpr[0]->eNodeType == SNT_OPERATION &&
                poNode->papoSubExpr[0]->nOperation == SWQ_ISNULL &&
                poNode->papoSubExpr[0]->nSubExprCount == 1 &&
                poNode->papoSubExpr[0]->papoSubExpr[0]->field_index != 0 &&
                poNode->papoSubExpr[0]->papoSubExpr[0]->field_index <
                                        m_poFeatureDefn->GetFieldCount() )
            {
                json_object* poRet = json_object_new_object();
                json_object* poExists = json_object_new_object();
                CPLString osFieldName(BuildPathFromArray(
                    m_aaosFieldPaths[
                        poNode->papoSubExpr[0]->papoSubExpr[0]->field_index]));
                json_object_object_add(poExists, "field",
                                       json_object_new_string(osFieldName));
                json_object_object_add(poRet, "exists", poExists);
                return poRet;
            }
            else
            {
                json_object* poFilter = TranslateSQLToFilter(
                                                    poNode->papoSubExpr[0]);
                if( poFilter )
                {
                    json_object* poRet = json_object_new_object();
                    json_object* poBool = json_object_new_object();
                    json_object_object_add(poRet, "bool", poBool);
                    json_object_object_add(poBool, "must_not", poFilter);
                    return poRet;
                }
                else
                {
                    return nullptr;
                }
            }
        }
        else if( poNode->nOperation == SWQ_ISNULL &&
                 poNode->nSubExprCount == 1 &&
                 (nFieldIdx = OGRESGetFieldIndexFromSQL(poNode->papoSubExpr[0])) > 0 &&
                 nFieldIdx < m_poFeatureDefn->GetFieldCount() )
        {
            json_object* poRet = json_object_new_object();
            json_object* poExists = json_object_new_object();
            CPLString osFieldName(BuildPathFromArray(
                m_aaosFieldPaths[nFieldIdx]));
            json_object_object_add(poExists, "field",
                                    json_object_new_string(osFieldName));
            json_object* poBool = json_object_new_object();
            json_object_object_add(poRet, "bool", poBool);
            json_object* poMustNot = json_object_new_object();
            json_object_object_add(poMustNot, "exists", poExists);
            json_object_object_add(poBool, "must_not", poMustNot);
            return poRet;
        }
        else if( poNode->nOperation == SWQ_NE )
        {
            poNode->nOperation = SWQ_EQ;
            json_object* poFilter = TranslateSQLToFilter(poNode);
            poNode->nOperation = SWQ_NE;
            if( poFilter )
            {
                json_object* poRet = json_object_new_object();
                json_object* poBool = json_object_new_object();
                json_object_object_add(poRet, "bool", poBool);
                json_object_object_add(poBool, "must_not", poFilter);
                return poRet;
            }
            else
            {
                return nullptr;
            }
        }
        else if( poNode->nOperation == SWQ_EQ && poNode->nSubExprCount == 2 &&
                 poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
                 (nFieldIdx = OGRESGetFieldIndexFromSQL(poNode->papoSubExpr[0])) >= 0 &&
                 nFieldIdx < m_poFeatureDefn->GetFieldCount() )
        {
            json_object* poVal = GetValue(nFieldIdx,
                                          poNode->papoSubExpr[1]);
            if( poVal == nullptr )
            {
                return nullptr;
            }
            json_object* poRet = json_object_new_object();
            if( nFieldIdx == 0 )
            {
                json_object* poIds = json_object_new_object();
                json_object* poValues = json_object_new_array();
                json_object_object_add(poIds, "values", poValues);
                json_object_array_add(poValues, poVal);
                json_object_object_add(poRet, "ids", poIds);
            }
            else
            {
                json_object* poTerm = json_object_new_object();
                CPLString osPath(BuildPathFromArray(
                        m_aaosFieldPaths[ nFieldIdx]));
                bool bNotAnalyzed = true;
                if( poNode->papoSubExpr[1]->field_type == SWQ_STRING )
                {
                    const char* pszFieldName =
                        m_poFeatureDefn->GetFieldDefn(
                            nFieldIdx)->GetNameRef();
                    bNotAnalyzed = CSLFindString(m_papszNotAnalyzedFields,
                                                 pszFieldName) >= 0;
                    if( !bNotAnalyzed )
                    {
                        if( CSLFindString(m_papszFieldsWithRawValue,
                                          pszFieldName) >= 0 )
                        {
                            osPath += ".raw";
                            bNotAnalyzed = true;
                        }
                        else if( !m_bFilterMustBeClientSideEvaluated )
                        {
                            m_bFilterMustBeClientSideEvaluated = true;
                            CPLDebug("ES",
                            "Part or full filter will have to be evaluated on "
                            "client side (equality test on a analyzed "
                            "field).");
                        }
                    }
                }
                json_object_object_add(poRet,
                                    bNotAnalyzed ? "term" : "match", poTerm);
                json_object_object_add(poTerm, osPath, poVal);

                if( !bNotAnalyzed && m_poDS->m_nMajorVersion < 2 )
                {
                    json_object* poNewRet = json_object_new_object();
                    json_object_object_add(poNewRet, "query", poRet);
                    poRet = poNewRet;
                }
            }
            return poRet;
        }
        else if( (poNode->nOperation == SWQ_LT ||
                  poNode->nOperation == SWQ_LE ||
                  poNode->nOperation == SWQ_GT ||
                  poNode->nOperation == SWQ_GE) &&
                  poNode->nSubExprCount == 2 &&
                  poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
                  (nFieldIdx = OGRESGetFieldIndexFromSQL(poNode->papoSubExpr[0])) > 0 &&
                  nFieldIdx < m_poFeatureDefn->GetFieldCount() )
        {
            json_object* poVal = GetValue(nFieldIdx,
                                          poNode->papoSubExpr[1]);
            if( poVal == nullptr )
            {
                return nullptr;
            }
            json_object* poRet = json_object_new_object();
            json_object* poRange = json_object_new_object();
            json_object_object_add(poRet, "range", poRange);
            json_object* poFieldConstraint = json_object_new_object();
            CPLString osFieldName(BuildPathFromArray(
                    m_aaosFieldPaths[nFieldIdx]));
            json_object_object_add(poRange, osFieldName, poFieldConstraint);
            const char* pszOp = (poNode->nOperation == SWQ_LT) ? "lt" :
                                (poNode->nOperation == SWQ_LE) ? "lte" :
                                (poNode->nOperation == SWQ_GT) ? "gt" :
                                /*(poNode->nOperation == SWQ_GE) ?*/ "gte";
            json_object_object_add(poFieldConstraint, pszOp, poVal);
            return poRet;
        }
        else if( poNode->nOperation == SWQ_BETWEEN &&
                 poNode->nSubExprCount == 3 &&
                 poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
                 poNode->papoSubExpr[2]->eNodeType == SNT_CONSTANT &&
                 (nFieldIdx = OGRESGetFieldIndexFromSQL(poNode->papoSubExpr[0])) > 0 &&
                 nFieldIdx < m_poFeatureDefn->GetFieldCount() )
        {
            json_object* poVal1 = GetValue(nFieldIdx,
                                          poNode->papoSubExpr[1]);
            if( poVal1 == nullptr )
            {
                return nullptr;
            }
            json_object* poVal2 = GetValue(nFieldIdx,
                                          poNode->papoSubExpr[2]);
            if( poVal2 == nullptr )
            {
                json_object_put(poVal1);
                return nullptr;
            }

            json_object* poRet = json_object_new_object();
            json_object* poRange = json_object_new_object();
            json_object_object_add(poRet, "range", poRange);
            json_object* poFieldConstraint = json_object_new_object();
            CPLString osFieldName(BuildPathFromArray(
                    m_aaosFieldPaths[nFieldIdx]));
            json_object_object_add(poRange, osFieldName, poFieldConstraint);
            json_object_object_add(poFieldConstraint, "gte", poVal1);
            json_object_object_add(poFieldConstraint, "lte", poVal2);
            return poRet;
        }
        else if( poNode->nOperation == SWQ_IN &&
                 poNode->nSubExprCount > 1 &&
                 (nFieldIdx = OGRESGetFieldIndexFromSQL(poNode->papoSubExpr[0])) >= 0 &&
                 nFieldIdx < m_poFeatureDefn->GetFieldCount() )
        {
            bool bAllConstant = true;
            for( int i=1; i< poNode->nSubExprCount; i++ )
            {
                if( poNode->papoSubExpr[i]->eNodeType != SNT_CONSTANT )
                {
                    bAllConstant = false;
                    break;
                }
            }
            if( bAllConstant )
            {
                json_object* poRet = json_object_new_object();
                if( nFieldIdx == 0 )
                {
                    json_object* poIds = json_object_new_object();
                    json_object* poValues = json_object_new_array();
                    json_object_object_add(poIds, "values", poValues);
                    json_object_object_add(poRet, "ids", poIds);
                        for( int i=1; i< poNode->nSubExprCount; i++ )
                    {
                        json_object* poVal = GetValue(
                                            nFieldIdx,
                                            poNode->papoSubExpr[i]);
                        if( poVal == nullptr )
                        {
                            json_object_put(poRet);
                            return nullptr;
                        }
                        json_object_array_add(poValues, poVal);
                    }
                }
                else
                {
                    bool bNotAnalyzed = true;
                    CPLString osPath(BuildPathFromArray(
                        m_aaosFieldPaths[nFieldIdx]));
                    if( poNode->papoSubExpr[1]->field_type == SWQ_STRING )
                    {
                        const char* pszFieldName =
                            m_poFeatureDefn->GetFieldDefn(nFieldIdx)->
                                                                GetNameRef();
                        bNotAnalyzed = CSLFindString(m_papszNotAnalyzedFields,
                                                     pszFieldName) >= 0;
                        if( !bNotAnalyzed &&
                            CSLFindString(m_papszFieldsWithRawValue,
                                                            pszFieldName) >= 0 )
                        {
                            osPath += ".raw";
                            bNotAnalyzed = true;
                        }

                        if( !bNotAnalyzed &&
                            !m_bFilterMustBeClientSideEvaluated )
                        {
                            m_bFilterMustBeClientSideEvaluated = true;
                            CPLDebug("ES",
                                    "Part or full filter will have to be "
                                    "evaluated on client side (IN test on a "
                                    "analyzed field).");
                        }
                    }

                    if( bNotAnalyzed )
                    {
                        json_object* poTerms = json_object_new_object();
                        json_object_object_add(poRet, "terms", poTerms);
                        json_object* poTermsValues = json_object_new_array();
                        json_object_object_add(poTerms, osPath,
                                               poTermsValues);
                        for( int i=1; i< poNode->nSubExprCount; i++ )
                        {
                            json_object* poVal = GetValue(
                                        nFieldIdx,
                                        poNode->papoSubExpr[i]);
                            if( poVal == nullptr )
                            {
                                json_object_put(poRet);
                                return nullptr;
                            }
                            json_object_array_add(poTermsValues, poVal);
                        }
                    }
                    else
                    {
                        json_object* poBool = json_object_new_object();
                        json_object_object_add(poRet, "bool", poBool);
                        json_object* poShould = json_object_new_array();
                        json_object_object_add(poBool, "should", poShould);
                        for( int i=1; i< poNode->nSubExprCount; i++ )
                        {
                            json_object* poVal = GetValue(
                                        nFieldIdx,
                                        poNode->papoSubExpr[i]);
                            if( poVal == nullptr )
                            {
                                json_object_put(poRet);
                                return nullptr;
                            }
                            json_object* poShouldElt = json_object_new_object();
                            json_object* poMatch = json_object_new_object();
                            json_object_object_add(poShouldElt, "match",
                                                   poMatch);
                            json_object_object_add(poMatch, osPath, poVal);

                            if( m_poDS->m_nMajorVersion < 2 )
                            {
                                json_object* poNewShouldElt =
                                                    json_object_new_object();
                                json_object_object_add(poNewShouldElt,
                                                       "query", poShouldElt);
                                poShouldElt = poNewShouldElt;
                            }
                            json_object_array_add(poShould, poShouldElt);
                        }
                    }
                }
                return poRet;
            }
        }
        else if( (poNode->nOperation == SWQ_LIKE ||
                  poNode->nOperation == SWQ_ILIKE ) && // ES actual semantics doesn't match exactly either...
                 poNode->nSubExprCount >= 2 &&
                 (nFieldIdx = OGRESGetFieldIndexFromSQL(poNode->papoSubExpr[0])) > 0 &&
                 nFieldIdx < m_poFeatureDefn->GetFieldCount() )
        {
            char chEscape = '\0';
            if( poNode->nSubExprCount == 3 )
                chEscape = poNode->papoSubExpr[2]->string_value[0];
            const char* pszPattern = poNode->papoSubExpr[1]->string_value;
            const char* pszFieldName = m_poFeatureDefn->GetFieldDefn(
                    nFieldIdx)->GetNameRef();
            bool bNotAnalyzed = CSLFindString(m_papszNotAnalyzedFields,
                                              pszFieldName) >= 0;
            CPLString osPath(BuildPathFromArray(
                    m_aaosFieldPaths[nFieldIdx]));
            if( !bNotAnalyzed && CSLFindString(m_papszFieldsWithRawValue,
                                                        pszFieldName) >= 0 )
            {
                osPath += ".raw";
                bNotAnalyzed = true;
            }

            if( strchr(pszPattern, '*') || strchr(pszPattern, '?') )
            {
                CPLDebug("ES", "Cannot handle * or ? in LIKE pattern");
            }
            else if( !bNotAnalyzed )
            {
                if( !m_bFilterMustBeClientSideEvaluated )
                {
                    m_bFilterMustBeClientSideEvaluated = true;
                    CPLDebug("ES",
                            "Part or full filter will have to be evaluated on "
                            "client side (wildcard test on a analyzed field).");
                }
            }
            else
            {
                CPLString osUnescaped;
                for( int i=0; pszPattern[i] != '\0'; ++i )
                {
                    if( chEscape == pszPattern[i] )
                    {
                        if( pszPattern[i+1] == '\0' )
                            break;
                        osUnescaped += pszPattern[i+1];
                        i ++;
                    }
                    else if( pszPattern[i] == '%' )
                    {
                        osUnescaped += '*';
                    }
                    else if( pszPattern[i] == '_' )
                    {
                        osUnescaped += '?';
                    }
                    else
                    {
                        osUnescaped += pszPattern[i];
                    }
                }
                json_object* poRet = json_object_new_object();
                json_object* poWildcard = json_object_new_object();
                json_object_object_add(poRet, "wildcard", poWildcard);
                json_object_object_add(poWildcard, osPath,
                    json_object_new_string(osUnescaped));
                return poRet;
            }
        }
    }

    if( !m_bFilterMustBeClientSideEvaluated )
    {
        m_bFilterMustBeClientSideEvaluated = true;
        CPLDebug("ES",
                 "Part or full filter will have to be evaluated on "
                 "client side.");
    }
    return nullptr;
}

/************************************************************************/
/*                          SetAttributeFilter()                        */
/************************************************************************/

OGRErr OGRElasticLayer::SetAttributeFilter(const char* pszFilter)
{
    m_bFilterMustBeClientSideEvaluated = false;
    if( pszFilter != nullptr && pszFilter[0] == '{' )
    {
        if( !m_osESSearch.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Setting an Elasticsearch filter on a resulting layer "
                "is not supported");
            return OGRERR_FAILURE;
        }
        OGRLayer::SetAttributeFilter(nullptr);
        m_osJSONFilter = pszFilter;
        return OGRERR_NONE;
    }
    else
    {
        m_osJSONFilter.clear();
        json_object_put(m_poJSONFilter);
        m_poJSONFilter = nullptr;
        OGRErr eErr = OGRLayer::SetAttributeFilter(pszFilter);
        if( eErr == OGRERR_NONE && m_poAttrQuery != nullptr )
        {
            swq_expr_node* poNode = reinterpret_cast<swq_expr_node*>(
                                                m_poAttrQuery->GetSWQExpr());
            m_poJSONFilter = TranslateSQLToFilter(poNode);
        }
        return eErr;
    }
}

/************************************************************************/
/*                          ClampEnvelope()                             */
/************************************************************************/

void OGRElasticLayer::ClampEnvelope(OGREnvelope& sEnvelope)
{
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
    m_poSpatialFilter = nullptr;

    if( poGeomIn == nullptr )
        return;

    if( !m_osESSearch.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Setting a spatial filter on a resulting layer is not supported");
        return;
    }

    OGREnvelope sEnvelope;
    poGeomIn->getEnvelope(&sEnvelope);
    ClampEnvelope(sEnvelope);

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
        json_object_object_add(top_left, "lat", json_object_new_double_with_precision(sEnvelope.MaxY, 6));
        json_object_object_add(top_left, "lon", json_object_new_double_with_precision(sEnvelope.MinX, 6));

        json_object* bottom_right = json_object_new_object();
        json_object_object_add(field, "bottom_right", bottom_right);
        json_object_object_add(bottom_right, "lat", json_object_new_double_with_precision(sEnvelope.MinY, 6));
        json_object_object_add(bottom_right, "lon", json_object_new_double_with_precision(sEnvelope.MaxX, 6));
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
        json_object_array_add(top_left, json_object_new_double_with_precision(sEnvelope.MinX, 6));
        json_object_array_add(top_left, json_object_new_double_with_precision(sEnvelope.MaxY, 6));
        json_object_array_add(coordinates, top_left);

        json_object* bottom_right = json_object_new_array();
        json_object_array_add(bottom_right, json_object_new_double_with_precision(sEnvelope.MaxX, 6));
        json_object_array_add(bottom_right, json_object_new_double_with_precision(sEnvelope.MinY, 6));
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

    // geo_shape aggregation is only available since ES 7.8, but only with XPack
    // for now
    if( !m_abIsGeoPoint[iGeomField] &&
        !(m_poDS->m_nMajorVersion > 7 ||
            (m_poDS->m_nMajorVersion == 7 && m_poDS->m_nMinorVersion >= 8)) )
    {
        m_bUseSingleQueryParams = true;
        const auto eRet = OGRLayer::GetExtentInternal(iGeomField, psExtent, bForce);
        m_bUseSingleQueryParams = false;
        return eRet;
    }

    CPLString osFilter = CPLSPrintf("{ \"size\": 0, \"aggs\" : { \"bbox\" : { \"geo_bounds\" : { \"field\" : \"%s\" } } } }",
                                    BuildPathFromArray(m_aaosGeomFieldPaths[iGeomField]).c_str() );
    CPLString osURL = CPLSPrintf("%s/%s", m_poDS->GetURL(), m_osIndexName.c_str());
    if (m_poDS->m_nMajorVersion < 7)
        osURL += CPLSPrintf("/%s", m_osMappingName.c_str());
    osURL += "/_search?pretty";
    AddTimeoutTerminateAfterToURL(osURL);

    CPLPushErrorHandler(CPLQuietErrorHandler);
    json_object* poResponse = m_poDS->RunRequest(osURL.c_str(), osFilter.c_str());
    CPLPopErrorHandler();
    if( poResponse == nullptr )
    {
        const char* pszLastErrorMsg = CPLGetLastErrorMsg();
        if( !m_abIsGeoPoint[iGeomField] &&
            strstr(pszLastErrorMsg, "Fielddata is not supported on field") != nullptr )
        {
            CPLDebug("ES",
                     "geo_bounds aggregation failed, likely because of lack "
                     "of XPack. Using client-side method");
            CPLErrorReset();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", pszLastErrorMsg);
        }
    }

    json_object* poBounds = json_ex_get_object_by_path(poResponse, "aggregations.bbox.bounds");
    json_object* poTopLeft = json_ex_get_object_by_path(poBounds, "top_left");
    json_object* poBottomRight = json_ex_get_object_by_path(poBounds, "bottom_right");
    json_object* poTopLeftLon = json_ex_get_object_by_path(poTopLeft, "lon");
    json_object* poTopLeftLat = json_ex_get_object_by_path(poTopLeft, "lat");
    json_object* poBottomRightLon = json_ex_get_object_by_path(poBottomRight, "lon");
    json_object* poBottomRightLat = json_ex_get_object_by_path(poBottomRight, "lat");

    OGRErr eErr;
    if( poTopLeftLon == nullptr || poTopLeftLat == nullptr ||
        poBottomRightLon == nullptr || poBottomRightLat == nullptr )
    {
        m_bUseSingleQueryParams = true;
        const auto eRet = OGRLayer::GetExtentInternal(iGeomField, psExtent, bForce);
        m_bUseSingleQueryParams = false;
        return eRet;
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
