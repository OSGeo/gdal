/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  Implements OGRPLScenesDataV1Layer
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Planet Labs
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

#include "ogr_plscenes.h"
#include "ogrgeojsonreader.h"
#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                           GetFieldCount()                            */
/************************************************************************/

int OGRPLScenesDataV1FeatureDefn::GetFieldCount() const
{
    if( nFieldCount == 0 && m_poLayer != nullptr )
        m_poLayer->EstablishLayerDefn();
    return nFieldCount;
}

/************************************************************************/
/*                        OGRPLScenesDataV1Layer()                      */
/************************************************************************/

OGRPLScenesDataV1Layer::OGRPLScenesDataV1Layer( OGRPLScenesDataV1Dataset* poDS,
                                                const char* pszName ) :
    m_poDS(poDS),
    m_bFeatureDefnEstablished(false),
    m_poSRS(new OGRSpatialReference(SRS_WKT_WGS84_LAT_LONG)),
    m_nTotalFeatures(-1),
    m_nNextFID(1),
    m_bEOF(false),
    m_bStillInFirstPage(true),
    m_nPageSize(atoi(CPLGetConfigOption("PLSCENES_PAGE_SIZE", "250"))),
    m_bInFeatureCountOrGetExtent(false),
    m_poPageObj(nullptr),
    m_poFeatures(nullptr),
    m_nFeatureIdx(0),
    m_poAttributeFilter(nullptr),
    m_bFilterMustBeClientSideEvaluated(false)
{
    m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // Cannot be moved to initializer list because of use of this, which MSVC 2008 doesn't like
    m_poFeatureDefn = new OGRPLScenesDataV1FeatureDefn(this, pszName);

    SetDescription(pszName);
    m_poFeatureDefn->SetGeomType(wkbMultiPolygon);
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_poSRS);
    OGRPLScenesDataV1Layer::ResetReading();
}

/************************************************************************/
/*                      ~OGRPLScenesDataV1Layer()                       */
/************************************************************************/

OGRPLScenesDataV1Layer::~OGRPLScenesDataV1Layer()
{
    m_poFeatureDefn->DropRefToLayer();
    m_poFeatureDefn->Release();
    m_poSRS->Release();
    if( m_poPageObj != nullptr )
        json_object_put(m_poPageObj);
    if( m_poAttributeFilter != nullptr )
        json_object_put(m_poAttributeFilter);
}

/************************************************************************/
/*                             GetLayerDefn()                           */
/************************************************************************/

OGRFeatureDefn* OGRPLScenesDataV1Layer::GetLayerDefn()
{
    return m_poFeatureDefn;
}

/************************************************************************/
/*                          RegisterField()                             */
/************************************************************************/

void OGRPLScenesDataV1Layer::RegisterField(OGRFieldDefn* poFieldDefn,
                                       const char* pszQueryableJSonName,
                                       const char* pszPrefixedJSonName)
{
    const int nIdx = m_poFeatureDefn->GetFieldCount();
    m_oMapPrefixedJSonFieldNameToFieldIdx[pszPrefixedJSonName] = nIdx;
    if( pszQueryableJSonName )
    {
        m_oMapFieldIdxToQueryableJSonFieldName[nIdx] = pszQueryableJSonName;
    }
    m_poFeatureDefn->AddFieldDefn(poFieldDefn);
}

/************************************************************************/
/*                         EstablishLayerDefn()                         */
/************************************************************************/

void OGRPLScenesDataV1Layer::EstablishLayerDefn()
{
    if( m_bFeatureDefnEstablished )
        return;
    m_bFeatureDefnEstablished = true;

    const char* pszConfFile = CPLFindFile("gdal", "plscenesconf.json");
    if( pszConfFile == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find plscenesconf.json");
        return;
    }

    GByte* pabyRet = nullptr;
    if( !VSIIngestFile( nullptr, pszConfFile, &pabyRet, nullptr, -1 ) )
    {
        return;
    }

    json_object* poRoot = nullptr;
    const char* pzText = reinterpret_cast<char*>(pabyRet);
    if( !OGRJSonParse( pzText, &poRoot ) )
    {
        VSIFree(pabyRet);
        return;
    }
    VSIFree(pabyRet);

    json_object* poV1Data = CPL_json_object_object_get(poRoot, "v1_data");
    if( poV1Data == nullptr || json_object_get_type(poV1Data) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find v1_data object in plscenesconf.json");
        json_object_put(poRoot);
        return;
    }

    json_object* poItemType = CPL_json_object_object_get(poV1Data,
                                                         GetDescription());
    if( poItemType == nullptr ||
        json_object_get_type(poItemType) != json_type_object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find v1_data.%s object in plscenesconf.json",
                 GetDescription());
        json_object_put(poRoot);
        return;
    }

    json_object* poFields = CPL_json_object_object_get(poItemType, "fields");
    if( poFields == nullptr ||
        json_object_get_type(poFields) != json_type_array )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find v1_data.%s.fields object in plscenesconf.json",
                 GetDescription());
        json_object_put(poRoot);
        return;
    }

    {
        OGRFieldDefn oFieldDefn("id", OFTString);
        RegisterField(&oFieldDefn, "id", "id");
    }
    const auto nFields = json_object_array_length(poFields);
    for( auto i=decltype(nFields){0}; i<nFields; i++ )
    {
        json_object* poField = json_object_array_get_idx(poFields, i);
        if( poField && json_object_get_type(poField) == json_type_object )
        {
            json_object* poName = CPL_json_object_object_get(poField, "name");
            json_object* poType = CPL_json_object_object_get(poField, "type");
            if( poName && json_object_get_type(poName) == json_type_string &&
                poType && json_object_get_type(poType) == json_type_string )
            {
                const char* pszName = json_object_get_string(poName);
                const char* pszType = json_object_get_string(poType);
                OGRFieldType eType(OFTString);
                OGRFieldSubType eSubType(OFSTNone);
                if( EQUAL(pszType, "datetime") )
                    eType = OFTDateTime;
                else if( EQUAL(pszType, "double") )
                    eType = OFTReal;
                else if( EQUAL(pszType, "int") )
                    eType = OFTInteger;
                else if( EQUAL(pszType, "string") )
                    eType = OFTString;
                else if( EQUAL(pszType, "boolean") )
                {
                    eType = OFTInteger;
                    eSubType = OFSTBoolean;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unrecognized field type %s for field %s",
                             pszType, pszName);
                }
                OGRFieldDefn oFieldDefn(pszName, eType);
                oFieldDefn.SetSubType(eSubType);
                RegisterField(&oFieldDefn, pszName,
                              (CPLString("properties.") + pszName).c_str());
            }
        }
    }

    {
        OGRFieldDefn oFieldDefn("self_link", OFTString);
        RegisterField(&oFieldDefn, nullptr, "_links._self");
    }

    {
        OGRFieldDefn oFieldDefn("assets_link", OFTString);
        RegisterField(&oFieldDefn, nullptr, "_links.assets");
    }

    {
        OGRFieldDefn oFieldDefn("permissions", OFTStringList);
        RegisterField(&oFieldDefn, nullptr, "_permissions");
    }

    if( m_poDS->DoesFollowLinks() )
    {
        json_object* poAssets = CPL_json_object_object_get(poItemType, "assets");
        if( poAssets == nullptr ||
            json_object_get_type(poAssets) != json_type_array )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find v1_data.%s.assets object in plscenesconf.json",
                    GetDescription());
            json_object_put(poRoot);
            return;
        }

        const auto nAssets = json_object_array_length(poAssets);
        for( auto i=decltype(nAssets){0}; i<nAssets; i++ )
        {
            json_object* poAsset = json_object_array_get_idx(poAssets, i);
            if( poAsset && json_object_get_type(poAsset) == json_type_string )
            {
                const char* pszAsset = json_object_get_string(poAsset);
                m_oSetAssets.insert(pszAsset);

                {
                    CPLString osName("asset_");
                    osName += pszAsset;
                    osName += "_self_link";
                    OGRFieldDefn oFieldDefn(osName, OFTString);
                    RegisterField(&oFieldDefn, nullptr,
                                  CPLSPrintf("/assets.%s._links._self", pszAsset));
                }
                {
                    CPLString osName("asset_");
                    osName += pszAsset;
                    osName += "_activate_link";
                    OGRFieldDefn oFieldDefn(osName, OFTString);
                    RegisterField(&oFieldDefn, nullptr,
                                  CPLSPrintf("/assets.%s._links.activate", pszAsset));
                }
                {
                    CPLString osName("asset_");
                    osName += pszAsset;
                    osName += "_permissions";
                    OGRFieldDefn oFieldDefn(osName, OFTStringList);
                    RegisterField(&oFieldDefn, nullptr,
                                  CPLSPrintf("/assets.%s._permissions", pszAsset));
                }
                {
                    CPLString osName("asset_");
                    osName += pszAsset;
                    osName += "_expires_at";
                    OGRFieldDefn oFieldDefn(osName, OFTDateTime);
                    RegisterField(&oFieldDefn, nullptr,
                                  CPLSPrintf("/assets.%s.expires_at", pszAsset));
                }
                {
                    CPLString osName("asset_");
                    osName += pszAsset;
                    osName += "_location";
                    OGRFieldDefn oFieldDefn(osName, OFTString);
                    RegisterField(&oFieldDefn, nullptr,
                                  CPLSPrintf("/assets.%s.location", pszAsset));
                }
                {
                    CPLString osName("asset_");
                    osName += pszAsset;
                    osName += "_status";
                    OGRFieldDefn oFieldDefn(osName, OFTString);
                    RegisterField(&oFieldDefn, nullptr,
                                  CPLSPrintf("/assets.%s.status", pszAsset));
                }
            }
        }
    }

    json_object_put(poRoot);
}

/************************************************************************/
/*                             GetMetadata()                            */
/************************************************************************/

char **OGRPLScenesDataV1Layer::GetMetadata( const char * pszDomain  )
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        EstablishLayerDefn();
    }
    return OGRLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                           GetMetadataItem()                          */
/************************************************************************/

const char *OGRPLScenesDataV1Layer::GetMetadataItem( const char * pszName, const char* pszDomain )
{
    if( pszDomain == nullptr || EQUAL(pszDomain, "") )
    {
        EstablishLayerDefn();
    }
    return OGRLayer::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                              GetNextPage()                           */
/************************************************************************/

bool OGRPLScenesDataV1Layer::GetNextPage()
{
    if( m_poPageObj != nullptr )
        json_object_put(m_poPageObj);
    m_poPageObj = nullptr;
    m_poFeatures = nullptr;
    m_nFeatureIdx = 0;

    if( m_osRequestURL.empty() )
    {
        m_bEOF = true;
        return false;
    }

    json_object* poObj;
    if (m_osRequestURL.find(m_poDS->GetBaseURL() + "quick-search?_page_size") == 0 )
    {
        CPLString osFilter(m_poDS->GetFilter());
        if( osFilter.empty() )
        {
            json_object* poFilterRoot = json_object_new_object();
            json_object* poItemTypes = json_object_new_array();
            json_object_array_add(poItemTypes,
                                  json_object_new_string(GetName()));
            json_object_object_add(poFilterRoot, "item_types", poItemTypes);
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilterRoot, "filter", poFilter);
            json_object_object_add(poFilter, "type",
                                   json_object_new_string("AndFilter"));
            json_object* poConfig = json_object_new_array();
            json_object_object_add(poFilter, "config", poConfig);

            if( m_poFilterGeom != nullptr )
            {
                json_object* poGeomFilter = json_object_new_object();
                json_object_array_add(poConfig, poGeomFilter);
                json_object_object_add(poGeomFilter, "type",
                                   json_object_new_string("GeometryFilter"));
                json_object_object_add(poGeomFilter, "field_name",
                                   json_object_new_string("geometry"));
                OGRGeoJSONWriteOptions oOptions;
                json_object* poGeoJSONGeom =
                            OGRGeoJSONWriteGeometry( m_poFilterGeom, oOptions );
                json_object_object_add(poGeomFilter, "config",
                                       poGeoJSONGeom);
            }
            if( m_poAttributeFilter != nullptr )
            {
                json_object_get(m_poAttributeFilter);
                json_object_array_add(poConfig, m_poAttributeFilter);
            }

            osFilter = json_object_to_json_string_ext(poFilterRoot, 0);
            json_object_put(poFilterRoot);
        }
        poObj = m_poDS->RunRequest(m_osRequestURL, FALSE, "POST", true,
                                   osFilter);
    }
    else
    {
        poObj = m_poDS->RunRequest(m_osRequestURL);
    }
    if( poObj == nullptr )
    {
        m_bEOF = true;
        return false;
    }

    json_object* poFeatures = CPL_json_object_object_get(poObj, "features");
    if( poFeatures == nullptr ||
        json_object_get_type(poFeatures) != json_type_array ||
        json_object_array_length(poFeatures) == 0 )
    {
        // If this is a single item, then wrap it in a features array
        json_object* poProperties = CPL_json_object_object_get(poObj, "properties");
        if( poProperties != nullptr )
        {
            m_poPageObj = json_object_new_object();
            poFeatures = json_object_new_array();
            json_object_array_add(poFeatures, poObj);
            json_object_object_add(m_poPageObj, "features", poFeatures);
            poObj = m_poPageObj;
        }
        else
        {
            json_object_put(poObj);
            m_bEOF = true;
            return false;
        }
    }

    m_poPageObj = poObj;
    m_poFeatures = poFeatures;

    // Get URL of next page
    m_osNextURL = "";
    json_object* poLinks = CPL_json_object_object_get(poObj, "_links");
    if( poLinks && json_object_get_type(poLinks) == json_type_object )
    {
        json_object* poNext = CPL_json_object_object_get(poLinks, "_next");
        if( poNext && json_object_get_type(poNext) == json_type_string )
        {
            m_osNextURL = json_object_get_string(poNext);
        }
    }

    return true;
}

/************************************************************************/
/*                             ResetReading()                           */
/************************************************************************/

void OGRPLScenesDataV1Layer::ResetReading()
{
    m_bEOF = false;

    if( m_poFeatures != nullptr && m_bStillInFirstPage )
        m_nFeatureIdx = 0;
    else
        m_poFeatures = nullptr;
    m_nNextFID = 1;
    m_bStillInFirstPage = true;
    m_osRequestURL = m_poDS->GetBaseURL() +
                        CPLSPrintf("quick-search?_page_size=%d", m_nPageSize);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRPLScenesDataV1Layer::SetSpatialFilter(  OGRGeometry *poGeomIn )
{
    m_poFeatures = nullptr;

    if( poGeomIn )
    {
        OGREnvelope sEnvelope;
        poGeomIn->getEnvelope(&sEnvelope);
        if( sEnvelope.MinX == sEnvelope.MaxX && sEnvelope.MinY == sEnvelope.MaxY )
        {
            OGRPoint p(sEnvelope.MinX, sEnvelope.MinY);
            InstallFilter(&p);
        }
        else
            InstallFilter( poGeomIn );
    }
    else
        InstallFilter( poGeomIn );

    ResetReading();
}

/************************************************************************/
/*                      OGRPLScenesDataV1ParseDateTime()                    */
/************************************************************************/

static bool OGRPLScenesDataV1ParseDateTime(const char* pszValue,
                                       int& nYear, int &nMonth, int &nDay,
                                       int& nHour, int &nMinute, int &nSecond)
{
    return ( sscanf(pszValue,"%04d/%02d/%02d %02d:%02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) >= 3 ||
             sscanf(pszValue,"%04d-%02d-%02dT%02d:%02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond) >= 3 );
}

/************************************************************************/
/*                          IsSimpleComparison()                        */
/************************************************************************/

bool OGRPLScenesDataV1Layer::IsSimpleComparison(const swq_expr_node* poNode)
{
    return  poNode->eNodeType == SNT_OPERATION &&
            (poNode->nOperation == SWQ_EQ ||
             poNode->nOperation == SWQ_NE ||
             poNode->nOperation == SWQ_LT ||
             poNode->nOperation == SWQ_LE ||
             poNode->nOperation == SWQ_GT ||
             poNode->nOperation == SWQ_GE) &&
             poNode->nSubExprCount == 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
             m_oMapFieldIdxToQueryableJSonFieldName.find(
                                    poNode->papoSubExpr[0]->field_index) !=
                                m_oMapFieldIdxToQueryableJSonFieldName.end();
}

/************************************************************************/
/*                             GetOperatorText()                        */
/************************************************************************/

static const char* GetOperatorText(swq_op nOp)
{
    if( nOp == SWQ_LT )
        return "lt";
    if( nOp == SWQ_LE )
            return "lte";
    if( nOp == SWQ_GT )
            return "gt";
    if( nOp == SWQ_GE )
            return "gte";
    CPLAssert(false);
    return "";
}

/************************************************************************/
/*                             BuildFilter()                            */
/************************************************************************/

json_object* OGRPLScenesDataV1Layer::BuildFilter(swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
    {
         // For AND, we can deal with a failure in one of the branch
        // since client-side will do that extra filtering
        json_object* poFilter1 = BuildFilter(poNode->papoSubExpr[0]);
        json_object* poFilter2 = BuildFilter(poNode->papoSubExpr[1]);
        if( poFilter1 && poFilter2 )
        {
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilter, "type",
                                   json_object_new_string("AndFilter"));
            json_object* poConfig = json_object_new_array();
            json_object_object_add(poFilter, "config", poConfig);
            json_object_array_add(poConfig, poFilter1);
            json_object_array_add(poConfig, poFilter2);
            return poFilter;
        }
        else if( poFilter1 )
            return poFilter1;
        else
            return poFilter2;
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_OR && poNode->nSubExprCount == 2 )
    {
         // For OR, we need both members to be valid
        json_object* poFilter1 = BuildFilter(poNode->papoSubExpr[0]);
        json_object* poFilter2 = BuildFilter(poNode->papoSubExpr[1]);
        if( poFilter1 && poFilter2 )
        {
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilter, "type",
                                   json_object_new_string("OrFilter"));
            json_object* poConfig = json_object_new_array();
            json_object_object_add(poFilter, "config", poConfig);
            json_object_array_add(poConfig, poFilter1);
            json_object_array_add(poConfig, poFilter2);
            return poFilter;
        }
        else
        {
            if( poFilter1 )
                json_object_put(poFilter1);
            if( poFilter2 )
                json_object_put(poFilter2);
            return nullptr;
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 )
    {
        json_object* poFilter1 = BuildFilter(poNode->papoSubExpr[0]);
        if( poFilter1 )
        {
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilter, "type",
                                   json_object_new_string("NotFilter"));
            json_object_object_add(poFilter, "config", poFilter1);
            return poFilter;
        }
        else
        {
            return nullptr;
        }
    }
    else if( IsSimpleComparison(poNode) )
    {
        int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
        const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
        if( poNode->nOperation == SWQ_NE )
        {
            poNode->nOperation = SWQ_EQ;
            json_object* poFilter1 = BuildFilter(poNode);
            poNode->nOperation = SWQ_NE;
            if( poFilter1 )
            {
                json_object* poFilter = json_object_new_object();
                json_object_object_add(poFilter, "type",
                                       json_object_new_string("NotFilter"));
                json_object_object_add(poFilter, "config", poFilter1);
                return poFilter;
            }
            else
            {
                return nullptr;
            }
        }
        else if( poNode->nOperation == SWQ_EQ &&
                 (m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTInteger ||
                  m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTReal) &&
                 (poNode->papoSubExpr[1]->field_type == SWQ_INTEGER ||
                  poNode->papoSubExpr[1]->field_type == SWQ_FLOAT) )
        {
            json_object* poFilter = json_object_new_object();
            if( m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTReal )
            {
                json_object_object_add(poFilter, "type",
                                    json_object_new_string("RangeFilter"));
                json_object_object_add(poFilter, "field_name",
                                    json_object_new_string(
                                        m_oMapFieldIdxToQueryableJSonFieldName[nFieldIdx]));
                json_object* poConfig = json_object_new_object();
                const double EPS = 1e-8;
                json_object_object_add(poConfig, "gte",
                    (poNode->papoSubExpr[1]->field_type == SWQ_INTEGER) ?
                        json_object_new_double(poNode->papoSubExpr[1]->int_value - EPS) :
                        json_object_new_double(poNode->papoSubExpr[1]->float_value - EPS));
                json_object_object_add(poConfig, "lte",
                    (poNode->papoSubExpr[1]->field_type == SWQ_INTEGER) ?
                        json_object_new_double(poNode->papoSubExpr[1]->int_value + EPS) :
                        json_object_new_double(poNode->papoSubExpr[1]->float_value + EPS));
                json_object_object_add(poFilter, "config", poConfig);
            }
            else
            {
                json_object_object_add(poFilter, "type",
                                    json_object_new_string("NumberInFilter"));
                json_object_object_add(poFilter, "field_name",
                                    json_object_new_string(
                                        m_oMapFieldIdxToQueryableJSonFieldName[nFieldIdx]));
                json_object* poConfig = json_object_new_array();
                json_object_array_add(poConfig,
                    (poNode->papoSubExpr[1]->field_type == SWQ_INTEGER) ?
                        json_object_new_int64(poNode->papoSubExpr[1]->int_value) :
                        json_object_new_double(poNode->papoSubExpr[1]->float_value));
                json_object_object_add(poFilter, "config", poConfig);
            }
            return poFilter;
        }
        else if( poNode->nOperation == SWQ_EQ &&
                 m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTString &&
                 poNode->papoSubExpr[1]->field_type == SWQ_STRING )
        {
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilter, "type",
                                json_object_new_string("StringInFilter"));
            json_object_object_add(poFilter, "field_name",
                        json_object_new_string(
                            m_oMapFieldIdxToQueryableJSonFieldName[nFieldIdx]));
            json_object* poConfig = json_object_new_array();
            json_object_array_add(poConfig,
                json_object_new_string(poNode->papoSubExpr[1]->string_value));
            json_object_object_add(poFilter, "config", poConfig);
            return poFilter;
        }
        else if( (poNode->nOperation == SWQ_LT ||
                  poNode->nOperation == SWQ_LE ||
                  poNode->nOperation == SWQ_GT ||
                  poNode->nOperation == SWQ_GE) &&
                 (m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTInteger ||
                  m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTReal) &&
                 (poNode->papoSubExpr[1]->field_type == SWQ_INTEGER ||
                  poNode->papoSubExpr[1]->field_type == SWQ_FLOAT) )
        {
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilter, "type",
                                   json_object_new_string("RangeFilter"));
            json_object_object_add(poFilter, "field_name",
                                   json_object_new_string(
                                       m_oMapFieldIdxToQueryableJSonFieldName[nFieldIdx]));
            json_object* poConfig = json_object_new_object();
            json_object_object_add(poConfig,
                GetOperatorText(poNode->nOperation),
                (poNode->papoSubExpr[1]->field_type == SWQ_INTEGER) ?
                    json_object_new_int64(poNode->papoSubExpr[1]->int_value) :
                    json_object_new_double(poNode->papoSubExpr[1]->float_value));
            json_object_object_add(poFilter, "config", poConfig);
            return poFilter;
        }
        else if( (poNode->nOperation == SWQ_LT ||
                  poNode->nOperation == SWQ_LE ||
                  poNode->nOperation == SWQ_GT ||
                  poNode->nOperation == SWQ_GE) &&
                 m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTDateTime &&
                 poNode->papoSubExpr[1]->field_type == SWQ_TIMESTAMP &&
                 OGRPLScenesDataV1ParseDateTime(poNode->papoSubExpr[1]->string_value,
                    nYear, nMonth, nDay, nHour, nMinute, nSecond) )
        {
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilter, "type",
                                   json_object_new_string("DateRangeFilter"));
            json_object_object_add(poFilter, "field_name",
                    json_object_new_string(
                        m_oMapFieldIdxToQueryableJSonFieldName[nFieldIdx]));
            json_object* poConfig = json_object_new_object();
            json_object_object_add(poConfig,
                GetOperatorText(poNode->nOperation),
                json_object_new_string(
                    CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
                                nYear, nMonth, nDay, nHour, nMinute, nSecond)));
            json_object_object_add(poFilter, "config", poConfig);
            return poFilter;
        }
    }
    else if ( poNode->eNodeType == SNT_OPERATION &&
              poNode->nOperation == SWQ_IN &&
              poNode->nSubExprCount >= 2 &&
              poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
              m_oMapFieldIdxToQueryableJSonFieldName.find(
                                    poNode->papoSubExpr[0]->field_index) !=
                                m_oMapFieldIdxToQueryableJSonFieldName.end() )
    {
        const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
        if( m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTString )
        {
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilter, "type",
                                json_object_new_string("StringInFilter"));
            json_object_object_add(poFilter, "field_name",
                        json_object_new_string(
                            m_oMapFieldIdxToQueryableJSonFieldName[nFieldIdx]));
            json_object* poConfig = json_object_new_array();
            json_object_object_add(poFilter, "config", poConfig);
            for( int i=1; i<poNode->nSubExprCount;i++)
            {
                if( poNode->papoSubExpr[i]->eNodeType != SNT_CONSTANT ||
                    poNode->papoSubExpr[i]->field_type != SWQ_STRING )
                {
                    json_object_put(poFilter);
                    m_bFilterMustBeClientSideEvaluated = true;
                    return nullptr;
                }
                json_object_array_add(poConfig, json_object_new_string(
                                    poNode->papoSubExpr[i]->string_value));
            }
            return poFilter;
        }
        else if( m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetType() == OFTInteger )
        {
            json_object* poFilter = json_object_new_object();
            json_object_object_add(poFilter, "type",
                                json_object_new_string("NumberInFilter"));
            json_object_object_add(poFilter, "field_name",
                        json_object_new_string(
                            m_oMapFieldIdxToQueryableJSonFieldName[nFieldIdx]));
            json_object* poConfig = json_object_new_array();
            json_object_object_add(poFilter, "config", poConfig);
            for( int i=1; i<poNode->nSubExprCount;i++)
            {
                if( poNode->papoSubExpr[i]->eNodeType != SNT_CONSTANT ||
                    poNode->papoSubExpr[i]->field_type != SWQ_INTEGER )
                {
                    json_object_put(poFilter);
                    m_bFilterMustBeClientSideEvaluated = true;
                    return nullptr;
                }
                json_object_array_add(poConfig, json_object_new_int64(
                                    poNode->papoSubExpr[i]->int_value));
            }
            return poFilter;
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_EQ &&
             poNode->nSubExprCount == 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
             poNode->papoSubExpr[0]->field_index ==
                           m_poFeatureDefn->GetFieldIndex("permissions") &&
             poNode->papoSubExpr[1]->field_type == SWQ_STRING )
    {
        json_object* poFilter = json_object_new_object();
        json_object_object_add(poFilter, "type",
                            json_object_new_string("PermissionFilter"));
        json_object* poConfig = json_object_new_array();
        json_object_object_add(poFilter, "config", poConfig);
        json_object_array_add(poConfig, json_object_new_string(
                                poNode->papoSubExpr[1]->string_value));
        return poFilter;
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_IN &&
             poNode->nSubExprCount >= 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[0]->field_index ==
                            m_poFeatureDefn->GetFieldIndex("permissions") )
    {
        json_object* poFilter = json_object_new_object();
        json_object_object_add(poFilter, "type",
                            json_object_new_string("PermissionFilter"));
        json_object* poConfig = json_object_new_array();
        json_object_object_add(poFilter, "config", poConfig);
        for( int i=1; i<poNode->nSubExprCount;i++)
        {
            if( poNode->papoSubExpr[i]->eNodeType != SNT_CONSTANT ||
                poNode->papoSubExpr[i]->field_type != SWQ_STRING )
            {
                json_object_put(poFilter);
                m_bFilterMustBeClientSideEvaluated = true;
                return nullptr;
            }
            json_object_array_add(poConfig, json_object_new_string(
                                    poNode->papoSubExpr[i]->string_value));
        }
        return poFilter;
    }

    m_bFilterMustBeClientSideEvaluated = true;
    return nullptr;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRPLScenesDataV1Layer::SetAttributeFilter( const char *pszQuery )

{
    m_poFeatures = nullptr;

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszQuery);

    if( m_poAttributeFilter )
        json_object_put(m_poAttributeFilter);
    m_poAttributeFilter = nullptr;
    m_bFilterMustBeClientSideEvaluated = false;
    if( m_poAttrQuery != nullptr )
    {
        swq_expr_node* poNode = (swq_expr_node*) m_poAttrQuery->GetSWQExpr();

        poNode->ReplaceBetweenByGEAndLERecurse();

        m_poAttributeFilter = BuildFilter(poNode);
        if( m_poAttributeFilter == nullptr )
        {
            CPLDebug("PLSCENES",
                        "Full filter will be evaluated on client side.");
        }
        else if( m_bFilterMustBeClientSideEvaluated )
        {
            CPLDebug("PLSCENES",
                "Only part of the filter will be evaluated on server side.");
        }
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPLScenesDataV1Layer::GetNextFeature()
{
    while( true )
    {
        OGRFeature  *poFeature = GetNextRawFeature();
        if (poFeature == nullptr)
            return nullptr;

        if( m_poAttrQuery == nullptr ||
            !m_bFilterMustBeClientSideEvaluated ||
            m_poAttrQuery->Evaluate( poFeature ) )
        {
            return poFeature;
        }
        else
        {
            delete poFeature;
        }
    }
}

/************************************************************************/
/*                            GetNextRawFeature()                       */
/************************************************************************/

OGRFeature* OGRPLScenesDataV1Layer::GetNextRawFeature()
{
    EstablishLayerDefn();
    if( m_bEOF )
        return nullptr;

    if( m_poFeatures == nullptr )
    {
        if( !GetNextPage() )
            return nullptr;
    }

    if( m_nFeatureIdx == static_cast<int>(json_object_array_length(m_poFeatures)) )
    {
        if( m_nFeatureIdx < m_nPageSize &&
            m_poDS->GetBaseURL().find("/vsimem/") != 0  )
        {
            return nullptr;
        }
        m_osRequestURL = m_osNextURL;
        m_bStillInFirstPage = false;
        if( !GetNextPage() )
            return nullptr;
    }
    json_object* poJSonFeature = json_object_array_get_idx(m_poFeatures, m_nFeatureIdx);
    m_nFeatureIdx ++;
    if( poJSonFeature == nullptr || json_object_get_type(poJSonFeature) != json_type_object )
    {
        m_bEOF = true;
        return nullptr;
    }

    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
    poFeature->SetFID(m_nNextFID++);

    json_object* poJSonGeom = CPL_json_object_object_get(poJSonFeature, "geometry");
    if( poJSonGeom != nullptr && json_object_get_type(poJSonGeom) == json_type_object )
    {
        OGRGeometry* poGeom = OGRGeoJSONReadGeometry(poJSonGeom);
        if( poGeom != nullptr )
        {
            if( poGeom->getGeometryType() == wkbPolygon )
            {
                OGRMultiPolygon* poMP = new OGRMultiPolygon();
                poMP->addGeometryDirectly(poGeom);
                poGeom = poMP;
            }
            poGeom->assignSpatialReference(m_poSRS);
            poFeature->SetGeometryDirectly(poGeom);
        }
    }

    json_object* poId = CPL_json_object_object_get(poJSonFeature, "id");
    if( poId != nullptr && json_object_get_type(poId) == json_type_string )
    {
        std::map<CPLString, int>::const_iterator oIter =
            m_oMapPrefixedJSonFieldNameToFieldIdx.find("id");
        if( oIter != m_oMapPrefixedJSonFieldNameToFieldIdx.end() )
        {
            const int iField = oIter->second;
            poFeature->SetField(iField, json_object_get_string(poId));
        }
    }

    json_object* poPermissions =
        CPL_json_object_object_get(poJSonFeature, "_permissions");
    if( poPermissions != nullptr &&
        json_object_get_type(poPermissions) == json_type_array )
    {
        std::map<CPLString, int>::const_iterator oIter =
                m_oMapPrefixedJSonFieldNameToFieldIdx.find("_permissions");
        if( oIter != m_oMapPrefixedJSonFieldNameToFieldIdx.end() )
        {
            const int iField = oIter->second;
            const auto nStrings = json_object_array_length(poPermissions);
            char** papszPermissions =
                static_cast<char**>(CPLCalloc(nStrings+1, sizeof(char*)));
            for(auto i=decltype(nStrings){0}, j=decltype(nStrings){0};
                i<nStrings;i++)
            {
                json_object* poPerm = json_object_array_get_idx(poPermissions,i);
                if( poPerm && json_object_get_type(poPerm) == json_type_string )
                {
                    papszPermissions[j++] =
                        CPLStrdup(json_object_get_string(poPerm));
                }
            }
            poFeature->SetField(iField, papszPermissions);
            CSLDestroy(papszPermissions);
        }
    }

    for(int i=0;i<2;i++)
    {
        const char* pszFeaturePart = (i == 0) ? "properties": "_links";
        json_object* poProperties =
                CPL_json_object_object_get(poJSonFeature, pszFeaturePart);
        if( poProperties != nullptr &&
            json_object_get_type(poProperties) == json_type_object )
        {
            json_object_iter it;
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
            json_object_object_foreachC( poProperties, it )
            {
                CPLString osPrefixedJSonFieldName(pszFeaturePart);
                osPrefixedJSonFieldName += ".";
                osPrefixedJSonFieldName += it.key;
                if( !SetFieldFromPrefixedJSonFieldName(
                        poFeature, osPrefixedJSonFieldName, it.val) )
                {
                    if( i == 0 &&
                        m_oSetUnregisteredFields.find(osPrefixedJSonFieldName)
                            == m_oSetUnregisteredFields.end() )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Field %s found in data but not "
                                 "in configuration",
                                 osPrefixedJSonFieldName.c_str());
                        m_oSetUnregisteredFields.insert(
                                                osPrefixedJSonFieldName);
                    }
                }
            }
        }
    }

    json_object* poAssets = nullptr;
    if( m_poDS->DoesFollowLinks() &&
        (!m_bInFeatureCountOrGetExtent || m_poAttrQuery != nullptr)  )
    {
        std::map<CPLString, int>::const_iterator oIter =
                m_oMapPrefixedJSonFieldNameToFieldIdx.find("_links.assets");
        if( oIter != m_oMapPrefixedJSonFieldNameToFieldIdx.end() )
        {
            const int iField = oIter->second;
            if( poFeature->IsFieldSetAndNotNull( iField ) )
            {
                const char* pszAssetURL = poFeature->GetFieldAsString( iField );
                poAssets = m_poDS->RunRequest(pszAssetURL);
            }
        }
    }
    if( poAssets != nullptr )
    {
        json_object_iter itAsset;
        itAsset.key = nullptr;
        itAsset.val = nullptr;
        itAsset.entry = nullptr;
        json_object_object_foreachC( poAssets, itAsset )
        {
            if( m_oSetAssets.find(itAsset.key) == m_oSetAssets.end() )
            {
                if( m_oSetUnregisteredAssets.find(itAsset.key) ==
                                    m_oSetUnregisteredAssets.end() )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "Asset %s found in data but "
                                "not in configuration",
                                itAsset.key);
                    m_oSetUnregisteredAssets.insert(itAsset.key);
                }
                continue;
            }

            json_object* poAsset = itAsset.val;
            if( poAsset != nullptr &&
                json_object_get_type(poAsset) == json_type_object )
            {
                json_object_iter it;
                it.key = nullptr;
                it.val = nullptr;
                it.entry = nullptr;
                json_object_object_foreachC( poAsset, it )
                {
                    if( it.val == nullptr ) continue;
                    CPLString osPrefixedJSonFieldName(
                                    "/assets." + CPLString(itAsset.key));
                    osPrefixedJSonFieldName += "." + CPLString(it.key);
                    if( strcmp(it.key, "_links") == 0 &&
                        json_object_get_type(it.val) == json_type_object )
                    {
                        if( CPL_json_object_object_get(it.val, "_self") != nullptr )
                        {
                            CPLString osPrefixedJSonFieldNameNew(
                                osPrefixedJSonFieldName + "._self");
                            SetFieldFromPrefixedJSonFieldName(
                                poFeature, osPrefixedJSonFieldNameNew,
                                CPL_json_object_object_get(it.val, "_self"));
                        }
                        if( CPL_json_object_object_get(it.val, "activate") != nullptr )
                        {
                            CPLString osPrefixedJSonFieldNameNew(
                                osPrefixedJSonFieldName + ".activate");
                            SetFieldFromPrefixedJSonFieldName(
                                poFeature, osPrefixedJSonFieldNameNew,
                                CPL_json_object_object_get(it.val, "activate"));
                        }
                    }
                    else
                    {
                        SetFieldFromPrefixedJSonFieldName(
                            poFeature, osPrefixedJSonFieldName, it.val);
                    }
                }
            }
        }
        json_object_put(poAssets);
    }

    return poFeature;
}

/************************************************************************/
/*                    SetFieldFromPrefixedJSonFieldName()               */
/************************************************************************/

bool OGRPLScenesDataV1Layer::SetFieldFromPrefixedJSonFieldName(
                                  OGRFeature* poFeature,
                                  const CPLString& osPrefixedJSonFieldName,
                                  json_object* poVal )
{
    std::map<CPLString, int>::const_iterator oIter =
        m_oMapPrefixedJSonFieldNameToFieldIdx.find(osPrefixedJSonFieldName);
    if( poVal != nullptr && oIter != m_oMapPrefixedJSonFieldNameToFieldIdx.end() )
    {
        const int iField = oIter->second;
        json_type eJSonType = json_object_get_type(poVal);
        if( eJSonType == json_type_int )
        {
            poFeature->SetField(iField,
                    static_cast<GIntBig>(json_object_get_int64(poVal)));
        }
        else if( eJSonType == json_type_double )
        {
            poFeature->SetField(iField, json_object_get_double(poVal));
        }
        else if( eJSonType == json_type_string )
        {
            poFeature->SetField(iField, json_object_get_string(poVal));
        }
        else if( eJSonType == json_type_boolean )
        {
            poFeature->SetField(iField, json_object_get_boolean(poVal));
        }
        else
        {
            poFeature->SetField(iField, json_object_to_json_string_ext( poVal, 0 ));
        }
        return true;
    }
    return false;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRPLScenesDataV1Layer::GetFeatureCount(int bForce)
{
    if( m_poDS->GetFilter().empty() )
    {
        if( m_nTotalFeatures >= 0 &&
            m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
        {
            return m_nTotalFeatures;
        }

        json_object* poFilterRoot = json_object_new_object();
        json_object* poItemTypes = json_object_new_array();
        json_object_array_add(poItemTypes,
                                json_object_new_string(GetName()));
        json_object_object_add(poFilterRoot, "interval",
                                json_object_new_string("year"));
        json_object_object_add(poFilterRoot, "item_types", poItemTypes);
        json_object* poFilter = json_object_new_object();
        json_object_object_add(poFilterRoot, "filter", poFilter);
        json_object_object_add(poFilter, "type",
                                json_object_new_string("AndFilter"));
        json_object* poConfig = json_object_new_array();
        json_object_object_add(poFilter, "config", poConfig);

        // We need to put a dummy filter
        if( m_poFilterGeom == nullptr && m_poAttributeFilter == nullptr )
        {
            json_object* poRangeFilter = json_object_new_object();
            json_object_array_add(poConfig, poRangeFilter);
            json_object_object_add(poRangeFilter, "type",
                                json_object_new_string("RangeFilter"));
            json_object_object_add(poRangeFilter, "field_name",
                                json_object_new_string("cloud_cover"));
            json_object* poRangeFilterConfig = json_object_new_object();
            json_object_object_add(poRangeFilterConfig, "gte",
                                   json_object_new_double(0.0));
            json_object_object_add(poRangeFilter, "config",
                                   poRangeFilterConfig);
        }

        if( m_poFilterGeom != nullptr )
        {
            json_object* poGeomFilter = json_object_new_object();
            json_object_array_add(poConfig, poGeomFilter);
            json_object_object_add(poGeomFilter, "type",
                                json_object_new_string("GeometryFilter"));
            json_object_object_add(poGeomFilter, "field_name",
                                json_object_new_string("geometry"));
            OGRGeoJSONWriteOptions oOptions;
            json_object* poGeoJSONGeom =
                        OGRGeoJSONWriteGeometry( m_poFilterGeom, oOptions );
            json_object_object_add(poGeomFilter, "config",
                                    poGeoJSONGeom);
        }
        if( m_poAttributeFilter != nullptr )
        {
            json_object_get(m_poAttributeFilter);
            json_object_array_add(poConfig, m_poAttributeFilter);
        }

        CPLString osFilter = json_object_to_json_string_ext(poFilterRoot, 0);
        json_object_put(poFilterRoot);

        json_object* poObj = m_poDS->RunRequest(
                                   (m_poDS->GetBaseURL() + "stats").c_str(),
                                   FALSE, "POST", true,
                                   osFilter);
        if( poObj != nullptr )
        {
            json_object* poBuckets =
                                CPL_json_object_object_get(poObj, "buckets");
            if( poBuckets && json_object_get_type(poBuckets) ==
                                                            json_type_array )
            {
                GIntBig nRes = 0;
                const auto nBuckets = json_object_array_length(poBuckets);
                for( auto i=decltype(nBuckets){0}; i<nBuckets;i++ )
                {
                    json_object* poBucket =
                                json_object_array_get_idx(poBuckets, i);
                    if( poBucket && json_object_get_type(poBucket) ==
                                                            json_type_object )
                    {
                        json_object* poCount =
                            CPL_json_object_object_get(poBucket, "count");
                        if( poCount && json_object_get_type(poCount) ==
                                                            json_type_int )
                        {
                            nRes += json_object_get_int64(poCount);
                        }
                    }
                }
                if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
                    m_nTotalFeatures = nRes;

                json_object_put(poObj);
                return nRes;
            }
            json_object_put(poObj);
        }
    }

    m_bInFeatureCountOrGetExtent = true;
    GIntBig nRes = OGRLayer::GetFeatureCount(bForce);
    m_bInFeatureCountOrGetExtent = false;
    return nRes;
}

/************************************************************************/
/*                                GetExtent()                           */
/************************************************************************/

OGRErr OGRPLScenesDataV1Layer::GetExtent( OGREnvelope *psExtent, int bForce )
{
    if( m_poFilterGeom != nullptr )
    {
        m_bInFeatureCountOrGetExtent = true;
        OGRErr eErr = OGRLayer::GetExtentInternal(0, psExtent, bForce);
        m_bInFeatureCountOrGetExtent = false;
        return eErr;
    }

    psExtent->MinX = -180;
    psExtent->MinY = -90;
    psExtent->MaxX = 180;
    psExtent->MaxY = 90;
    return OGRERR_NONE;
}

/************************************************************************/
/*                              TestCapability()                        */
/************************************************************************/

int OGRPLScenesDataV1Layer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return !m_bFilterMustBeClientSideEvaluated;
    if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    return FALSE;
}
