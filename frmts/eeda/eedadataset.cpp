/******************************************************************************
 *
 * Project:  Earth Engine Data API driver
 * Purpose:  Earth Engine Data API driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017-2018, Planet Labs
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

#include "gdal_priv.h"
#include "cpl_http.h"
#include "cpl_conv.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonwriter.h"
#include "ogr_swq.h"
#include "eeda.h"

#include <algorithm>
#include <vector>
#include <map>
#include <set>
#include <limits>

extern "C" void GDALRegister_EEDA();

/************************************************************************/
/*                     CPLEscapeURLQueryParameter()                     */
/************************************************************************/

static CPLString CPLEscapeURLQueryParameter( const char *pszInput )
{
    int nLength = static_cast<int>(strlen(pszInput));

    const size_t nSizeAlloc = nLength * 4 + 1;
    char *pszOutput = static_cast<char *>( CPLMalloc( nSizeAlloc ) );
    int iOut = 0;

    for( int iIn = 0; iIn < nLength; ++iIn )
    {
        if( (pszInput[iIn] >= 'a' && pszInput[iIn] <= 'z')
            || (pszInput[iIn] >= 'A' && pszInput[iIn] <= 'Z')
            || (pszInput[iIn] >= '0' && pszInput[iIn] <= '9') )
        {
            pszOutput[iOut++] = pszInput[iIn];
        }
        else
        {
            snprintf( pszOutput+iOut, nSizeAlloc - iOut, "%%%02X",
                        static_cast<unsigned char>( pszInput[iIn] ) );
            iOut += 3;
        }
    }
    pszOutput[iOut] = '\0';

    CPLString osRet(pszOutput);
    CPLFree(pszOutput);
    return osRet;
}

/************************************************************************/
/*                          GDALEEDADataset                             */
/************************************************************************/

class GDALEEDALayer;

class GDALEEDADataset final: public GDALEEDABaseDataset
{
            CPL_DISALLOW_COPY_ASSIGN(GDALEEDADataset)

            GDALEEDALayer* m_poLayer;

    public:
                GDALEEDADataset();
                virtual ~GDALEEDADataset();

                virtual int GetLayerCount() CPL_OVERRIDE { return m_poLayer ? 1 : 0; }
                virtual OGRLayer* GetLayer(int idx) CPL_OVERRIDE;

                bool Open(GDALOpenInfo* poOpenInfo);
                json_object* RunRequest(const CPLString& osURL);
                const CPLString& GetBaseURL() const { return m_osBaseURL; }
};

/************************************************************************/
/*                          GDALEEDALayer                               */
/************************************************************************/

class GDALEEDALayer final: public OGRLayer
{
                CPL_DISALLOW_COPY_ASSIGN(GDALEEDALayer)

                GDALEEDADataset* m_poDS;
                CPLString        m_osCollection{};
                CPLString        m_osCollectionName{};
                OGRFeatureDefn*  m_poFeatureDefn;
                json_object*     m_poCurPageObj;
                json_object*     m_poCurPageAssets;
                int              m_nIndexInPage;
                GIntBig          m_nFID;
                CPLString        m_osAttributeFilter{};
                CPLString        m_osStartTime{};
                CPLString        m_osEndTime{};
                bool             m_bFilterMustBeClientSideEvaluated;
                std::set<int>    m_oSetQueryableFields{};
                std::map<CPLString, CPLString> m_oMapCodeToWKT{};

                OGRFeature*      GetNextRawFeature();
                bool             IsSimpleComparison(const swq_expr_node* poNode);
                CPLString        BuildFilter(swq_expr_node* poNode, bool bIsAndTopLevel);

    public:
        GDALEEDALayer(GDALEEDADataset* poDS,
                      const CPLString& osCollection,
                      const CPLString& osCollectionName,
                      json_object* poAsset,
                      json_object* poLayerConf);
        virtual ~GDALEEDALayer();

        virtual void ResetReading() CPL_OVERRIDE;
        virtual OGRFeature* GetNextFeature() CPL_OVERRIDE;
        virtual int TestCapability(const char*) CPL_OVERRIDE;
        virtual OGRFeatureDefn* GetLayerDefn() CPL_OVERRIDE
                                        { return m_poFeatureDefn; }
        virtual GIntBig GetFeatureCount(int) CPL_OVERRIDE
                                        { return -1; }

        virtual void        SetSpatialFilter( OGRGeometry *poGeom ) CPL_OVERRIDE;
        virtual void        SetSpatialFilter( int iGeomField, OGRGeometry *poGeom ) CPL_OVERRIDE
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }

        virtual OGRErr      SetAttributeFilter( const char * ) CPL_OVERRIDE;

        virtual OGRErr      GetExtent( OGREnvelope *psExtent, int bForce ) CPL_OVERRIDE;
        virtual OGRErr      GetExtent(int iGeomField, OGREnvelope *psExtent, int bForce) CPL_OVERRIDE
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

};

/************************************************************************/
/*                            GDALEEDALayer()                           */
/************************************************************************/

GDALEEDALayer::GDALEEDALayer(GDALEEDADataset* poDS,
                             const CPLString& osCollection,
                             const CPLString& osCollectionName,
                             json_object* poAsset,
                             json_object* poLayerConf) :
    m_poDS(poDS),
    m_osCollection(osCollection),
    m_osCollectionName(osCollectionName),
    m_poFeatureDefn(nullptr),
    m_poCurPageObj(nullptr),
    m_poCurPageAssets(nullptr),
    m_nIndexInPage(0),
    m_nFID(1),
    m_bFilterMustBeClientSideEvaluated(true)
{
    CPLString osLaundered(osCollection);
    for( size_t i = 0; i < osLaundered.size(); i++ )
    {
        if( !isalnum(static_cast<int>(osLaundered[i])) )
        {
            osLaundered[i] = '_';
        }
    }
    SetDescription(osLaundered);
    m_poFeatureDefn = new OGRFeatureDefn(osLaundered);
    m_poFeatureDefn->Reference();
    m_poFeatureDefn->SetGeomType(wkbMultiPolygon);
    OGRSpatialReference* poSRS = new OGRSpatialReference();
    poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poSRS->Release();

    {
        OGRFieldDefn oFieldDefn("name", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("id", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("gdal_dataset", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("updateTime", OFTDateTime);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("startTime", OFTDateTime);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("endTime", OFTDateTime);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("sizeBytes", OFTInteger64);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("band_count", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("band_max_width", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("band_max_height", OFTInteger);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("band_min_pixel_size", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("band_upper_left_x", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("band_upper_left_y", OFTReal);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("band_crs", OFTString);
        m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }

    if( poLayerConf )
    {
        json_object* poFields = CPL_json_object_object_get(poLayerConf, "fields");
        if( poFields == nullptr ||
            json_object_get_type(poFields) != json_type_array )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find %s.fields object in eedaconf.json",
                    GetDescription());
            return;
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
                    if( EQUAL(pszType, "datetime") )
                        eType = OFTDateTime;
                    else if( EQUAL(pszType, "double") )
                        eType = OFTReal;
                    else if( EQUAL(pszType, "int") )
                        eType = OFTInteger;
                    else if( EQUAL(pszType, "int64") )
                        eType = OFTInteger64;
                    else if( EQUAL(pszType, "string") )
                        eType = OFTString;
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                "Unrecognized field type %s for field %s",
                                pszType, pszName);
                    }
                    OGRFieldDefn oFieldDefn(pszName, eType);
                    m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
                    m_oSetQueryableFields.insert( m_poFeatureDefn->GetFieldCount() - 1 );
                }
            }
        }

        json_object* poAddOtherProp = CPL_json_object_object_get(
            poLayerConf, "add_other_properties_field");
        if( json_object_get_boolean(poAddOtherProp) )
        {
            OGRFieldDefn oFieldDefn("other_properties", OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
    }
    else
    {
        json_object* poProperties = CPL_json_object_object_get(poAsset,
                                                            "properties");
        if( poProperties != nullptr &&
            json_object_get_type(poProperties) == json_type_object )
        {
            json_object_iter it;
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
            json_object_object_foreachC(poProperties, it)
            {
                OGRFieldType eType(OFTString);
                if( it.val )
                {
                    if( json_object_get_type(it.val) == json_type_int )
                    {
                        if( strstr(it.key, "PERCENTAGE") )
                            eType = OFTReal;
                        else if( CPLAtoGIntBig(json_object_get_string(it.val)) > INT_MAX )
                            eType = OFTInteger64;
                        else
                            eType = OFTInteger;
                    }
                    else if( json_object_get_type(it.val) == json_type_double )
                    {
                        eType = OFTReal;
                    }
                }
                OGRFieldDefn oFieldDefn(it.key, eType);
                m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
                m_oSetQueryableFields.insert( m_poFeatureDefn->GetFieldCount() - 1 );
            }
        }
        {
            OGRFieldDefn oFieldDefn("other_properties", OFTString);
            m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
    }
}


/************************************************************************/
/*                           ~GDALEEDALayer()                           */
/************************************************************************/

GDALEEDALayer::~GDALEEDALayer()
{
    m_poFeatureDefn->Release();
    if( m_poCurPageObj )
        json_object_put(m_poCurPageObj);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void GDALEEDALayer::ResetReading()
{
    if( m_poCurPageObj )
        json_object_put(m_poCurPageObj);
    m_poCurPageObj = nullptr;
    m_poCurPageAssets = nullptr;
    m_nIndexInPage = 0;
    m_nFID = 1  ;
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature* GDALEEDALayer::GetNextRawFeature()
{
    CPLString osNextPageToken;
    if( m_poCurPageAssets != nullptr &&
        m_nIndexInPage >= static_cast<int>(json_object_array_length(m_poCurPageAssets)) )
    {
        json_object* poToken =
            CPL_json_object_object_get(m_poCurPageObj, "nextPageToken");
        const char* pszToken = json_object_get_string(poToken);
        if( pszToken == nullptr )
            return nullptr;
        osNextPageToken = pszToken;
        json_object_put(m_poCurPageObj);
        m_poCurPageObj = nullptr;
        m_poCurPageAssets = nullptr;
        m_nIndexInPage = 0;
    }

    if( m_poCurPageObj == nullptr )
    {
        CPLString osURL(m_poDS->GetBaseURL() + m_osCollectionName +
                    ":listImages");
        CPLString query = "";
        if( !osNextPageToken.empty() )
        {
            query += "&pageToken=" +
                        CPLEscapeURLQueryParameter(osNextPageToken);
        }
        const char *pszPageSize = CPLGetConfigOption("EEDA_PAGE_SIZE", nullptr);
        if( pszPageSize )
        {
            query += "&pageSize=";
            query += pszPageSize;
        }
        if( m_poFilterGeom != nullptr )
        {
            char* pszGeoJSON = OGR_G_ExportToJson(
                reinterpret_cast<OGRGeometryH>(m_poFilterGeom) );
            query += "&region=";
            query += CPLEscapeURLQueryParameter(pszGeoJSON);
            CPLFree(pszGeoJSON);
        }
        if( !m_osAttributeFilter.empty() )
        {
            query += "&filter=";
            query += CPLEscapeURLQueryParameter(m_osAttributeFilter);
        }
        if( !m_osStartTime.empty() )
        {
            query += "&startTime=";
            query += CPLEscapeURLQueryParameter(m_osStartTime);
        }
        if( !m_osEndTime.empty() )
        {
            query += "&endTime=";
            query += CPLEscapeURLQueryParameter(m_osEndTime);
        }
        if (query.size() > 0) {
          osURL = osURL + "?" + query.substr(1);
        }
        m_poCurPageObj = m_poDS->RunRequest(osURL);
        if( m_poCurPageObj == nullptr )
            return nullptr;

        m_poCurPageAssets = CPL_json_object_object_get(
                                                m_poCurPageObj, "images");
    }

    if( m_poCurPageAssets == nullptr ||
        json_object_get_type(m_poCurPageAssets) != json_type_array )
    {
        json_object_put(m_poCurPageObj);
        m_poCurPageObj = nullptr;
        return nullptr;
    }
    json_object* poAsset = json_object_array_get_idx(m_poCurPageAssets,
                                                     m_nIndexInPage);
    if( poAsset == nullptr || json_object_get_type(poAsset) != json_type_object )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Invalid asset" );
        return nullptr;
    }

    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
    poFeature->SetFID(m_nFID);

    json_object* poJSonGeom = CPL_json_object_object_get(poAsset, "geometry");
    if( poJSonGeom != nullptr &&
        json_object_get_type(poJSonGeom) == json_type_object )
    {
        const char* pszGeoJSON = json_object_get_string(poJSonGeom);
        if( strstr(pszGeoJSON, "Infinity") == nullptr )
        {
            OGRGeometry* poGeom = reinterpret_cast<OGRGeometry*>(
                OGR_G_CreateGeometryFromJson(pszGeoJSON));
            if( poGeom != nullptr )
            {
                if( poGeom->getGeometryType() == wkbPolygon )
                {
                    OGRMultiPolygon* poMP = new OGRMultiPolygon();
                    poMP->addGeometryDirectly(poGeom);
                    poGeom = poMP;
                }
                poGeom->assignSpatialReference(
                        m_poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef());
                poFeature->SetGeometryDirectly(poGeom);
            }
        }
    }

    const char* pszName =
        json_object_get_string(CPL_json_object_object_get(poAsset, "name"));
    if( pszName )
    {
        poFeature->SetField("name", pszName);
        poFeature->SetField("gdal_dataset",
                    ("EEDAI:" + CPLString(pszName)).c_str());
    }

    const char* pszId =
        json_object_get_string(CPL_json_object_object_get(poAsset, "id"));
    if ( pszId )
    {
        poFeature->SetField("id", pszId);
    }

    const char* const apszBaseProps[] =
        { "updateTime", "startTime", "endTime", "sizeBytes" };
    for( size_t i = 0; i < CPL_ARRAYSIZE(apszBaseProps); i++ )
    {
        const char* pszVal = json_object_get_string(
            CPL_json_object_object_get(poAsset, apszBaseProps[i]));
        if( pszVal )
        {
            poFeature->SetField(apszBaseProps[i], pszVal);
        }
    }

    json_object* poBands = CPL_json_object_object_get(poAsset, "bands");
    if( poBands != nullptr &&
        json_object_get_type(poBands) == json_type_array )
    {
        std::vector<EEDAIBandDesc> aoBands = BuildBandDescArray(poBands,
                                                            m_oMapCodeToWKT);
        poFeature->SetField( "band_count", static_cast<int>(aoBands.size()));
        if( !aoBands.empty() )
        {
            int nWidth = 0, nHeight = 0;
            double dfMinPixelSize = std::numeric_limits<double>::max();
            CPLString osSRS(aoBands[0].osWKT);
            double dfULX = aoBands[0].adfGeoTransform[0];
            double dfULY = aoBands[0].adfGeoTransform[3];
            bool bULValid = true;
            for(size_t i = 0; i< aoBands.size(); i++)
            {
                nWidth = std::max(nWidth, aoBands[i].nWidth);
                nHeight = std::max(nHeight, aoBands[i].nHeight);
                dfMinPixelSize = std::min(dfMinPixelSize,
                                    std::min(aoBands[i].adfGeoTransform[1],
                                             fabs(aoBands[i].adfGeoTransform[5])));
                if( osSRS != aoBands[i].osWKT )
                {
                    osSRS.clear();
                }
                if( dfULX != aoBands[i].adfGeoTransform[0] ||
                    dfULY != aoBands[i].adfGeoTransform[3] )
                {
                    bULValid = false;
                }
            }
            poFeature->SetField( "band_max_width", nWidth );
            poFeature->SetField( "band_max_height", nHeight );
            poFeature->SetField( "band_min_pixel_size", dfMinPixelSize );
            if( bULValid )
            {
                poFeature->SetField( "band_upper_left_x", dfULX );
                poFeature->SetField( "band_upper_left_y", dfULY );
            }
            if( !osSRS.empty() )
            {
                OGRSpatialReference oSRS;
                oSRS.SetFromUserInput(osSRS, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get());
                const char* pszAuthName = oSRS.GetAuthorityName(nullptr);
                const char* pszAuthCode = oSRS.GetAuthorityCode(nullptr);
                if( pszAuthName && pszAuthCode )
                {
                    poFeature->SetField( "band_crs",
                        CPLSPrintf("%s:%s", pszAuthName, pszAuthCode) );
                }
                else
                {
                    poFeature->SetField( "band_crs", osSRS.c_str() );
                }
            }
        }
    }

    json_object* poProperties = CPL_json_object_object_get(poAsset,
                                                           "properties");
    if( poProperties != nullptr &&
        json_object_get_type(poProperties) == json_type_object )
    {
        json_object* poOtherProperties = nullptr;

        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC(poProperties, it)
        {
            if( it.val )
            {
                int nIdx = m_poFeatureDefn->GetFieldIndex(it.key);
                if( nIdx >= 0 )
                {
                    poFeature->SetField(nIdx, json_object_get_string(it.val));
                }
                else
                {
                    if( poOtherProperties == nullptr )
                        poOtherProperties = json_object_new_object();
                    json_object_object_add(poOtherProperties,
                                           it.key, it.val);
                    json_object_get(it.val);
                }
            }
        }

        if( poOtherProperties )
        {
            int nIdxOtherProperties =
                m_poFeatureDefn->GetFieldIndex("other_properties");
            if( nIdxOtherProperties >= 0 )
            {
                poFeature->SetField(nIdxOtherProperties,
                                json_object_get_string(poOtherProperties));
            }
            json_object_put(poOtherProperties);
        }
    }

    m_nFID ++;
    m_nIndexInPage ++;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *GDALEEDALayer::GetNextFeature()
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
/*                      GDALEEDALayerParseDateTime()                    */
/************************************************************************/

static int GDALEEDALayerParseDateTime(const char* pszValue,
                                       int operation,
                                       int& nYear, int &nMonth, int &nDay,
                                       int& nHour, int &nMinute, int &nSecond)
{
    nHour = (operation == SWQ_GE) ? 0 : 23;
    nMinute = (operation == SWQ_GE) ? 0 : 59;
    nSecond = (operation == SWQ_GE) ? 0 : 59;
    int nRet = sscanf(pszValue,"%04d/%02d/%02d %02d:%02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond);
    if( nRet >= 3 )
    {
        return nRet;
    }
    nRet = sscanf(pszValue,"%04d-%02d-%02dT%02d:%02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond);
    if( nRet >= 3 )
    {
        return nRet;
    }
    return 0;
}

/************************************************************************/
/*                          IsSimpleComparison()                        */
/************************************************************************/

bool GDALEEDALayer::IsSimpleComparison(const swq_expr_node* poNode)
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
             m_oSetQueryableFields.find(
                                    poNode->papoSubExpr[0]->field_index) !=
                                m_oSetQueryableFields.end();
}

/************************************************************************/
/*                             GetOperatorText()                        */
/************************************************************************/

static const char* GetOperatorText(swq_op nOp)
{
    if( nOp == SWQ_LT )
        return "<";
    if( nOp == SWQ_LE )
        return "<=";
    if( nOp == SWQ_GT )
        return ">";
    if( nOp == SWQ_GE )
        return ">=";
    if( nOp == SWQ_EQ )
        return "=";
    if( nOp == SWQ_NE )
        return "!=";
    CPLAssert(false);
    return "";
}

/************************************************************************/
/*                             BuildFilter()                            */
/************************************************************************/

CPLString GDALEEDALayer::BuildFilter(swq_expr_node* poNode, bool bIsAndTopLevel)
{
    int nYear, nMonth, nDay, nHour, nMinute, nSecond;

    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
    {
         // For AND, we can deal with a failure in one of the branch
        // since client-side will do that extra filtering
        CPLString osLeft = BuildFilter(poNode->papoSubExpr[0], bIsAndTopLevel);
        CPLString osRight = BuildFilter(poNode->papoSubExpr[1], bIsAndTopLevel);
        if( !osLeft.empty() && !osRight.empty() )
        {
            return "(" + osLeft + " AND " + osRight + ")";
        }
        else if( !osLeft.empty() )
            return osLeft;
        else
            return osRight;
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_OR && poNode->nSubExprCount == 2 )
    {
         // For OR, we need both members to be valid
        CPLString osLeft = BuildFilter(poNode->papoSubExpr[0], false);
        CPLString osRight = BuildFilter(poNode->papoSubExpr[1], false);
        if( !osLeft.empty() && !osRight.empty() )
        {
            return "(" + osLeft + " OR " + osRight + ")";
        }
        return "";
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 )
    {
        CPLString osFilter = BuildFilter(poNode->papoSubExpr[0], false);
        if( !osFilter.empty() )
        {
            return "(NOT " + osFilter + ")";
        }
        return "";
    }
    else if( IsSimpleComparison(poNode) )
    {
        const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
        CPLString osFilter(
            m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetNameRef());
        osFilter += " ";
        osFilter += GetOperatorText(poNode->nOperation);
        osFilter += " ";
        if( poNode->papoSubExpr[1]->field_type == SWQ_INTEGER ||
            poNode->papoSubExpr[1]->field_type == SWQ_INTEGER64 )
        {
            osFilter += CPLSPrintf(CPL_FRMT_GIB, poNode->papoSubExpr[1]->int_value);
        }
        else if( poNode->papoSubExpr[1]->field_type == SWQ_FLOAT )
        {
            osFilter += CPLSPrintf("%.18g", poNode->papoSubExpr[1]->float_value);
        }
        else
        {
            osFilter += "\"";
            osFilter += poNode->papoSubExpr[1]->string_value;
            osFilter += "\"";
        }
        return osFilter;
    }
    else if( bIsAndTopLevel &&
             poNode->eNodeType == SNT_OPERATION &&
             (poNode->nOperation == SWQ_EQ ||
              poNode->nOperation == SWQ_GE) &&
             poNode->nSubExprCount == 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
             poNode->papoSubExpr[0]->field_index ==
                    m_poFeatureDefn->GetFieldIndex("startTime") &&
             poNode->papoSubExpr[1]->field_type == SWQ_TIMESTAMP )
    {
        int nTerms = GDALEEDALayerParseDateTime(poNode->papoSubExpr[1]->string_value,
                SWQ_GE,
                nYear, nMonth, nDay, nHour, nMinute, nSecond);
        if( nTerms >= 3 )
        {
            m_osStartTime = CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
                                nYear, nMonth, nDay, nHour, nMinute, nSecond);
        }
        else
        {
            m_bFilterMustBeClientSideEvaluated = true;
        }
        return "";
    }
    else if( bIsAndTopLevel &&
             poNode->eNodeType == SNT_OPERATION &&
             (poNode->nOperation == SWQ_EQ ||
              poNode->nOperation == SWQ_LE) &&
             poNode->nSubExprCount == 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
             poNode->papoSubExpr[0]->field_index ==
                    m_poFeatureDefn->GetFieldIndex("endTime") &&
             poNode->papoSubExpr[1]->field_type == SWQ_TIMESTAMP )
    {
        int nTerms = GDALEEDALayerParseDateTime(poNode->papoSubExpr[1]->string_value,
                SWQ_LE,
                nYear, nMonth, nDay, nHour, nMinute, nSecond);
        if( nTerms >= 3 )
        {
            if( poNode->nOperation == SWQ_EQ && nTerms == 6 )
            {
                if( nSecond < 59 )
                    nSecond ++;
                else if( nMinute < 59 )
                    nMinute ++;
                else if( nHour < 23 )
                    nHour ++;
                else
                    nDay ++;
            }
            m_osEndTime = CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ",
                                nYear, nMonth, nDay, nHour, nMinute, nSecond);
        }
        else
        {
            m_bFilterMustBeClientSideEvaluated = true;
        }
        return "";
    }
    else if ( poNode->eNodeType == SNT_OPERATION &&
              poNode->nOperation == SWQ_IN &&
              poNode->nSubExprCount >= 2 &&
              poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
              m_oSetQueryableFields.find(
                                    poNode->papoSubExpr[0]->field_index) !=
                                m_oSetQueryableFields.end() )
    {
        const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
        CPLString osFilter;

        for( int i=1; i<poNode->nSubExprCount;i++)
        {
            if( !osFilter.empty() )
                osFilter += " OR ";
            osFilter +=
                m_poFeatureDefn->GetFieldDefn(nFieldIdx)->GetNameRef();
            osFilter += " = ";
            if( poNode->papoSubExpr[i]->field_type == SWQ_INTEGER ||
                poNode->papoSubExpr[i]->field_type == SWQ_INTEGER64 )
            {
                osFilter += CPLSPrintf(CPL_FRMT_GIB, poNode->papoSubExpr[i]->int_value);
            }
            else if( poNode->papoSubExpr[i]->field_type == SWQ_FLOAT )
            {
                osFilter += CPLSPrintf("%.18g", poNode->papoSubExpr[i]->float_value);
            }
            else
            {
                osFilter += "\"";
                osFilter += poNode->papoSubExpr[i]->string_value;
                osFilter += "\"";
            }
        }

        return osFilter;
    }

    m_bFilterMustBeClientSideEvaluated = true;
    return "";
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr GDALEEDALayer::SetAttributeFilter( const char *pszQuery )

{
    m_osAttributeFilter.clear();
    m_osStartTime.clear();
    m_osEndTime.clear();
    m_bFilterMustBeClientSideEvaluated = false;

    if( pszQuery && STARTS_WITH_CI(pszQuery, "EEDA:") )
    {
        m_osAttributeFilter = pszQuery + strlen("EEDA:");
        OGRLayer::SetAttributeFilter(nullptr);
        ResetReading();
        return OGRERR_NONE;
    }

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszQuery);

    if( m_poAttrQuery != nullptr )
    {
        swq_expr_node* poNode = static_cast<swq_expr_node*>(m_poAttrQuery->GetSWQExpr());

#ifndef PLUGIN
        poNode->ReplaceBetweenByGEAndLERecurse();
#endif

        m_osAttributeFilter = BuildFilter(poNode, true);
        if( m_osAttributeFilter.empty() && m_osStartTime.empty() &&
            m_osEndTime.empty() )
        {
            CPLDebug("EEDA",
                        "Full filter will be evaluated on client side.");
        }
        else if( m_bFilterMustBeClientSideEvaluated )
        {
            CPLDebug("EEDA",
                "Only part of the filter will be evaluated on server side.");
        }
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void GDALEEDALayer::SetSpatialFilter(  OGRGeometry *poGeomIn )
{
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
/*                                GetExtent()                           */
/************************************************************************/

OGRErr GDALEEDALayer::GetExtent(OGREnvelope* psExtent, int /* bForce */)
{
    psExtent->MinX = -180;
    psExtent->MinY = -90;
    psExtent->MaxX = 180;
    psExtent->MaxY = 90;
    return OGRERR_NONE;
}

/************************************************************************/
/*                              TestCapability()                        */
/************************************************************************/

int GDALEEDALayer::TestCapability(const char* pszCap)
{
   if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                         GDALEEDADataset()                           */
/************************************************************************/

GDALEEDADataset::GDALEEDADataset() :
    m_poLayer(nullptr)
{
}


/************************************************************************/
/*                        ~GDALEEDADataset()                            */
/************************************************************************/

GDALEEDADataset::~GDALEEDADataset()
{
    delete m_poLayer;
}

/************************************************************************/
/*                            GetLayer()                                */
/************************************************************************/

OGRLayer* GDALEEDADataset::GetLayer(int idx)
{
    if( idx == 0 )
        return m_poLayer;
    return nullptr;
}

/************************************************************************/
/*                            RunRequest()                              */
/************************************************************************/

json_object* GDALEEDADataset::RunRequest(const CPLString& osURL)
{
    char** papszOptions = GetBaseHTTPOptions();
    if( papszOptions == nullptr )
        return nullptr;
    CPLHTTPResult* psResult = EEDAHTTPFetch(osURL, papszOptions);
    CSLDestroy(papszOptions);
    if( psResult == nullptr )
        return nullptr;
    if( psResult->pszErrBuf != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                psResult->pabyData ? reinterpret_cast<const char*>(psResult->pabyData) :
                psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    const char* pszText = reinterpret_cast<const char*>(psResult->pabyData);
#ifdef DEBUG_VERBOSE
    CPLDebug("EEDA", "%s", pszText);
#endif

    json_object* poObj = nullptr;
    if( !OGRJSonParse(pszText, &poObj, true) )
    {
        CPLHTTPDestroyResult(psResult);
        return nullptr;
    }

    CPLHTTPDestroyResult(psResult);

    if( json_object_get_type(poObj) != json_type_object )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Return is not a JSON dictionary");
        json_object_put(poObj);
        return nullptr;
    }

    return poObj;
}

/************************************************************************/
/*                         GDALEEDADatasetGetConf()                     */
/************************************************************************/

static json_object* GDALEEDADatasetGetConf()
{
    const char* pszConfFile = CPLFindFile("gdal", "eedaconf.json");
    if( pszConfFile == nullptr )
    {
        CPLDebug("EEDA", "Cannot find eedaconf.json");
        return nullptr;
    }

    GByte* pabyRet = nullptr;
    if( !VSIIngestFile( nullptr, pszConfFile, &pabyRet, nullptr, -1 ) )
    {
        return nullptr;
    }

    json_object* poRoot = nullptr;
    const char* pzText = reinterpret_cast<char*>(pabyRet);
    if( !OGRJSonParse( pzText, &poRoot ) )
    {
        VSIFree(pabyRet);
        return nullptr;
    }
    VSIFree(pabyRet);

    if( json_object_get_type(poRoot) != json_type_object )
    {
        json_object_put(poRoot);
        return nullptr;
    }

    return poRoot;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool GDALEEDADataset::Open(GDALOpenInfo* poOpenInfo)
{
    m_osBaseURL = CPLGetConfigOption("EEDA_URL",
                            "https://earthengine-highvolume.googleapis.com/v1alpha/");

    CPLString osCollection =
            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "COLLECTION", "");
    if( osCollection.empty() )
    {
        char** papszTokens =
                CSLTokenizeString2(poOpenInfo->pszFilename, ":", 0);
        if( CSLCount(papszTokens) < 2 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No collection specified in connection string or "
                     "COLLECTION open option");
            CSLDestroy(papszTokens);
            return false;
        }
        osCollection = papszTokens[1];
        CSLDestroy(papszTokens);
    }
    CPLString osCollectionName = ConvertPathToName(osCollection);

    json_object* poRootConf = GDALEEDADatasetGetConf();
    if( poRootConf )
    {
        json_object* poLayerConf =
            CPL_json_object_object_get(poRootConf, osCollection);
        if( poLayerConf != nullptr &&
            json_object_get_type(poLayerConf) == json_type_object )
        {
            m_poLayer = new GDALEEDALayer(this, osCollection, osCollectionName,
                                          nullptr, poLayerConf);
            json_object_put(poRootConf);
            return true;
        }
        json_object_put(poRootConf);
    }

    // Issue request to get layer schema
    json_object* poRootAsset = RunRequest(m_osBaseURL + osCollectionName +
                ":listImages?pageSize=1");
    if( poRootAsset == nullptr )
        return false;

    json_object* poAssets = CPL_json_object_object_get(poRootAsset, "images");
    if( poAssets == nullptr ||
        json_object_get_type(poAssets) != json_type_array ||
        json_object_array_length(poAssets) != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No assets" );
        json_object_put(poRootAsset);
        return false;
    }
    json_object* poAsset = json_object_array_get_idx(poAssets, 0);
    if( poAsset == nullptr || json_object_get_type(poAsset) != json_type_object )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No assets" );
        json_object_put(poRootAsset);
        return false;
    }

    m_poLayer = new GDALEEDALayer(this, osCollection, osCollectionName, poAsset,
                                  nullptr);
    json_object_put(poRootAsset);

    return true;
}

/************************************************************************/
/*                          GDALEEDAdentify()                          */
/************************************************************************/

static int GDALEEDAdentify(GDALOpenInfo* poOpenInfo)
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "EEDA:");
}


/************************************************************************/
/*                            GDALEEDAOpen()                            */
/************************************************************************/

static GDALDataset* GDALEEDAOpen(GDALOpenInfo* poOpenInfo)
{
    if(! GDALEEDAdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return nullptr;

    GDALEEDADataset* poDS = new GDALEEDADataset();
    if( !poDS->Open(poOpenInfo) )
    {
        delete poDS;
        return nullptr;
    }
    return poDS;
}

/************************************************************************/
/*                         GDALRegister_EEDA()                          */
/************************************************************************/

void GDALRegister_EEDA()

{
    if( GDALGetDriverByName( "EEDA" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "EEDA" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Earth Engine Data API" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/eeda.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "EEDA:" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='COLLECTION' type='string' description='Collection name'/>"
"</OpenOptionList>");

    poDriver->pfnOpen = GDALEEDAOpen;
    poDriver->pfnIdentify = GDALEEDAdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
