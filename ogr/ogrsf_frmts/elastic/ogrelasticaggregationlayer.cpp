/******************************************************************************
 *
 * Project:  Elasticsearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogrgeojsonreader.h"
#include "cpl_json.h"

#include <set>

CPL_CVSID("$Id$")

/************************************************************************/
/*                       OGRElasticAggregationLayer()                   */
/************************************************************************/

OGRElasticAggregationLayer::OGRElasticAggregationLayer(OGRElasticDataSource* poDS):
    m_poDS(poDS)
{
    m_poFeatureDefn = new OGRFeatureDefn("aggregation");
    m_poFeatureDefn->SetGeomType(wkbPoint);
    m_poFeatureDefn->Reference();
    SetDescription(m_poFeatureDefn->GetName());

    OGRSpatialReference* poSRS_WGS84 = new OGRSpatialReference();
    poSRS_WGS84->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
    poSRS_WGS84->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS_WGS84);
    poSRS_WGS84->Dereference();

    OGRFieldDefn oKey("key", OFTString);
    m_poFeatureDefn->AddFieldDefn(&oKey);

    OGRFieldDefn oDocCount("doc_count", OFTInteger64);
    m_poFeatureDefn->AddFieldDefn(&oDocCount);
}

/************************************************************************/
/*                      ~OGRElasticAggregationLayer()                   */
/************************************************************************/

OGRElasticAggregationLayer::~OGRElasticAggregationLayer()
{
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                                  Build()                             */
/************************************************************************/

std::unique_ptr<OGRElasticAggregationLayer>
    OGRElasticAggregationLayer::Build(OGRElasticDataSource* poDS,
                                      const char* pszAggregation)
{
    CPLJSONDocument oDoc;
    if( !oDoc.LoadMemory(pszAggregation) )
        return nullptr;
    const auto oRoot = oDoc.GetRoot();
    const auto osIndex = oRoot.GetString("index");
    if( osIndex.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing 'index' member in AGGREGATION");
        return nullptr;
    }

    auto osGeometryField = oRoot.GetString("geometry_field");
    if( osGeometryField.empty() )
    {
        std::set<CPLString> oSetLayers;
        std::vector<std::unique_ptr<OGRElasticLayer>> apoLayers;
        poDS->FetchMapping(osIndex.c_str(), oSetLayers, apoLayers);
        if( apoLayers.size() == 1 )
        {
            apoLayers[0]->SetFeatureDefnFinalized();
            const int nGeomFieldCount = apoLayers[0]->GetLayerDefn()->GetGeomFieldCount();
            if( nGeomFieldCount == 1 )
            {
                std::vector<CPLString> aosPath;
                bool bIsGeoPoint = false;
                apoLayers[0]->GetGeomFieldProperties( 0, aosPath, bIsGeoPoint );
                for( const auto& osPart: aosPath )
                {
                    if( !osGeometryField.empty() )
                        osGeometryField += '.';
                    osGeometryField += osPart;
                }
            }
            else if( nGeomFieldCount == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                     "No geometry field found upon which to build aggregation");
                return nullptr;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Multiple geometry fields exist in the index. "
                         "Specify one with the 'geometry_field' member in AGGREGATION");
                return nullptr;
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing 'geometry_field' member in AGGREGATION");
            return nullptr;
        }
    }

    auto poLayer = std::unique_ptr<OGRElasticAggregationLayer>(
        new OGRElasticAggregationLayer(poDS));
    poLayer->m_osIndexName = osIndex;
    poLayer->m_osGeometryField = osGeometryField;

    // Parse geohash_grid options
    auto oGeohashGrid = oRoot["geohash_grid"];
    if( oGeohashGrid.IsValid() &&
        oGeohashGrid.GetType() == CPLJSONObject::Type::Object )
    {
        const int nPrecision = oGeohashGrid.GetInteger("precision");
        if( nPrecision > 0 )
            poLayer->m_nGeohashGridPrecision = nPrecision;

        const int nMaxSize = oGeohashGrid.GetInteger("size");
        if( nMaxSize > 0 )
            poLayer->m_nGeohashGridMaxSize = nMaxSize;
    }

    // Parse additional fields that correspond to statistical operations on
    // fields
    poLayer->m_oFieldDef = oRoot["fields"];
    if( poLayer->m_oFieldDef.IsValid() &&
        poLayer->m_oFieldDef.GetType() == CPLJSONObject::Type::Object )
    {
        // Start with stats, to keep track of the created columns, and
        // avoid duplicating them if a users ask for stats and min/max/etc.
        // on the same property.
        {
            auto oOp = poLayer->m_oFieldDef["stats"];
            if( oOp.IsValid() && oOp.GetType() == CPLJSONObject::Type::Array )
            {
                for( const auto& oField: oOp.ToArray() )
                {
                    if( oField.GetType() == CPLJSONObject::Type::String )
                    {
                        for( const char* pszOp: { "min", "max", "avg", "sum", "count" } )
                        {
                            OGRFieldDefn oFieldDefn(
                                CPLSPrintf("%s_%s", oField.ToString().c_str(), pszOp),
                                strcmp(pszOp, "count") == 0 ? OFTInteger64 : OFTReal);
                            poLayer->m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
                        }

                        CPLJSONObject oAgg;
                        CPLJSONObject oFieldAgg;
                        oFieldAgg.Add("field", oField.ToString());
                        oAgg.Add("stats", oFieldAgg);
                        poLayer->m_oAggregatedFieldsRequest.Add(
                            CPLSPrintf("%s_stats", oField.ToString().c_str()), oAgg);
                    }
                }
            }
        }

        for( const char* pszOp: { "min", "max", "avg", "sum", "count" } )
        {
            auto oOp = poLayer->m_oFieldDef[pszOp];
            if( oOp.IsValid() && oOp.GetType() == CPLJSONObject::Type::Array )
            {
                for( const auto& oField: oOp.ToArray() )
                {
                    if( oField.GetType() == CPLJSONObject::Type::String )
                    {
                        const char* pszFieldName = CPLSPrintf("%s_%s", oField.ToString().c_str(), pszOp);
                        if( poLayer->m_poFeatureDefn->GetFieldIndex(pszFieldName) < 0 )
                        {
                            OGRFieldDefn oFieldDefn(
                                pszFieldName,
                                strcmp(pszOp, "count") == 0 ? OFTInteger64 : OFTReal);
                            poLayer->m_poFeatureDefn->AddFieldDefn(&oFieldDefn);

                            CPLJSONObject oAgg;
                            CPLJSONObject oFieldAgg;
                            oFieldAgg.Add("field", oField.ToString());
                            oAgg.Add(strcmp(pszOp, "count") == 0 ? "value_count": pszOp, oFieldAgg);
                            poLayer->m_oAggregatedFieldsRequest.Add(oFieldDefn.GetNameRef(), oAgg);
                        }
                    }
                }
            }
        }
    }

    return poLayer;
}

/************************************************************************/
/*                          ResetReading()                              */
/************************************************************************/

void OGRElasticAggregationLayer::ResetReading()
{
    m_iCurFeature = 0;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRElasticAggregationLayer::SetSpatialFilter( OGRGeometry * poGeom )

{
    OGRLayer::SetSpatialFilter(poGeom);
    m_bFeaturesRequested = false;
    m_apoCachedFeatures.clear();
}

/************************************************************************/
/*                          BuildRequest()                              */
/************************************************************************/

#define FILTERED_STR "filtered"
#define GRID_STR     "grid"
#define CENTROID_STR "centroid"

// Return a JSON serialized document that is the payload to POST for a /_search
// request
std::string OGRElasticAggregationLayer::BuildRequest()
{
    CPLJSONDocument oDoc;
    auto oRoot = oDoc.GetRoot();
    oRoot.Set("size", 0);

    auto aggs = CPLJSONObject();

    // Clamp spatial filter if needed
    m_bRequestHasSpatialFilter = false;
    OGREnvelope sEnvelope;
    if( m_poFilterGeom )
    {
        m_poFilterGeom->getEnvelope(&sEnvelope);

        OGRElasticLayer::ClampEnvelope(sEnvelope);
        if( !(sEnvelope.MinX == -180 && sEnvelope.MinY == -90 &&
              sEnvelope.MaxX == 180 && sEnvelope.MaxY == 90 ) )
        {
            m_bRequestHasSpatialFilter = true;
        }
    }

    if( m_bRequestHasSpatialFilter )
    {
        // Add spatial filtering
        auto top_aggs = CPLJSONObject();
        oRoot.Add("aggs", top_aggs);

        auto filtered = CPLJSONObject();
        top_aggs.Add(FILTERED_STR, filtered);

        auto filter = CPLJSONObject();
        filtered.Add("filter", filter);
        filtered.Add("aggs", aggs);

        auto geo_bounding_box = CPLJSONObject();
        filter.Add("geo_bounding_box", geo_bounding_box);

        auto coordinates = CPLJSONObject();
        geo_bounding_box.Add(m_osGeometryField, coordinates);

        auto top_left = CPLJSONObject();
        coordinates.Add("top_left", top_left);
        top_left.Add("lat", sEnvelope.MaxY);
        top_left.Add("lon", sEnvelope.MinX);

        auto bottom_right = CPLJSONObject();
        coordinates.Add("bottom_right", bottom_right);
        bottom_right.Add("lat", sEnvelope.MinY);
        bottom_right.Add("lon", sEnvelope.MaxX);
    }
    else
    {
        oRoot.Add("aggs", aggs);
    }

    auto grid = CPLJSONObject();
    aggs.Add(GRID_STR, grid);

    // Build geohash_grid aggregation object
    auto geohash_grid = CPLJSONObject();
    grid.Add("geohash_grid", geohash_grid);
    geohash_grid.Set("field", m_osGeometryField);

    if( m_nGeohashGridPrecision >= 1 )
    {
        geohash_grid.Set("precision", m_nGeohashGridPrecision);
    }
    else if( !m_bRequestHasSpatialFilter ||
             (sEnvelope.MinX < sEnvelope.MaxX && sEnvelope.MinY < sEnvelope.MaxY) )
    {
        const double dfSpatialRatio = m_bRequestHasSpatialFilter ?
             (sEnvelope.MaxX - sEnvelope.MinX) / 360. *  (sEnvelope.MaxY - sEnvelope.MinY) / 180. :
             1.0;

        // A geohash of size 1 can encode up to 32 positions, size 2 up to 32*32
        // etc.
        const int geohashSize = static_cast<int>(std::min(12.0,
             std::max(1.0, log(m_nGeohashGridMaxSize / dfSpatialRatio) / log(32.0))));

        geohash_grid.Set("precision", geohashSize);
    }
    geohash_grid.Set("size", m_nGeohashGridMaxSize);

    auto subaggs = CPLJSONObject();
    grid.Add("aggs", subaggs);

    auto centroid = CPLJSONObject();
    subaggs.Add(CENTROID_STR, centroid);

    auto geo_centroid = CPLJSONObject();
    centroid.Add("geo_centroid", geo_centroid);

    geo_centroid.Set("field", m_osGeometryField);

    // Add extra fields
    for( auto oChild: m_oAggregatedFieldsRequest.GetChildren() )
    {
        subaggs.Add(oChild.GetName(), oChild);
    }

    return oRoot.Format(CPLJSONObject::PrettyFormat::Plain);
}

/************************************************************************/
/*                       IssueAggregationRequest()                      */
/************************************************************************/

void OGRElasticAggregationLayer::IssueAggregationRequest()
{
    const auto IsNumericJsonType = [](json_type type) {
        return type == json_type_int || type == json_type_double;
    };

    m_apoCachedFeatures.clear();

    json_object* poResponse = m_poDS->RunRequest(
        (std::string(m_poDS->GetURL()) + "/" + m_osIndexName + "/_search").c_str(),
        BuildRequest().c_str());
    if( !poResponse )
        return;
    json_object* poBuckets = json_ex_get_object_by_path(poResponse,
        m_bRequestHasSpatialFilter ?
            "aggregations." FILTERED_STR "." GRID_STR ".buckets" :
            "aggregations." GRID_STR ".buckets");
    if( poBuckets && json_object_get_type(poBuckets) == json_type_array )
    {
        const auto nBuckets = json_object_array_length(poBuckets);
        for(auto i=decltype(nBuckets){0};i<nBuckets;i++)
        {
            json_object* poBucket = json_object_array_get_idx(poBuckets, i);
            if( poBucket && json_object_get_type(poBucket) == json_type_object )
            {
                OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
                poFeature->SetFID(i);

                json_object* poKey = CPL_json_object_object_get(poBucket, "key");
                if( poKey && json_object_get_type(poKey) == json_type_string )
                {
                    poFeature->SetField("key", json_object_get_string(poKey));
                }

                json_object* poDocCount = CPL_json_object_object_get(
                                                        poBucket, "doc_count");
                if( poDocCount && json_object_get_type(poDocCount) == json_type_int )
                {
                    poFeature->SetField("doc_count", static_cast<GIntBig>(
                        json_object_get_int64(poDocCount)));
                }

                json_object* poLocation = json_ex_get_object_by_path(
                                                poBucket, CENTROID_STR ".location");
                if( poLocation && json_object_get_type(poLocation) == json_type_object )
                {
                    json_object* poLat = CPL_json_object_object_get(poLocation, "lat");
                    json_object* poLon = CPL_json_object_object_get(poLocation, "lon");
                    if( poLat && IsNumericJsonType(json_object_get_type(poLat)) &&
                        poLon && IsNumericJsonType(json_object_get_type(poLon)) )
                    {
                        OGRPoint* poPoint = new OGRPoint(
                            json_object_get_double(poLon),
                            json_object_get_double(poLat)
                        );
                        poPoint->assignSpatialReference(
                            m_poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef());
                        poFeature->SetGeometryDirectly(poPoint);
                    }
                }

                if( m_oFieldDef.IsValid() &&
                    m_oFieldDef.GetType() == CPLJSONObject::Type::Object )
                {
                    for( const char* pszOp: { "min", "max", "avg", "sum", "count" } )
                    {
                        auto oOp = m_oFieldDef[pszOp];
                        if( oOp.IsValid() && oOp.GetType() == CPLJSONObject::Type::Array )
                        {
                            for( const auto& oField: oOp.ToArray() )
                            {
                                if( oField.GetType() == CPLJSONObject::Type::String )
                                {
                                    json_object* poField = json_ex_get_object_by_path(
                                        poBucket, CPLSPrintf("%s_%s.value", oField.ToString().c_str(), pszOp));
                                    if( poField && IsNumericJsonType(json_object_get_type(poField)) )
                                    {
                                        const char* pszFieldName =
                                            CPLSPrintf("%s_%s", oField.ToString().c_str(), pszOp);
                                        if( strcmp(pszOp, "count") == 0 )
                                        {
                                            poFeature->SetField(
                                                pszFieldName,
                                                static_cast<GIntBig>(json_object_get_int64(poField)));
                                        }
                                        else
                                        {
                                            poFeature->SetField(
                                                pszFieldName,
                                                json_object_get_double(poField));
                                        }
                                    }
                                }
                            }
                        }
                    }

                    auto oOp = m_oFieldDef["stats"];
                    if( oOp.IsValid() && oOp.GetType() == CPLJSONObject::Type::Array )
                    {
                        for( const auto& oField: oOp.ToArray() )
                        {
                            if( oField.GetType() == CPLJSONObject::Type::String )
                            {
                                for( const char* pszOp: { "min", "max", "avg", "sum", "count" } )
                                {
                                    json_object* poField = json_ex_get_object_by_path(
                                        poBucket, CPLSPrintf("%s_stats.%s", oField.ToString().c_str(), pszOp));
                                    if( poField && IsNumericJsonType(json_object_get_type(poField)) )
                                    {
                                        const char* pszFieldName =
                                            CPLSPrintf("%s_%s", oField.ToString().c_str(), pszOp);
                                        if( strcmp(pszOp, "count") == 0 )
                                        {
                                            poFeature->SetField(
                                                pszFieldName,
                                                static_cast<GIntBig>(json_object_get_int64(poField)));
                                        }
                                        else
                                        {
                                            poFeature->SetField(
                                                pszFieldName,
                                                json_object_get_double(poField));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                m_apoCachedFeatures.emplace_back(poFeature);
            }
        }
    }
    json_object_put(poResponse);
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature* OGRElasticAggregationLayer::GetNextRawFeature()
{
    if( !m_bFeaturesRequested )
    {
        m_bFeaturesRequested = true;
        IssueAggregationRequest();
    }
    if( m_iCurFeature < static_cast<int>(m_apoCachedFeatures.size()) )
    {
        auto poFeature = m_apoCachedFeatures[m_iCurFeature]->Clone();
        ++m_iCurFeature;
        return poFeature;
    }

    return nullptr;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRElasticAggregationLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
    {
        if( !m_bFeaturesRequested )
        {
            m_bFeaturesRequested = true;
            IssueAggregationRequest();
        }
        return static_cast<int>(m_apoCachedFeatures.size());
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                          TestCapability()                            */
/************************************************************************/

int OGRElasticAggregationLayer::TestCapability(const char* pszCap)
{
    return EQUAL(pszCap, OLCStringsAsUTF8);
}
