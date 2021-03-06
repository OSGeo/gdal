/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGC API - Features (previously known as WFS3)
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2018-2019, Even Rouault <even dot rouault at spatialys dot com>
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

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "ogr_swq.h"
#include "parsexsd.h"

#include <algorithm>
#include <memory>
#include <vector>
#include <set>

// g++ -Wshadow -Wextra -std=c++11 -fPIC -g -Wall ogr/ogrsf_frmts/wfs/ogroapif*.cpp -shared -o ogr_OAPIF.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gml -Iogr/ogrsf_frmts/wfs -L. -lgdal

extern "C" void RegisterOGROAPIF();

#define MEDIA_TYPE_OAPI_3_0      "application/vnd.oai.openapi+json;version=3.0"
#define MEDIA_TYPE_OAPI_3_0_ALT  "application/openapi+json;version=3.0"
#define MEDIA_TYPE_JSON          "application/json"
#define MEDIA_TYPE_GEOJSON       "application/geo+json"
#define MEDIA_TYPE_TEXT_XML      "text/xml"
#define MEDIA_TYPE_APPLICATION_XML "application/xml"
#define MEDIA_TYPE_JSON_SCHEMA     "application/schema+json"

/************************************************************************/
/*                           OGROAPIFDataset                             */
/************************************************************************/
class OGROAPIFLayer;

class OGROAPIFDataset final: public GDALDataset
{
        friend class OGROAPIFLayer;

        bool                                   m_bMustCleanPersistent = false;
        CPLString                              m_osRootURL;
        CPLString                              m_osUserQueryParams;
        CPLString                              m_osUserPwd;
        int                                    m_nPageSize = 10;
        std::vector<std::unique_ptr<OGRLayer>> m_apoLayers;

        bool                                   m_bAPIDocLoaded = false;
        CPLJSONDocument                        m_oAPIDoc;

        bool                                   m_bLandingPageDocLoaded = false;
        CPLJSONDocument                        m_oLandingPageDoc;

        bool                                   m_bIgnoreSchema = false;

        bool                    Download(
            const CPLString& osURL,
            const char* pszAccept,
            CPLString& osResult,
            CPLString& osContentType,
            CPLStringList* paosHeaders = nullptr );

        bool                    DownloadJSon(
            const CPLString& osURL,
            CPLJSONDocument& oDoc,
            const char* pszAccept = MEDIA_TYPE_GEOJSON ", " MEDIA_TYPE_JSON,
            CPLStringList* paosHeaders = nullptr);

        bool LoadJSONCollection(const CPLJSONObject& oCollection);
        bool LoadJSONCollections(const CPLString& osResultIn);

    public:
        OGROAPIFDataset() = default;
        ~OGROAPIFDataset();

        int GetLayerCount() override
                            { return static_cast<int>(m_apoLayers.size()); }
        OGRLayer* GetLayer(int idx) override;

        bool Open(GDALOpenInfo*);
        const CPLJSONDocument& GetAPIDoc();
        const CPLJSONDocument& GetLandingPageDoc();

        CPLString ReinjectAuthInURL(const CPLString& osURL) const;
};

/************************************************************************/
/*                            OGROAPIFLayer                              */
/************************************************************************/

class OGROAPIFLayer final: public OGRLayer
{
        OGROAPIFDataset* m_poDS = nullptr;
        OGRFeatureDefn* m_poFeatureDefn = nullptr;
        bool            m_bIsGeographicCRS = false;
        CPLString       m_osURL;
        CPLString       m_osPath;
        OGREnvelope     m_oExtent;
        bool            m_bFeatureDefnEstablished = false;
        std::unique_ptr<GDALDataset> m_poUnderlyingDS;
        OGRLayer*       m_poUnderlyingLayer = nullptr;
        GIntBig         m_nFID = 1;
        CPLString       m_osGetURL;
        CPLString       m_osAttributeFilter;
        CPLString       m_osGetID;
        bool            m_bFilterMustBeClientSideEvaluated = false;
        bool            m_bGotQueryableAttributes = false;
        std::set<CPLString> m_aoSetQueryableAttributes;
        bool            m_bHasCQLText = false;
        // https://github.com/tschaub/ogcapi-features/blob/json-array-expression/extensions/cql/jfe/readme.md
        bool            m_bHasJSONFilterExpression = false;
        GIntBig         m_nTotalFeatureCount = -1;
        bool            m_bHasIntIdMember = false;
        bool            m_bHasStringIdMember = false;
        std::vector<std::unique_ptr<OGRFieldDefn>> m_apoFieldsFromSchema{};
        CPLString       m_osDescribedByURL{};
        CPLString       m_osDescribedByType{};
        bool            m_bDescribedByIsXML = false;
        CPLString       m_osQueryablesURL{};

        void            EstablishFeatureDefn();
        OGRFeature     *GetNextRawFeature();
        CPLString       AddFilters(const CPLString& osURL);
        CPLString       BuildFilter(const swq_expr_node* poNode);
        CPLString       BuildFilterCQLText(const swq_expr_node* poNode);
        CPLString       BuildFilterJSONFilterExpr(const swq_expr_node* poNode);
        bool            SupportsResultTypeHits();
        void            GetQueryableAttributes();
        void            GetSchema();

    public:
        OGROAPIFLayer(OGROAPIFDataset* poDS,
                     const CPLString& osName,
                     const CPLJSONArray& oBBOX,
                     const CPLJSONArray& oCRS,
                     const CPLJSONArray& oLinks);

       ~OGROAPIFLayer();

       const char*     GetName() override { return GetDescription(); }
       OGRFeatureDefn* GetLayerDefn() override;
       void            ResetReading() override;
       OGRFeature*     GetNextFeature() override;
       OGRFeature*     GetFeature(GIntBig) override;
       int             TestCapability(const char*) override;
       GIntBig         GetFeatureCount(int bForce = FALSE) override;
       OGRErr          GetExtent(OGREnvelope *psExtent,
                                 int bForce = TRUE) override;
       OGRErr          GetExtent(int iGeomField, OGREnvelope *psExtent,
                                 int bForce) override
                { return OGRLayer::GetExtent(iGeomField, psExtent, bForce); }

       void            SetSpatialFilter( OGRGeometry *poGeom ) override;
       void            SetSpatialFilter( int iGeomField, OGRGeometry *poGeom )
                                                                    override
                { OGRLayer::SetSpatialFilter(iGeomField, poGeom); }
       OGRErr          SetAttributeFilter( const char *pszQuery ) override;

};

/************************************************************************/
/*                          CheckContentType()                          */
/************************************************************************/

// We may ask for "application/openapi+json;version=3.0"
// and the server returns "application/openapi+json; charset=utf-8; version=3.0"
static bool CheckContentType(const char* pszGotContentType,
                             const char* pszExpectedContentType)
{
    CPLStringList aosGotTokens(CSLTokenizeString2(pszGotContentType, "; ", 0));
    CPLStringList aosExpectedTokens(CSLTokenizeString2(pszExpectedContentType, "; ", 0));
    for( int i = 0; i < aosExpectedTokens.size(); i++ )
    {
        bool bFound = false;
        for( int j = 0; j < aosGotTokens.size(); j++ )
        {
            if( EQUAL(aosExpectedTokens[i], aosGotTokens[j]) )
            {
                bFound = true;
                break;
            }
        }
        if( !bFound )
            return false;
    }
    return true;
}

/************************************************************************/
/*                         ~OGROAPIFDataset()                           */
/************************************************************************/

OGROAPIFDataset::~OGROAPIFDataset()
{
    if( m_bMustCleanPersistent )
    {
        char **papszOptions =
            CSLSetNameValue(
                nullptr, "CLOSE_PERSISTENT", CPLSPrintf("OAPIF:%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch(m_osRootURL, papszOptions));
        CSLDestroy(papszOptions);
    }
}

/************************************************************************/
/*                         ReinjectAuthInURL()                          */
/************************************************************************/

// If source URL is https://user:pwd@server.com/bla
// and link only contains https://server.com/bla, then insert
// into it user:pwd
CPLString OGROAPIFDataset::ReinjectAuthInURL(const CPLString& osURL) const
{
    CPLString osRet(osURL);

    if( !osRet.empty() && osRet[0] == '/' )
        osRet = m_osRootURL + osRet;

    const auto nArobaseInURLPos = m_osRootURL.find('@');
    if( !osRet.empty() &&
        STARTS_WITH(m_osRootURL, "https://") &&
        STARTS_WITH(osRet, "https://") &&
        nArobaseInURLPos != std::string::npos &&
        osRet.find('@') == std::string::npos )
    {
        const auto nFirstSlashPos = m_osRootURL.find('/', strlen("https://"));
        if( nFirstSlashPos == std::string::npos ||
            nFirstSlashPos > nArobaseInURLPos )
        {
            auto osUserPwd = m_osRootURL.substr(strlen("https://"),
                            nArobaseInURLPos - strlen("https://"));
            auto osServer = nFirstSlashPos == std::string::npos ?
                m_osRootURL.substr(nArobaseInURLPos + 1):
                m_osRootURL.substr(nArobaseInURLPos + 1,
                                            nFirstSlashPos - nArobaseInURLPos);
            if( STARTS_WITH(osRet, ("https://" + osServer).c_str()) )
            {
                osRet = "https://" + osUserPwd + "@" +
                        osRet.substr(strlen("https://"));
            }
        }
    }
    return osRet;
}

/************************************************************************/
/*                              Download()                              */
/************************************************************************/

bool OGROAPIFDataset::Download(
            const CPLString& osURL,
            const char* pszAccept,
            CPLString& osResult,
            CPLString& osContentType,
            CPLStringList* paosHeaders )
{
#ifndef REMOVE_HACK
    VSIStatBufL sStatBuf;
    if( VSIStatL(osURL, &sStatBuf) == 0 )
    {
        CPLDebug("OAPIF", "Reading %s", osURL.c_str());
        GByte* pabyRet = nullptr;
        if( VSIIngestFile( nullptr, osURL, &pabyRet, nullptr, -1) )
        {
            osResult = reinterpret_cast<char*>(pabyRet);
            CPLFree(pabyRet);
        }
        return false;
    }
#endif
    char** papszOptions = CSLSetNameValue(nullptr,
            "HEADERS", (CPLString("Accept: ") + pszAccept).c_str());
    if( !m_osUserPwd.empty() )
    {
        papszOptions = CSLSetNameValue(papszOptions,
                                       "USERPWD", m_osUserPwd.c_str());
    }
    m_bMustCleanPersistent = true;
    papszOptions =
        CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=OAPIF:%p", this));
    CPLString osURLWithQueryParameters(osURL);
    if( !m_osUserQueryParams.empty() &&
        osURL.find('?' + m_osUserQueryParams) == std::string::npos &&
        osURL.find('&' + m_osUserQueryParams) == std::string::npos )
    {
        if( osURL.find('?') == std::string::npos )
        {
            osURLWithQueryParameters += '?';
        }
        else
        {
            osURLWithQueryParameters += '&';
        }
        osURLWithQueryParameters += m_osUserQueryParams;
    }
    CPLHTTPResult* psResult = CPLHTTPFetch(osURLWithQueryParameters, papszOptions);
    CSLDestroy(papszOptions);
    if( !psResult )
        return false;

    if( psResult->pszErrBuf != nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                psResult->pabyData ?
                    reinterpret_cast<const char*>(psResult->pabyData) :
                psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    if( psResult->pszContentType )
        osContentType = psResult->pszContentType;
    bool bFoundExpectedContentType = false;

#ifndef REMOVE_HACK
    if( strstr(pszAccept, "json") )
    {
        if( strstr(osURL, "raw.githubusercontent.com") &&
            strstr(osURL, ".json") )
        {
            bFoundExpectedContentType = true;
        }
        else if( psResult->pszContentType != nullptr &&
            (CheckContentType(psResult->pszContentType, MEDIA_TYPE_JSON) ||
             CheckContentType(psResult->pszContentType, MEDIA_TYPE_GEOJSON)) )
        {
            bFoundExpectedContentType = true;
        }
    }
#endif

    if( strstr(pszAccept, "xml") &&
        psResult->pszContentType != nullptr &&
        (CheckContentType(psResult->pszContentType, MEDIA_TYPE_TEXT_XML) ||
         CheckContentType(psResult->pszContentType, MEDIA_TYPE_APPLICATION_XML)) )
    {
        bFoundExpectedContentType = true;
    }

    if( strstr(pszAccept, MEDIA_TYPE_JSON_SCHEMA) &&
        psResult->pszContentType != nullptr &&
        (CheckContentType(psResult->pszContentType, MEDIA_TYPE_JSON) ||
         CheckContentType(psResult->pszContentType, MEDIA_TYPE_JSON_SCHEMA)) )
    {
        bFoundExpectedContentType = true;
    }

    for( const char* pszMediaType : { MEDIA_TYPE_JSON,
                                      MEDIA_TYPE_GEOJSON,
                                      MEDIA_TYPE_OAPI_3_0,
#ifndef REMOVE_SUPPORT_FOR_OLD_VERSIONS
                                      MEDIA_TYPE_OAPI_3_0_ALT,
#endif
                                    })
    {
        if( strstr(pszAccept, pszMediaType) &&
            psResult->pszContentType != nullptr &&
            CheckContentType(psResult->pszContentType, pszMediaType) )
        {
            bFoundExpectedContentType = true;
            break;
        }
    }

    if( !bFoundExpectedContentType )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Unexpected Content-Type: %s",
                psResult->pszContentType ?
                    psResult->pszContentType : "(null)" );
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    if( psResult->pabyData == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    if( paosHeaders )
    {
        *paosHeaders = CSLDuplicate(psResult->papszHeaders);
    }

    osResult = reinterpret_cast<const char*>(psResult->pabyData);
    CPLHTTPDestroyResult(psResult);
    return true;
}


/************************************************************************/
/*                           DownloadJSon()                             */
/************************************************************************/

bool OGROAPIFDataset::DownloadJSon(const CPLString& osURL,
                                  CPLJSONDocument& oDoc,
                                  const char* pszAccept,
                                  CPLStringList* paosHeaders)
{
    CPLString osResult;
    CPLString osContentType;
    if( !Download(osURL, pszAccept, osResult, osContentType, paosHeaders) )
        return false;
    return oDoc.LoadMemory( osResult );
}

/************************************************************************/
/*                        GetLandingPageDoc()                           */
/************************************************************************/

const CPLJSONDocument& OGROAPIFDataset::GetLandingPageDoc()
{
    if( m_bLandingPageDocLoaded )
        return m_oLandingPageDoc;
    m_bLandingPageDocLoaded = true;
    CPL_IGNORE_RET_VAL(
        DownloadJSon(m_osRootURL, m_oLandingPageDoc, MEDIA_TYPE_JSON));
    return m_oLandingPageDoc;
}

/************************************************************************/
/*                            GetAPIDoc()                               */
/************************************************************************/

const CPLJSONDocument& OGROAPIFDataset::GetAPIDoc()
{
    if( m_bAPIDocLoaded )
        return m_oAPIDoc;
    m_bAPIDocLoaded = true;

    // Fetch the /api URL from the links of the landing page
    CPLString osAPIURL;
    const auto& oLandingPage = GetLandingPageDoc();
    if( oLandingPage.GetRoot().IsValid() )
    {
        const auto oLinks = oLandingPage.GetRoot().GetArray("links");
        if( oLinks.IsValid() )
        {
            int nCountRelAPI = 0;
            for( int i = 0; i < oLinks.Size(); i++ )
            {
                CPLJSONObject oLink = oLinks[i];
                if( !oLink.IsValid() ||
                    oLink.GetType() != CPLJSONObject::Type::Object )
                {
                    continue;
                }
                const auto osRel(oLink.GetString("rel"));
                const auto osType(oLink.GetString("type"));
                if( osRel == "service-desc"
#ifndef REMOVE_SUPPORT_FOR_OLD_VERSIONS
                    // Needed for http://beta.fmi.fi/data/3/wfs/sofp
                    || osRel == "service"
#endif
                    )
                {
                    nCountRelAPI ++;
                    osAPIURL = ReinjectAuthInURL(oLink.GetString("href"));
                    if( osType == MEDIA_TYPE_OAPI_3_0
#ifndef REMOVE_SUPPORT_FOR_OLD_VERSIONS
                        // Needed for http://beta.fmi.fi/data/3/wfs/sofp
                        || osType == MEDIA_TYPE_OAPI_3_0_ALT
#endif
                    )
                    {
                        nCountRelAPI = 1;
                        break;
                    }
                }
            }
            if( !osAPIURL.empty() && nCountRelAPI > 1 )
            {
                osAPIURL.clear();
            }
        }
    }

    const char* pszAccept = MEDIA_TYPE_OAPI_3_0
#ifndef REMOVE_SUPPORT_FOR_OLD_VERSIONS
                            ", " MEDIA_TYPE_OAPI_3_0_ALT
                            ", " MEDIA_TYPE_JSON
#endif
                            ;

    if( !osAPIURL.empty() )
    {
        CPL_IGNORE_RET_VAL(DownloadJSon(osAPIURL, m_oAPIDoc, pszAccept));
        return m_oAPIDoc;
    }

#ifndef REMOVE_HACK
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLString osURL(m_osRootURL + "/api");
    osURL = CPLGetConfigOption("OGR_WFS3_API_URL", osURL.c_str());
    bool bOK = DownloadJSon(osURL, m_oAPIDoc, pszAccept);
    CPLPopErrorHandler();
    CPLErrorReset();
    if( bOK )
    {
        return m_oAPIDoc;
    }

    if( DownloadJSon(m_osRootURL + "/api/", m_oAPIDoc, pszAccept) )
    {
        return m_oAPIDoc;
    }
#endif
    return m_oAPIDoc;
}

/************************************************************************/
/*                         LoadJSONCollection()                         */
/************************************************************************/

bool OGROAPIFDataset::LoadJSONCollection(const CPLJSONObject& oCollection)
{
    if( oCollection.GetType() != CPLJSONObject::Type::Object )
        return false;

    // As used by https://maps.ecere.com/ogcapi/collections?f=json
    const auto osLayerDataType = oCollection.GetString("layerDataType");
    if( osLayerDataType == "Raster" || osLayerDataType == "Coverage" )
        return false;

    CPLString osName( oCollection.GetString("id") );
#ifndef REMOVE_SUPPORT_FOR_OLD_VERSIONS
    if( osName.empty() )
        osName = oCollection.GetString("name");
    if( osName.empty() )
        osName = oCollection.GetString("collectionId");
#endif
    if( osName.empty() )
        return false;

    CPLString osTitle( oCollection.GetString("title") );
    CPLString osDescription( oCollection.GetString("description") );
    CPLJSONArray oBBOX = oCollection.GetArray("extent/spatial/bbox");
#ifndef REMOVE_SUPPORT_FOR_OLD_VERSIONS
    if( !oBBOX.IsValid() )
        oBBOX = oCollection.GetArray("extent/spatial");
#endif
    CPLJSONArray oCRS = oCollection.GetArray("crs");
    const auto oLinks = oCollection.GetArray("links");
    std::unique_ptr<OGROAPIFLayer> poLayer( new
        OGROAPIFLayer(this, osName, oBBOX, oCRS, oLinks) );
    if( !osTitle.empty() )
        poLayer->SetMetadataItem("TITLE", osTitle.c_str());
    if( !osDescription.empty() )
        poLayer->SetMetadataItem("DESCRIPTION", osDescription.c_str());
    auto oTemporalInterval = oCollection.GetArray("extent/temporal/interval");
    if( oTemporalInterval.IsValid() && oTemporalInterval.Size() == 1 &&
        oTemporalInterval[0].GetType() == CPLJSONObject::Type::Array )
    {
        auto oArray = oTemporalInterval[0].ToArray();
        if( oArray.Size() == 2 )
        {
            if( oArray[0].GetType() == CPLJSONObject::Type::String )
            {
                poLayer->SetMetadataItem("TEMPORAL_INTERVAL_MIN",
                                         oArray[0].ToString().c_str() );
            }
            if( oArray[1].GetType() == CPLJSONObject::Type::String )
            {
                poLayer->SetMetadataItem("TEMPORAL_INTERVAL_MAX",
                                         oArray[1].ToString().c_str() );
            }
        }
    }

    auto oJSONStr = oCollection.Format(CPLJSONObject::PrettyFormat::Pretty);
    char* apszMetadata[2] = { &oJSONStr[0], nullptr };
    poLayer->SetMetadata(apszMetadata, "json:metadata");

    m_apoLayers.emplace_back(std::move(poLayer));
    return true;
}

/************************************************************************/
/*                         LoadJSONCollections()                        */
/************************************************************************/

bool OGROAPIFDataset::LoadJSONCollections(const CPLString& osResultIn)
{
    CPLString osResult(osResultIn);
    while(!osResult.empty())
    {
        CPLJSONDocument oDoc;
        if( !oDoc.LoadMemory(osResult) )
        {
            return false;
        }
        const auto& oRoot = oDoc.GetRoot();
        CPLJSONArray oCollections = oRoot.GetArray("collections");
        if( !oCollections.IsValid() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "No collections array");
            return false;
        }

        for( int i = 0; i < oCollections.Size(); i++ )
        {
            LoadJSONCollection(oCollections[i]);
        }

        osResult.clear();

        // Paging is a (unspecified) extension to the core used by
        // https://{api_key}:@api.planet.com/analytics
        const auto oLinks = oRoot.GetArray("links");
        if( oLinks.IsValid() )
        {
            CPLString osNextURL;
            int nCountRelNext = 0;
            for( int i = 0; i < oLinks.Size(); i++ )
            {
                CPLJSONObject oLink = oLinks[i];
                if( !oLink.IsValid() ||
                    oLink.GetType() != CPLJSONObject::Type::Object )
                {
                    continue;
                }
                if( oLink.GetString("rel") == "next" )
                {
                    osNextURL = oLink.GetString("href");
                    nCountRelNext ++;
                    auto type = oLink.GetString("type");
                    if (type == MEDIA_TYPE_GEOJSON ||
                        type == MEDIA_TYPE_JSON )
                    {
                        nCountRelNext = 1;
                        break;
                    }
                }
            }
            if( nCountRelNext == 1 && !osNextURL.empty() )
            {
                CPLString osContentType;
                osNextURL = ReinjectAuthInURL(osNextURL);
                if( !Download(osNextURL, MEDIA_TYPE_JSON,
                              osResult, osContentType) )
                {
                    return false;
                }
            }
        }
    }
    return !m_apoLayers.empty();
}

/************************************************************************/
/*                         ConcatenateURLParts()                        */
/************************************************************************/

static std::string ConcatenateURLParts(const std::string& osPart1,
                                       const std::string& osPart2)
{
    if( !osPart1.empty() && osPart1.back() == '/' &&
        !osPart2.empty() && osPart2.front() == '/' )
    {
        return osPart1.substr(0, osPart1.size() - 1) + osPart2;
    }
    return osPart1 + osPart2;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

bool OGROAPIFDataset::Open(GDALOpenInfo* poOpenInfo)
{
    CPLString osCollectionDescURL;

    m_osRootURL =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "URL",
            poOpenInfo->pszFilename);
    if( STARTS_WITH_CI(m_osRootURL, "WFS3:") )
        m_osRootURL = m_osRootURL.substr(strlen("WFS3:"));
    else if( STARTS_WITH_CI(m_osRootURL, "OAPIF:") )
        m_osRootURL = m_osRootURL.substr(strlen("OAPIF:"));
    else if( STARTS_WITH_CI(m_osRootURL, "OAPIF_COLLECTION:") )
    {
        osCollectionDescURL = m_osRootURL.substr(strlen("OAPIF_COLLECTION:"));
        m_osRootURL = osCollectionDescURL;
        const char* pszStr = m_osRootURL.c_str();
        const char* pszPtr = pszStr;
        if( STARTS_WITH(pszPtr, "http://") )
            pszPtr += strlen("http://");
        else if( STARTS_WITH(pszPtr, "https://") )
            pszPtr += strlen("https://");
        pszPtr = strchr(pszPtr, '/');
        if( pszPtr )
            m_osRootURL.assign(pszStr, pszPtr - pszStr);
    }

    if( osCollectionDescURL.empty() )
    {
        auto nPosQuotationMark = m_osRootURL.find('?');
        if( nPosQuotationMark != std::string::npos )
        {
            m_osUserQueryParams = m_osRootURL.substr(nPosQuotationMark + 1);
            m_osRootURL.resize(nPosQuotationMark);
        }

        auto nCollectionsPos = m_osRootURL.find("/collections/");
        if( nCollectionsPos != std::string::npos )
        {
            osCollectionDescURL = m_osRootURL;
            m_osRootURL.resize(nCollectionsPos);
        }
    }

    m_bIgnoreSchema = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "IGNORE_SCHEMA",
                             "FALSE"));
    m_nPageSize = atoi( CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                            "PAGE_SIZE",CPLSPrintf("%d", m_nPageSize)) );
    m_osUserPwd =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "USERPWD", "");
    CPLString osResult;
    CPLString osContentType;

    if( !osCollectionDescURL.empty() )
    {
        if( !Download(osCollectionDescURL, MEDIA_TYPE_JSON,
                      osResult, osContentType) )
        {
            return false;
        }
        CPLJSONDocument oDoc;
        if( !oDoc.LoadMemory(osResult) )
        {
            return false;
        }
        const auto& oRoot = oDoc.GetRoot();
        return LoadJSONCollection(oRoot);
    }

    if( !Download(ConcatenateURLParts(m_osRootURL, "/collections"),
            MEDIA_TYPE_JSON, osResult, osContentType) )
    {
        return false;
    }

    if( osContentType.find("json") != std::string::npos )
    {
        return LoadJSONCollections(osResult);
    }

    return true;
}

/************************************************************************/
/*                             GetLayer()                               */
/************************************************************************/

OGRLayer* OGROAPIFDataset::GetLayer(int nIndex)
{
    if( nIndex < 0 || nIndex >= GetLayerCount() )
        return nullptr;
    return m_apoLayers[nIndex].get();
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

static int OGROAPIFDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "WFS3:") ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, "OAPIF:") ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, "OAPIF_COLLECTION:");
}

/************************************************************************/
/*                           OGROAPIFLayer()                             */
/************************************************************************/

OGROAPIFLayer::OGROAPIFLayer(OGROAPIFDataset* poDS,
                           const CPLString& osName,
                           const CPLJSONArray& oBBOX,
                           const CPLJSONArray& /* oCRS */,
                           const CPLJSONArray& oLinks) :
    m_poDS(poDS)
{
    m_poFeatureDefn = new OGRFeatureDefn(osName);
    m_poFeatureDefn->Reference();
    SetDescription(osName);
    if( oBBOX.IsValid() && oBBOX.Size() > 0 )
    {
        CPLJSONArray oRealBBOX;
        // In the final 1.0.0 spec, spatial.bbox is an array (normally with
        // a single element) of 4-element arrays
        if( oBBOX[0].GetType() == CPLJSONObject::Type::Array )
        {
            oRealBBOX = oBBOX[0].ToArray();
        }
#ifndef REMOVE_SUPPORT_FOR_OLD_VERSIONS
        else if( oBBOX.Size() == 4 || oBBOX.Size() == 6 )
        {
            oRealBBOX = oBBOX;
        }
#endif
        if( oRealBBOX.Size() == 4 || oRealBBOX.Size() == 6 )
        {
            m_oExtent.MinX = oRealBBOX[0].ToDouble();
            m_oExtent.MinY = oRealBBOX[1].ToDouble();
            m_oExtent.MaxX = oRealBBOX[oRealBBOX.Size() == 6 ? 3 : 2].ToDouble();
            m_oExtent.MaxY = oRealBBOX[oRealBBOX.Size() == 6 ? 4 : 3].ToDouble();

            // Handle bbox over antimerdian, which we do not support properly
            // in OGR
            if( m_oExtent.MinX > m_oExtent.MaxX &&
                fabs(m_oExtent.MinX) <= 180.0 &&
                fabs(m_oExtent.MaxX) <= 180.0 )
            {
                m_oExtent.MinX = -180.0;
                m_oExtent.MaxX = 180.0;
            }
        }
    }

    OGRSpatialReference* poSRS = new OGRSpatialReference();
    poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poSRS->Release();

    // Default to what the spec mandates for the /items URL, but check links later
    m_osURL = ConcatenateURLParts(m_poDS->m_osRootURL, "/collections/" + osName + "/items");
    m_osPath = "/collections/" + osName + "/items";

    if( oLinks.IsValid() )
    {
        for( int i = 0; i < oLinks.Size(); i++ )
        {
            CPLJSONObject oLink = oLinks[i];
            if( !oLink.IsValid() ||
                oLink.GetType() != CPLJSONObject::Type::Object )
            {
                continue;
            }
            const auto osRel(oLink.GetString("rel"));
            const auto osURL = oLink.GetString("href");
            const auto type = oLink.GetString("type");
            if( osRel == "describedBy" )
            {
                if (type == MEDIA_TYPE_TEXT_XML ||
                    type == MEDIA_TYPE_APPLICATION_XML )
                {
                    m_osDescribedByURL = osURL;
                    m_osDescribedByType = type;
                    m_bDescribedByIsXML = true;
                }
                else if( type == MEDIA_TYPE_JSON_SCHEMA &&
                         m_osDescribedByURL.empty() )
                {
                    m_osDescribedByURL = osURL;
                    m_osDescribedByType = type;
                    m_bDescribedByIsXML = false;
                }
            }
            else if( osRel == "queryables" )
            {
                if( type == MEDIA_TYPE_JSON || m_osQueryablesURL.empty() )
                {
                    m_osQueryablesURL = m_poDS->ReinjectAuthInURL(osURL);
                }
            }
            else if( osRel == "items" )
            {
                if( type == MEDIA_TYPE_GEOJSON )
                {
                    m_osURL = m_poDS->ReinjectAuthInURL(osURL);
                }
            }
        }
        if( !m_osDescribedByURL.empty() )
        {
            m_osDescribedByURL = m_poDS->ReinjectAuthInURL(m_osDescribedByURL);
        }
    }

    m_bIsGeographicCRS = true;

    OGROAPIFLayer::ResetReading();
}

/************************************************************************/
/*                          ~OGROAPIFLayer()                             */
/************************************************************************/

OGROAPIFLayer::~OGROAPIFLayer()
{
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResolveRefs()                             */
/************************************************************************/

static CPLJSONObject ResolveRefs(const CPLJSONObject& oRoot,
                                 const CPLJSONObject& oObj)
{
     const auto osRef = oObj.GetString("$ref");
    if( osRef.empty() )
        return oObj;
    if( STARTS_WITH(osRef.c_str(), "#/") )
    {
        return oRoot.GetObj(osRef.c_str() + 2);
    }
    CPLJSONObject oInvalid;
    oInvalid.Deinit();
    return oInvalid;
}

/************************************************************************/
/*                      BuildExampleRecursively()                       */
/************************************************************************/

static bool BuildExampleRecursively(CPLJSONObject& oRes,
                             const CPLJSONObject& oRoot,
                             const CPLJSONObject& oObjIn)
{
    auto oResolvedObj = ResolveRefs(oRoot, oObjIn);
    if( !oResolvedObj.IsValid() )
        return false;
    const auto osType = oResolvedObj.GetString("type");
    if( osType == "object" )
    {
        const auto oAllOf = oResolvedObj.GetArray("allOf");
        const auto oProperties = oResolvedObj.GetObj("properties");
        if( oAllOf.IsValid() )
        {
            for( int i = 0; i < oAllOf.Size(); i++ )
            {
                CPLJSONObject oChildRes;
                if( BuildExampleRecursively(oChildRes, oRoot, oAllOf[i]) &&
                    oChildRes.GetType() == CPLJSONObject::Type::Object )
                {
                    auto oChildren = oChildRes.GetChildren();
                    for( const auto& oChild: oChildren )
                    {
                        oRes.Add( oChild.GetName(), oChild );
                    }
                }
            }
        }
        else if( oProperties.IsValid() )
        {
            auto oChildren = oProperties.GetChildren();
            for( const auto& oChild: oChildren )
            {
                CPLJSONObject oChildRes;
                if( BuildExampleRecursively(oChildRes, oRoot, oChild) )
                {
                    oRes.Add( oChild.GetName(), oChildRes );
                }
                else
                {
                    oRes.Add( oChild.GetName(), "unknown type" );
                }
            }
        }
        return true;
    }
    else if( osType == "array" )
    {
        CPLJSONArray oArray;
        const auto oItems = oResolvedObj.GetObj("items");
        if( oItems.IsValid() )
        {
            CPLJSONObject oChildRes;
            if( BuildExampleRecursively(oChildRes, oRoot, oItems) )
            {
                oArray.Add(oChildRes);
            }
        }
        oRes = oArray;
        return true;
    }
    else if( osType == "string" )
    {
        CPLJSONObject oTemp;
        const auto osFormat = oResolvedObj.GetString("format");
        if( !osFormat.empty() )
            oTemp.Set("_", osFormat);
        else
            oTemp.Set("_", "string");
        oRes = oTemp.GetObj("_");
        return true;
    }
    else if( osType == "number" )
    {
        CPLJSONObject oTemp;
        oTemp.Set("_", 1.25);
        oRes = oTemp.GetObj("_");
        return true;
    }
    else if( osType == "integer" )
    {
        CPLJSONObject oTemp;
        oTemp.Set("_", 1);
        oRes = oTemp.GetObj("_");
        return true;
    }
    else if( osType == "boolean" )
    {
        CPLJSONObject oTemp;
        oTemp.Set("_", true);
        oRes = oTemp.GetObj("_");
        return true;
    }
    else if( osType == "null" )
    {
        CPLJSONObject oTemp;
        oTemp.SetNull("_");
        oRes = oTemp.GetObj("_");
        return true;
    }

    return false;
}

/************************************************************************/
/*                     GetObjectExampleFromSchema()                     */
/************************************************************************/

static CPLJSONObject GetObjectExampleFromSchema(const std::string& osJSONSchema)
{
    CPLJSONDocument oDoc;
    if( !oDoc.LoadMemory(osJSONSchema) )
    {
        CPLJSONObject oInvalid;
        oInvalid.Deinit();
        return oInvalid;
    }
    const auto& oRoot = oDoc.GetRoot();
    CPLJSONObject oRes;
    BuildExampleRecursively(oRes, oRoot, oRoot);
    return oRes;
}

/************************************************************************/
/*                            GetSchema()                               */
/************************************************************************/

void OGROAPIFLayer::GetSchema()
{
    if( m_osDescribedByURL.empty() || m_poDS->m_bIgnoreSchema )
        return;

    CPLErrorHandlerPusher oErrorHandlerPusher(CPLQuietErrorHandler);
    CPLErrorStateBackuper oErrorStateBackuper;

    if( m_bDescribedByIsXML )
    {
        std::vector<GMLFeatureClass*> apoClasses;
        bool bFullyUnderstood = false;
        bool bHaveSchema = GMLParseXSD( m_osDescribedByURL, apoClasses,
                                        bFullyUnderstood );
        if (bHaveSchema && apoClasses.size() == 1)
        {
            CPLDebug("OAPIF", "Using XML schema");
            auto poGMLFeatureClass = apoClasses[0];
            if( poGMLFeatureClass->GetGeometryPropertyCount() ==  1 )
            {
                // Force linear type as we work with GeoJSON data
                m_poFeatureDefn->SetGeomType(OGR_GT_GetLinear(
                    static_cast<OGRwkbGeometryType>(
                        poGMLFeatureClass->GetGeometryProperty(0)->GetType())));
            }

            const int nPropertyCount = poGMLFeatureClass->GetPropertyCount();
            // This is a hack for http://www.pvretano.com/cubewerx/cubeserv/default/wfs/3.0.0/framework/collections/UNINCORPORATED_PL/schema
            // The GML representation has attributes starting all with "UNINCORPORATED_PL." whereas the GeoJSON output not
            CPLString osPropertyNamePrefix(GetName());
            osPropertyNamePrefix += '.';
            bool bAllPrefixed = true;
            for( int iField = 0; iField < nPropertyCount; iField++ )
            {
                const auto poProperty = poGMLFeatureClass->GetProperty( iField );
                if( !STARTS_WITH(poProperty->GetName(), osPropertyNamePrefix.c_str()) )
                {
                    bAllPrefixed = false;
                }
            }
            for( int iField = 0; iField < nPropertyCount; iField++ )
            {
                const auto poProperty = poGMLFeatureClass->GetProperty( iField );
                OGRFieldSubType eSubType = OFSTNone;
                const OGRFieldType eFType = GML_GetOGRFieldType(poProperty->GetType(), eSubType);

                const char* pszName = poProperty->GetName() + (bAllPrefixed ? osPropertyNamePrefix.size() : 0);
                std::unique_ptr<OGRFieldDefn> oField(new OGRFieldDefn( pszName, eFType ));
                oField->SetSubType(eSubType);
                m_apoFieldsFromSchema.emplace_back(std::move(oField));
            }
        }

        for( auto poFeatureClass: apoClasses )
            delete poFeatureClass;
    }
    else
    {
        CPLString osContentType;
        CPLString osResult;
        if( !m_poDS->Download(m_osDescribedByURL, m_osDescribedByType,
                              osResult, osContentType) )
        {
            CPLDebug("OAPIF", "Could not download schema");
        }
        else
        {
            const auto oExample = GetObjectExampleFromSchema(osResult);
            //CPLDebug("OAPIF", "Example from schema: %s",
            //         oExample.Format(CPLJSONObject::PrettyFormat::Pretty).c_str());
            if( oExample.IsValid() && oExample.GetType() == CPLJSONObject::Type::Object )
            {
                const auto oProperties = oExample.GetObj("properties");
                if( oProperties.IsValid() && oProperties.GetType() == CPLJSONObject::Type::Object )
                {
                    CPLDebug("OAPIF", "Using JSON schema");
                    const auto oProps = oProperties.GetChildren();
                    for( const auto& oProp: oProps )
                    {
                        OGRFieldType eType = OFTString;
                        OGRFieldSubType eSubType = OFSTNone;
                        const auto oType = oProp.GetType();
                        if( oType == CPLJSONObject::Type::String )
                        {
                            if( oProp.ToString() == "date-time" )
                            {
                                eType = OFTDateTime;
                            }
                            else if( oProp.ToString() == "date" )
                            {
                                eType = OFTDate;
                            }
                        }
                        else if( oType == CPLJSONObject::Type::Boolean )
                        {
                            eType = OFTInteger;
                            eSubType = OFSTBoolean;
                        }
                        else if( oType == CPLJSONObject::Type::Double )
                        {
                            eType = OFTReal;
                        }
                        else if( oType == CPLJSONObject::Type::Integer )
                        {
                            eType = OFTInteger;
                        }
                        else if( oType == CPLJSONObject::Type::Long )
                        {
                            eType = OFTInteger64;
                        }
                        else if( oType == CPLJSONObject::Type::Array )
                        {
                            const auto oArray = oProp.ToArray();
                            if( oArray.Size() > 0 )
                            {
                                if( oArray[0].GetType() == CPLJSONObject::Type::String )
                                    eType = OFTStringList;
                                else if( oArray[0].GetType() == CPLJSONObject::Type::Integer )
                                    eType = OFTIntegerList;
                            }
                        }

                        std::unique_ptr<OGRFieldDefn> oField(new OGRFieldDefn( oProp.GetName().c_str(), eType ));
                        oField->SetSubType(eSubType);
                        m_apoFieldsFromSchema.emplace_back(std::move(oField));
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGROAPIFLayer::GetLayerDefn()
{
    if( !m_bFeatureDefnEstablished )
        EstablishFeatureDefn();
    return m_poFeatureDefn;
}

/************************************************************************/
/*                        EstablishFeatureDefn()                        */
/************************************************************************/

void OGROAPIFLayer::EstablishFeatureDefn()
{
    CPLAssert(!m_bFeatureDefnEstablished);
    m_bFeatureDefnEstablished = true;

    GetSchema();

    CPLJSONDocument oDoc;
    CPLString osURL(m_osURL);
    osURL = CPLURLAddKVP(osURL, "limit",
                            CPLSPrintf("%d", m_poDS->m_nPageSize));
    if( !m_poDS->DownloadJSon(osURL, oDoc) )
        return;

    CPLString osTmpFilename(CPLSPrintf("/vsimem/oapif_%p.json", this));
    oDoc.Save(osTmpFilename);
    std::unique_ptr<GDALDataset> poDS(
      reinterpret_cast<GDALDataset*>(
        GDALOpenEx(osTmpFilename, GDAL_OF_VECTOR | GDAL_OF_INTERNAL,
                   nullptr, nullptr, nullptr)));
    VSIUnlink(osTmpFilename);
    if( !poDS.get() )
        return;
    OGRLayer* poLayer = poDS->GetLayer(0);
    if( !poLayer )
        return;
    OGRFeatureDefn* poFeatureDefn = poLayer->GetLayerDefn();
    if( m_poFeatureDefn->GetGeomType() == wkbUnknown )
    {
        m_poFeatureDefn->SetGeomType( poFeatureDefn->GetGeomType() );
    }
    if( m_apoFieldsFromSchema.empty() )
    {
        for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        {
            m_poFeatureDefn->AddFieldDefn( poFeatureDefn->GetFieldDefn(i) );
        }
    }
    else
    {
        if( poFeatureDefn->GetFieldCount() > 0 &&
            strcmp(poFeatureDefn->GetFieldDefn(0)->GetNameRef(), "id") == 0 )
        {
            m_poFeatureDefn->AddFieldDefn( poFeatureDefn->GetFieldDefn(0) );
        }
        for( const auto& poField: m_apoFieldsFromSchema )
        {
            m_poFeatureDefn->AddFieldDefn( poField.get() );
        }
        // In case there would be properties found in sample, but not in schema...
        for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        {
            auto poFDefn = poFeatureDefn->GetFieldDefn(i);
            if( m_poFeatureDefn->GetFieldIndex(poFDefn->GetNameRef()) < 0 )
            {
                m_poFeatureDefn->AddFieldDefn( poFDefn );
            }
        }
    }

    const auto& oRoot = oDoc.GetRoot();
    GIntBig nFeatures = oRoot.GetLong("numberMatched", -1);
    if( nFeatures >= 0 )
        m_nTotalFeatureCount = nFeatures;

    auto oFeatures = oRoot.GetArray("features");
    if( oFeatures.IsValid() && oFeatures.Size() > 0 )
    {
        auto eType = oFeatures[0].GetObj("id").GetType();
        if( eType == CPLJSONObject::Type::Integer ||
            eType == CPLJSONObject::Type::Long )
        {
            m_bHasIntIdMember = true;
        }
        else if( eType == CPLJSONObject::Type::String )
        {
            m_bHasStringIdMember = true;
        }
    }
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

void OGROAPIFLayer::ResetReading()
{
    m_poUnderlyingDS.reset();
    m_poUnderlyingLayer = nullptr;
    m_nFID = 1;
    m_osGetURL = m_osURL;
    if( !m_osGetID.empty() )
    {
        m_osGetURL += "/" + m_osGetID;
    }
    else
    {
        if( m_poDS->m_nPageSize > 0 )
        {
            m_osGetURL = CPLURLAddKVP(m_osGetURL, "limit",
                                CPLSPrintf("%d", m_poDS->m_nPageSize));
        }
        m_osGetURL = AddFilters(m_osGetURL);
    }
}

/************************************************************************/
/*                           AddFilters()                               */
/************************************************************************/

CPLString OGROAPIFLayer::AddFilters(const CPLString& osURL)
{
    CPLString osURLNew(osURL);
    if( m_poFilterGeom )
    {
        double dfMinX = m_sFilterEnvelope.MinX;
        double dfMinY = m_sFilterEnvelope.MinY;
        double dfMaxX = m_sFilterEnvelope.MaxX;
        double dfMaxY = m_sFilterEnvelope.MaxY;
        bool bAddBBoxFilter = true;
        if( m_bIsGeographicCRS )
        {
            dfMinX = std::max(dfMinX, -180.0);
            dfMinY = std::max(dfMinY, -90.0);
            dfMaxX = std::min(dfMaxX, 180.0);
            dfMaxY = std::min(dfMaxY, 90.0);
            bAddBBoxFilter = dfMinX > -180.0 || dfMinY > -90.0 ||
                             dfMaxX < 180.0 || dfMaxY < 90.0;
        }
        if( bAddBBoxFilter )
        {
            osURLNew = CPLURLAddKVP(osURLNew, "bbox",
                CPLSPrintf("%.18g,%.18g,%.18g,%.18g",
                        dfMinX,dfMinY,dfMaxX,dfMaxY));
        }
    }
    if( !m_osAttributeFilter.empty() )
    {
        if( osURLNew.find('?') == std::string::npos )
            osURLNew += "?";
        else
            osURLNew += "&";
        osURLNew += m_osAttributeFilter;
    }
    return osURLNew;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature* OGROAPIFLayer::GetNextRawFeature()
{
    if( !m_bFeatureDefnEstablished )
        EstablishFeatureDefn();

    OGRFeature* poSrcFeature = nullptr;
    while( true )
    {
        if( m_poUnderlyingLayer == nullptr )
        {
            if( m_osGetURL.empty() )
                return nullptr;

            CPLJSONDocument oDoc;

            CPLString osURL(m_osGetURL);
            m_osGetURL.clear();
            CPLStringList aosHeaders;
            if( !m_poDS->DownloadJSon(osURL, oDoc,
                                      MEDIA_TYPE_GEOJSON ", " MEDIA_TYPE_JSON,
                                      &aosHeaders) )
            {
                return nullptr;
            }

            CPLString osTmpFilename(CPLSPrintf("/vsimem/oapif_%p.json", this));
            oDoc.Save(osTmpFilename);
            m_poUnderlyingDS = std::unique_ptr<GDALDataset>(
            reinterpret_cast<GDALDataset*>(
                GDALOpenEx(osTmpFilename, GDAL_OF_VECTOR | GDAL_OF_INTERNAL,
                        nullptr, nullptr, nullptr)));
            VSIUnlink(osTmpFilename);
            if( !m_poUnderlyingDS.get() )
            {
                return nullptr;
            }
            m_poUnderlyingLayer = m_poUnderlyingDS->GetLayer(0);
            if( !m_poUnderlyingLayer )
            {
                m_poUnderlyingDS.reset();
                return nullptr;
            }

            // To avoid issues with implementations having a non-relevant
            // next link, make sure the current page is not empty
            // We could even check that the feature count is the page size
            // actually
            if( m_poUnderlyingLayer->GetFeatureCount() > 0 && m_osGetID.empty() )
            {
                CPLJSONArray oLinks = oDoc.GetRoot().GetArray("links");
                if( oLinks.IsValid() )
                {
                    int nCountRelNext = 0;
                    CPLString osNextURL;
                    for( int i = 0; i < oLinks.Size(); i++ )
                    {
                        CPLJSONObject oLink = oLinks[i];
                        if( !oLink.IsValid() ||
                            oLink.GetType() != CPLJSONObject::Type::Object )
                        {
                            continue;
                        }
                        if( oLink.GetString("rel") == "next" )
                        {
                            nCountRelNext ++;
                            auto type = oLink.GetString("type");
                            if (type == MEDIA_TYPE_GEOJSON ||
                                type == MEDIA_TYPE_JSON )
                            {
                                m_osGetURL = oLink.GetString("href");
                                break;
                            }
                            else if( type.empty() )
                            {
                                osNextURL = oLink.GetString("href");
                            }
                        }
                    }
                    if( nCountRelNext == 1 && m_osGetURL.empty() )
                    {
                        // In case we go a "rel": "next" without a "type"
                        m_osGetURL = osNextURL;
                    }
                }

#ifdef no_longer_used
                // Recommendation /rec/core/link-header
                if( m_osGetURL.empty() )
                {
                    for( int i = 0; i < aosHeaders.size(); i++ )
                    {
                        CPLDebug("OAPIF", "%s", aosHeaders[i]);
                        if( STARTS_WITH_CI(aosHeaders[i], "Link=") &&
                            strstr(aosHeaders[i], "rel=\"next\"") &&
                            strstr(aosHeaders[i], "type=\"" MEDIA_TYPE_GEOJSON "\"") )
                        {
                            const char* pszStart = strchr(aosHeaders[i], '<');
                            if( pszStart )
                            {
                                const char* pszEnd = strchr(pszStart + 1, '>');
                                if( pszEnd )
                                {
                                    m_osGetURL = pszStart + 1;
                                    m_osGetURL.resize(pszEnd - pszStart - 1);
                                }
                            }
                            break;
                        }
                    }
                }
#endif

                if( !m_osGetURL.empty() )
                {
                    m_osGetURL = m_poDS->ReinjectAuthInURL(m_osGetURL);
                }
            }
        }

        poSrcFeature = m_poUnderlyingLayer->GetNextFeature();
        if( poSrcFeature )
            break;
        m_poUnderlyingDS.reset();
        m_poUnderlyingLayer = nullptr;
    }

    OGRFeature* poFeature = new OGRFeature(m_poFeatureDefn);
    poFeature->SetFrom(poSrcFeature);
    auto poGeom = poFeature->GetGeometryRef();
    if( poGeom )
    {
        poGeom->assignSpatialReference(GetSpatialRef());
    }
    if( m_bHasIntIdMember )
    {
        poFeature->SetFID(poSrcFeature->GetFID());
    }
    else
    {
        poFeature->SetFID(m_nFID);
        m_nFID ++;
    }
    delete poSrcFeature;
    return poFeature;
}

/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature* OGROAPIFLayer::GetFeature(GIntBig nFID)
{
    if( !m_bFeatureDefnEstablished )
        EstablishFeatureDefn();
    if( !m_bHasIntIdMember )
        return OGRLayer::GetFeature(nFID);

    m_osGetID.Printf(CPL_FRMT_GIB, nFID);
    ResetReading();
    auto poRet = GetNextRawFeature();
    m_osGetID.clear();
    ResetReading();
    return poRet;
}

/************************************************************************/
/*                         GetNextFeature()                             */
/************************************************************************/

OGRFeature* OGROAPIFLayer::GetNextFeature()
{
    while( true )
    {
        OGRFeature  *poFeature = GetNextRawFeature();
        if (poFeature == nullptr)
            return nullptr;

        if( (m_poFilterGeom == nullptr ||
                FilterGeometry(poFeature->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr || !m_bFilterMustBeClientSideEvaluated ||
                m_poAttrQuery->Evaluate(poFeature)) )
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
/*                      SupportsResultTypeHits()                        */
/************************************************************************/

bool OGROAPIFLayer::SupportsResultTypeHits()
{
    CPLJSONDocument oDoc = m_poDS->GetAPIDoc();
    if( oDoc.GetRoot().GetString("openapi").empty() )
        return false;

    CPLJSONArray oParameters = oDoc.GetRoot().GetObj("paths")
                                  .GetObj(m_osPath)
                                  .GetObj("get")
                                  .GetArray("parameters");
    if( !oParameters.IsValid() )
        return false;
    for( int i = 0; i < oParameters.Size(); i++ )
    {
        CPLJSONObject oParam = oParameters[i];
        CPLString osRef = oParam.GetString("$ref");
        if( !osRef.empty() && osRef.find("#/") == 0 )
        {
            oParam = oDoc.GetRoot().GetObj(osRef.substr(2));
#ifndef REMOVE_HACK
            // Needed for http://www.pvretano.com/cubewerx/cubeserv/default/wfs/3.0.0/foundation/api
            // that doesn't define #/components/parameters/resultType
            if( osRef == "#/components/parameters/resultType" )
                return true;
#endif
        }
        if( oParam.GetString("name") == "resultType" &&
            oParam.GetString("in") == "query" )
        {
            CPLJSONArray oEnum = oParam.GetArray("schema/enum");
            for( int j = 0; j < oEnum.Size(); j++ )
            {
                if( oEnum[j].ToString() == "hits" )
                    return true;
            }
            return false;
        }
    }

    return false;
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/

GIntBig OGROAPIFLayer::GetFeatureCount(int bForce)
{
    if( m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
    {
        GetLayerDefn();
        if( m_nTotalFeatureCount >= 0  )
        {
            return m_nTotalFeatureCount;
        }
    }

    if( SupportsResultTypeHits() && !m_bFilterMustBeClientSideEvaluated )
    {
        CPLString osURL(m_osURL);
        osURL = CPLURLAddKVP(osURL, "resultType", "hits");
        osURL = AddFilters(osURL);
#ifndef REMOVE_HACK
        bool bGMLRequest = m_osURL.find("cubeserv") != std::string::npos;
#else
        constexpr bool bGMLRequest = false;
#endif
        if( bGMLRequest )
        {
            CPLString osResult;
            CPLString osContentType;
            if( m_poDS->Download(osURL, MEDIA_TYPE_TEXT_XML, osResult, osContentType) )
            {
                CPLXMLNode* psDoc = CPLParseXMLString(osResult);
                if( psDoc )
                {
                    CPLXMLTreeCloser oCloser(psDoc);
                    CPL_IGNORE_RET_VAL(oCloser);
                    CPLStripXMLNamespace(psDoc, nullptr, true);
                    CPLString osNumberMatched =
                        CPLGetXMLValue(psDoc,
                                       "=FeatureCollection.numberMatched", "");
                    if( !osNumberMatched.empty() )
                        return CPLAtoGIntBig(osNumberMatched);
                }
            }
        }
        else
        {
            CPLJSONDocument oDoc;
            if( m_poDS->DownloadJSon(osURL, oDoc) )
            {
                GIntBig nFeatures = oDoc.GetRoot().GetLong("numberMatched", -1);
                if( nFeatures >= 0 )
                    return nFeatures;
            }
        }
    }

    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGROAPIFLayer::GetExtent(OGREnvelope* psEnvelope, int bForce)
{
    if( m_oExtent.IsInit() )
    {
        *psEnvelope = m_oExtent;
        return OGRERR_NONE;
    }
    return OGRLayer::GetExtent(psEnvelope, bForce);
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGROAPIFLayer::SetSpatialFilter(  OGRGeometry *poGeomIn )
{
    InstallFilter( poGeomIn );

    ResetReading();
}

/************************************************************************/
/*                      OGRWF3ParseDateTime()                           */
/************************************************************************/

static int OGRWF3ParseDateTime(const char* pszValue,
                                       int& nYear, int &nMonth, int &nDay,
                                       int& nHour, int &nMinute, int &nSecond)
{
    int ret = sscanf(pszValue,"%04d/%02d/%02d %02d:%02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond);
    if( ret >= 3 )
        return ret;
    return sscanf(pszValue,"%04d-%02d-%02dT%02d:%02d:%02d",
                    &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond);
}

/************************************************************************/
/*                       SerializeDateTime()                            */
/************************************************************************/

static CPLString SerializeDateTime(int nDateComponents,
                                   int nYear,
                                   int nMonth,
                                   int nDay,
                                   int nHour,
                                   int nMinute,
                                   int nSecond)
{
    CPLString osRet;
    osRet.Printf("%04d-%02d-%02dT", nYear, nMonth, nDay);
    if( nDateComponents >= 4 )
    {
        osRet += CPLSPrintf("%02d", nHour);
        if( nDateComponents >= 5 )
            osRet += CPLSPrintf(":%02d", nMinute);
        if( nDateComponents >= 6 )
            osRet += CPLSPrintf(":%02d", nSecond);
        osRet += "Z";
    }
    return osRet;
}

/************************************************************************/
/*                            BuildFilter()                             */
/************************************************************************/

CPLString OGROAPIFLayer::BuildFilter(const swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
    {
        const auto leftExpr = poNode->papoSubExpr[0];
        const auto rightExpr = poNode->papoSubExpr[1];

        // Detect expression: datetime >=|> XXX and datetime <=|< XXXX
        if( leftExpr->eNodeType == SNT_OPERATION &&
            (leftExpr->nOperation == SWQ_GT || leftExpr->nOperation == SWQ_GE) &&
             leftExpr->nSubExprCount == 2 &&
             leftExpr->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             leftExpr->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
            rightExpr->eNodeType == SNT_OPERATION &&
            (rightExpr->nOperation == SWQ_LT || rightExpr->nOperation == SWQ_LE) &&
             rightExpr->nSubExprCount == 2 &&
             rightExpr->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             rightExpr->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
            leftExpr->papoSubExpr[0]->field_index == rightExpr->papoSubExpr[0]->field_index &&
            leftExpr->papoSubExpr[1]->field_type == SWQ_TIMESTAMP &&
            rightExpr->papoSubExpr[1]->field_type == SWQ_TIMESTAMP  )
        {
            const OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(
                leftExpr->papoSubExpr[0]->field_index);
            if( poFieldDefn &&
                 (poFieldDefn->GetType() == OFTDate ||
                  poFieldDefn->GetType() == OFTDateTime) )
            {
                CPLString osExpr;
                {
                    int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
                    int nDateComponents = OGRWF3ParseDateTime(
                        leftExpr->papoSubExpr[1]->string_value,
                        nYear, nMonth, nDay, nHour, nMinute, nSecond);
                    if( nDateComponents >= 3 )
                    {
                        osExpr = "datetime=" +
                            SerializeDateTime(nDateComponents,
                                nYear, nMonth, nDay, nHour, nMinute, nSecond);
                    }
                }
                if( !osExpr.empty() )
                {
                    int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
                    int nDateComponents = OGRWF3ParseDateTime(
                        rightExpr->papoSubExpr[1]->string_value,
                        nYear, nMonth, nDay, nHour, nMinute, nSecond);
                    if( nDateComponents >= 3 )
                    {
                        osExpr += "%2F" // '/' URL encoded
                            + SerializeDateTime(nDateComponents,
                                nYear, nMonth, nDay, nHour, nMinute, nSecond);
                        return osExpr;
                    }
                }
            }
        }

         // For AND, we can deal with a failure in one of the branch
        // since client-side will do that extra filtering
        CPLString osFilter1 = BuildFilter(leftExpr);
        CPLString osFilter2 = BuildFilter(rightExpr);
        if( !osFilter1.empty() && !osFilter2.empty() )
        {
            return osFilter1 + "&" + osFilter2;
        }
        else if( !osFilter1.empty() )
            return osFilter1;
        else
            return osFilter2;
    }
    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_EQ &&
             poNode->nSubExprCount == 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT )
    {
        const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
        const OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(nFieldIdx);
        int nDateComponents;
        int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
        if( m_bHasStringIdMember &&
            strcmp(poFieldDefn->GetNameRef(), "id") == 0 &&
            poNode->papoSubExpr[1]->field_type == SWQ_STRING )
        {
            m_osGetID = poNode->papoSubExpr[1]->string_value;
        }
        else if( poFieldDefn &&
            m_aoSetQueryableAttributes.find(poFieldDefn->GetNameRef()) !=
                m_aoSetQueryableAttributes.end() )
        {
            CPLString osEscapedFieldName;
            {
                char* pszEscapedFieldName = CPLEscapeString(
                    poFieldDefn->GetNameRef(), -1, CPLES_URL);
                osEscapedFieldName = pszEscapedFieldName;
                CPLFree(pszEscapedFieldName);
            }
            if( poNode->papoSubExpr[1]->field_type == SWQ_STRING )
            {
                char* pszEscapedValue = CPLEscapeString(
                    poNode->papoSubExpr[1]->string_value, -1, CPLES_URL);
                CPLString osRet(osEscapedFieldName);
                osRet += "=";
                osRet += pszEscapedValue;
                CPLFree(pszEscapedValue);
                return osRet;
            }
            if( poNode->papoSubExpr[1]->field_type == SWQ_INTEGER )
            {
                CPLString osRet(osEscapedFieldName);
                osRet += "=";
                osRet += CPLSPrintf(CPL_FRMT_GIB,
                                    poNode->papoSubExpr[1]->int_value);
                return osRet;
            }
        }
        else if( poFieldDefn &&
                 (poFieldDefn->GetType() == OFTDate ||
                  poFieldDefn->GetType() == OFTDateTime) &&
                poNode->papoSubExpr[1]->field_type == SWQ_TIMESTAMP &&
                (nDateComponents = OGRWF3ParseDateTime(
                    poNode->papoSubExpr[1]->string_value,
                    nYear, nMonth, nDay, nHour, nMinute, nSecond)) >= 3 )
        {
            return "datetime=" + SerializeDateTime(nDateComponents,
                                nYear, nMonth, nDay, nHour, nMinute, nSecond);
        }
    }
    else if (poNode->eNodeType == SNT_OPERATION &&
             (poNode->nOperation == SWQ_GT ||
              poNode->nOperation == SWQ_GE ||
              poNode->nOperation == SWQ_LT ||
              poNode->nOperation == SWQ_LE) &&
             poNode->nSubExprCount == 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
             poNode->papoSubExpr[1]->field_type == SWQ_TIMESTAMP )
    {
        const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
        const OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(nFieldIdx);
        int nDateComponents;
        int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
        if( poFieldDefn &&
                 (poFieldDefn->GetType() == OFTDate ||
                  poFieldDefn->GetType() == OFTDateTime) &&
                (nDateComponents = OGRWF3ParseDateTime(
                    poNode->papoSubExpr[1]->string_value,
                    nYear, nMonth, nDay, nHour, nMinute, nSecond)) >= 3 )
        {
            CPLString osDT(SerializeDateTime(nDateComponents,
                                nYear, nMonth, nDay, nHour, nMinute, nSecond));
            if( poNode->nOperation == SWQ_GT || poNode->nOperation == SWQ_GE )
            {
                return "datetime=" + osDT + "%2F..";
            }
            else
            {
                return "datetime=..%2F" + osDT;
            }
        }

    }
    m_bFilterMustBeClientSideEvaluated = true;
    return CPLString();
}

/************************************************************************/
/*                       BuildFilterCQLText()                           */
/************************************************************************/

CPLString OGROAPIFLayer::BuildFilterCQLText(const swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
    {
        const auto leftExpr = poNode->papoSubExpr[0];
        const auto rightExpr = poNode->papoSubExpr[1];

         // For AND, we can deal with a failure in one of the branch
        // since client-side will do that extra filtering
        CPLString osFilter1 = BuildFilterCQLText(leftExpr);
        CPLString osFilter2 = BuildFilterCQLText(rightExpr);
        if( !osFilter1.empty() && !osFilter2.empty() )
        {
            return '(' + osFilter1 + ") AND (" + osFilter2 + ')';
        }
        else if( !osFilter1.empty() )
            return osFilter1;
        else
            return osFilter2;
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_OR && poNode->nSubExprCount == 2 )
    {
        const auto leftExpr = poNode->papoSubExpr[0];
        const auto rightExpr = poNode->papoSubExpr[1];

        CPLString osFilter1 = BuildFilterCQLText(leftExpr);
        CPLString osFilter2 = BuildFilterCQLText(rightExpr);
        if( !osFilter1.empty() && !osFilter2.empty() )
        {
            return '(' + osFilter1 + ") OR (" + osFilter2 + ')';
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 )
    {
        const auto childExpr = poNode->papoSubExpr[0];
        CPLString osFilterChild = BuildFilterCQLText(childExpr);
        if( !osFilterChild.empty() )
        {
            return "NOT (" + osFilterChild + ')';
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_ISNULL &&
             poNode->nSubExprCount == 1 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN )
    {
        const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
        const OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(nFieldIdx);
        if( poFieldDefn )
        {
            return CPLString("(") + poFieldDefn->GetNameRef() + " IS NULL)";
        }
    }
    else if (poNode->eNodeType == SNT_OPERATION &&
             (poNode->nOperation == SWQ_EQ ||
              poNode->nOperation == SWQ_NE ||
              poNode->nOperation == SWQ_GT ||
              poNode->nOperation == SWQ_GE ||
              poNode->nOperation == SWQ_LT ||
              poNode->nOperation == SWQ_LE ||
              poNode->nOperation == SWQ_LIKE ||
              poNode->nOperation == SWQ_ILIKE) &&
             poNode->nSubExprCount == 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT )
    {
        const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
        const OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(nFieldIdx);
        if( m_bHasStringIdMember &&
            poNode->nOperation == SWQ_EQ &&
            strcmp(poFieldDefn->GetNameRef(), "id") == 0 &&
            poNode->papoSubExpr[1]->field_type == SWQ_STRING )
        {
            m_osGetID = poNode->papoSubExpr[1]->string_value;
        }
        else if( poFieldDefn &&
                 m_aoSetQueryableAttributes.find(poFieldDefn->GetNameRef()) !=
                    m_aoSetQueryableAttributes.end() )
        {
            CPLString osRet(poFieldDefn->GetNameRef());
            switch(poNode->nOperation )
            {
                case SWQ_EQ: osRet += " = "; break;
                case SWQ_NE: osRet += " <> "; break;
                case SWQ_GT: osRet += " > "; break;
                case SWQ_GE: osRet += " >= "; break;
                case SWQ_LT: osRet += " < "; break;
                case SWQ_LE: osRet += " <= "; break;
                case SWQ_LIKE: osRet += " LIKE "; break;
                case SWQ_ILIKE: osRet += " ILIKE "; break;
                default: CPLAssert(false); break;
            }
            if( poNode->papoSubExpr[1]->field_type == SWQ_STRING )
            {
                osRet += '\'';
                osRet += CPLString(poNode->papoSubExpr[1]->string_value).replaceAll('\'', "''");
                osRet += '\'';
                return osRet;
            }
            if( poNode->papoSubExpr[1]->field_type == SWQ_INTEGER ||
                poNode->papoSubExpr[1]->field_type == SWQ_INTEGER64 )
            {
                osRet += CPLSPrintf(CPL_FRMT_GIB,
                                    poNode->papoSubExpr[1]->int_value);
                return osRet;
            }
            if( poNode->papoSubExpr[1]->field_type == SWQ_FLOAT )
            {
                osRet += CPLSPrintf("%.16g",
                                    poNode->papoSubExpr[1]->float_value);
                return osRet;
            }
            if( poNode->papoSubExpr[1]->field_type == SWQ_TIMESTAMP )
            {
                int nDateComponents;
                int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
                if( (poFieldDefn->GetType() == OFTDate ||
                     poFieldDefn->GetType() == OFTDateTime) &&
                    (nDateComponents = OGRWF3ParseDateTime(
                        poNode->papoSubExpr[1]->string_value,
                        nYear, nMonth, nDay, nHour, nMinute, nSecond)) >= 3 )
                {
                    CPLString osDT(SerializeDateTime(nDateComponents,
                                        nYear, nMonth, nDay, nHour, nMinute, nSecond));
                    osRet += '\'';
                    osRet += osDT;
                    osRet += '\'';
                    return osRet;
                }
            }
        }
    }

    m_bFilterMustBeClientSideEvaluated = true;
    return CPLString();
}

/************************************************************************/
/*                     BuildFilterJSONFilterExpr()                      */
/************************************************************************/

CPLString OGROAPIFLayer::BuildFilterJSONFilterExpr(const swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
    {
        const auto leftExpr = poNode->papoSubExpr[0];
        const auto rightExpr = poNode->papoSubExpr[1];

         // For AND, we can deal with a failure in one of the branch
        // since client-side will do that extra filtering
        CPLString osFilter1 = BuildFilterJSONFilterExpr(leftExpr);
        CPLString osFilter2 = BuildFilterJSONFilterExpr(rightExpr);
        if( !osFilter1.empty() && !osFilter2.empty() )
        {
            return "[\"all\"," + osFilter1 + ',' + osFilter2 + ']';
        }
        else if( !osFilter1.empty() )
            return osFilter1;
        else
            return osFilter2;
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_OR && poNode->nSubExprCount == 2 )
    {
        const auto leftExpr = poNode->papoSubExpr[0];
        const auto rightExpr = poNode->papoSubExpr[1];

        CPLString osFilter1 = BuildFilterJSONFilterExpr(leftExpr);
        CPLString osFilter2 = BuildFilterJSONFilterExpr(rightExpr);
        if( !osFilter1.empty() && !osFilter2.empty() )
        {
            return "[\"any\"," + osFilter1 + ',' + osFilter2 + ']';
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 )
    {
        const auto childExpr = poNode->papoSubExpr[0];
        CPLString osFilterChild = BuildFilterJSONFilterExpr(childExpr);
        if( !osFilterChild.empty() )
        {
            return "[\"!\"," + osFilterChild + ']';
        }
    }
    else if( poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_ISNULL && poNode->nSubExprCount == 1 )
    {
        const auto childExpr = poNode->papoSubExpr[0];
        CPLString osFilterChild = BuildFilterJSONFilterExpr(childExpr);
        if( !osFilterChild.empty() )
        {
            return "[\"==\"," + osFilterChild + ",null]";
        }
    }
    else if (poNode->eNodeType == SNT_OPERATION &&
             (poNode->nOperation == SWQ_EQ ||
              poNode->nOperation == SWQ_NE ||
              poNode->nOperation == SWQ_GT ||
              poNode->nOperation == SWQ_GE ||
              poNode->nOperation == SWQ_LT ||
              poNode->nOperation == SWQ_LE ||
              poNode->nOperation == SWQ_LIKE) &&
             poNode->nSubExprCount == 2  )
    {
        if( m_bHasStringIdMember &&
            poNode->nOperation == SWQ_EQ &&
            poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
            poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
            poNode->papoSubExpr[1]->field_type == SWQ_STRING )
        {
            const int nFieldIdx = poNode->papoSubExpr[0]->field_index;
            const OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(nFieldIdx);
            if( strcmp(poFieldDefn->GetNameRef(), "id") == 0 )
            {
                m_osGetID = poNode->papoSubExpr[1]->string_value;
                return CPLString();
            }
        }

        CPLString osRet("[\"");
        switch(poNode->nOperation )
        {
            case SWQ_EQ: osRet += "=="; break;
            case SWQ_NE: osRet += "!="; break;
            case SWQ_GT: osRet += ">"; break;
            case SWQ_GE: osRet += ">="; break;
            case SWQ_LT: osRet += "<"; break;
            case SWQ_LE: osRet += "<="; break;
            case SWQ_LIKE: osRet += "like"; break;
            default: CPLAssert(false); break;
        }
        osRet += "\",";
        CPLString osFilter1 = BuildFilterJSONFilterExpr(poNode->papoSubExpr[0]);
        CPLString osFilter2 = BuildFilterJSONFilterExpr(poNode->papoSubExpr[1]);
        if( !osFilter1.empty() && !osFilter2.empty() )
        {
            osRet += osFilter1;
            osRet += ',';
            osRet += osFilter2;
            osRet += ']';
            return osRet;
        }
    }
    else if( poNode->eNodeType == SNT_COLUMN )
    {
        const int nFieldIdx = poNode->field_index;
        const OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(nFieldIdx);
        if( poFieldDefn &&
            m_aoSetQueryableAttributes.find(poFieldDefn->GetNameRef()) !=
            m_aoSetQueryableAttributes.end() )
        {
            CPLString osRet("[\"get\",\"");
            osRet += CPLString(poFieldDefn->GetNameRef()).replaceAll('\\',"\\\\").replaceAll('"',"\\\"");
            osRet += "\"]";
            return osRet;
        }
    }
    else if( poNode->eNodeType == SNT_CONSTANT )
    {
        if( poNode->field_type == SWQ_STRING )
        {
            CPLString osRet("\"");
            osRet += CPLString(poNode->string_value).replaceAll('\\',"\\\\").replaceAll('"',"\\\"");
            osRet += '"';
            return osRet;
        }
        if( poNode->field_type == SWQ_INTEGER ||
            poNode->field_type == SWQ_INTEGER64 )
        {
            return CPLSPrintf(CPL_FRMT_GIB,
                                poNode->int_value);
        }
        if( poNode->field_type == SWQ_FLOAT )
        {
            return CPLSPrintf("%.16g",
                                poNode->float_value);
        }
        if( poNode->field_type == SWQ_TIMESTAMP )
        {
            int nYear = 0, nMonth = 0, nDay = 0, nHour = 0, nMinute = 0, nSecond = 0;
            const int nDateComponents =  OGRWF3ParseDateTime(
                    poNode->string_value,
                    nYear, nMonth, nDay, nHour, nMinute, nSecond);
            if( nDateComponents >= 3 )
            {
                CPLString osDT(SerializeDateTime(nDateComponents,
                                    nYear, nMonth, nDay, nHour, nMinute, nSecond));
                CPLString osRet("\"");
                osRet += osDT;
                osRet += '"';
                return osRet;
            }
        }
    }

    m_bFilterMustBeClientSideEvaluated = true;
    return CPLString();
}

/************************************************************************/
/*                        GetQueryableAttributes()                      */
/************************************************************************/

void OGROAPIFLayer::GetQueryableAttributes()
{
    if( m_bGotQueryableAttributes )
        return;
    m_bGotQueryableAttributes = true;
    CPLJSONDocument oAPIDoc = m_poDS->GetAPIDoc();
    if( oAPIDoc.GetRoot().GetString("openapi").empty() )
        return;

    CPLJSONObject oPaths = oAPIDoc.GetRoot().GetObj("paths");
    CPLJSONArray oParameters = oPaths.GetObj(m_osPath)
                                  .GetObj("get")
                                  .GetArray("parameters");
    if( !oParameters.IsValid() )
    {
        oParameters = oPaths.GetObj("/collections/{collectionId}/items")
                                  .GetObj("get")
                                  .GetArray("parameters");
    }
    for( int i = 0; i < oParameters.Size(); i++ )
    {
        CPLJSONObject oParam = oParameters[i];
        CPLString osRef = oParam.GetString("$ref");
        if( !osRef.empty() && osRef.find("#/") == 0 )
        {
            oParam = oAPIDoc.GetRoot().GetObj(osRef.substr(2));
        }
        if( oParam.GetString("in") == "query" )
        {
            const auto osName(oParam.GetString("name"));
            if( osName == "filter-lang" )
            {
                const auto oEnums = oParam.GetObj("schema").GetArray("enum");
                for( int j = 0; j < oEnums.Size(); j++ )
                {
                    if( oEnums[j].ToString() == "cql-text" )
                    {
                        m_bHasCQLText = true;
                        CPLDebug("OAPIF", "CQL text detected");
                    }
                    else if( oEnums[j].ToString() == "json-filter-expr" )
                    {
                        m_bHasJSONFilterExpression = true;
                        CPLDebug("OAPIF", "JSON Filter expression detected");
                    }
                }
            }
            else if( GetLayerDefn()->GetFieldIndex(osName.c_str()) >= 0 )
            {
                m_aoSetQueryableAttributes.insert(osName);
            }
        }
    }

    // HACK
    if( CPLTestBool(CPLGetConfigOption("OGR_OAPIF_ALLOW_CQL_TEXT", "NO")) )
        m_bHasCQLText = true;

    if( m_bHasCQLText || m_bHasJSONFilterExpression )
    {
        if( !m_osQueryablesURL.empty() )
        {
            CPLJSONDocument oDoc;
            if( m_poDS->DownloadJSon(m_osQueryablesURL, oDoc) )
            {
                auto oQueryables = oDoc.GetRoot().GetArray("queryables");
                for( int i = 0; i < oQueryables.Size(); i++ )
                {
                    const auto osId = oQueryables[i].GetString("id");
                    if( !osId.empty() )
                    {
                        m_aoSetQueryableAttributes.insert(osId);
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGROAPIFLayer::SetAttributeFilter( const char *pszQuery )

{
    if( m_poAttrQuery == nullptr && pszQuery == nullptr )
        return OGRERR_NONE;

    if( !m_bFeatureDefnEstablished )
        EstablishFeatureDefn();

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszQuery);

    m_osAttributeFilter.clear();
    m_bFilterMustBeClientSideEvaluated = false;
    m_osGetID.clear();
    if( m_poAttrQuery != nullptr )
    {
        GetQueryableAttributes();

        swq_expr_node* poNode = (swq_expr_node*) m_poAttrQuery->GetSWQExpr();

        poNode->ReplaceBetweenByGEAndLERecurse();

        if( m_bHasCQLText )
        {
            m_osAttributeFilter = BuildFilterCQLText(poNode);
            if( !m_osAttributeFilter.empty() )
            {
                char* pszEscaped = CPLEscapeString(
                    m_osAttributeFilter, -1, CPLES_URL);
                m_osAttributeFilter = "filter=";
                m_osAttributeFilter += pszEscaped;
                m_osAttributeFilter += "&filter-lang=cql-text";
                CPLFree(pszEscaped);
            }
        }
        else if( m_bHasJSONFilterExpression )
        {
            m_osAttributeFilter = BuildFilterJSONFilterExpr(poNode);
            if( !m_osAttributeFilter.empty() )
            {
                char* pszEscaped = CPLEscapeString(
                    m_osAttributeFilter, -1, CPLES_URL);
                m_osAttributeFilter = "filter=";
                m_osAttributeFilter += pszEscaped;
                m_osAttributeFilter += "&filter-lang=json-filter-expr";
                CPLFree(pszEscaped);
            }
        }
        else
        {
            m_osAttributeFilter = BuildFilter(poNode);
        }
        if( m_osAttributeFilter.empty() )
        {
            CPLDebug("OAPIF",
                        "Full filter will be evaluated on client side.");
        }
        else if( m_bFilterMustBeClientSideEvaluated )
        {
            CPLDebug("OAPIF",
                "Only part of the filter will be evaluated on server side.");
        }
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                              TestCapability()                        */
/************************************************************************/

int OGROAPIFLayer::TestCapability(const char* pszCap)
{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
    {
        return m_nTotalFeatureCount >= 0 &&
               m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;
    }
    if( EQUAL(pszCap, OLCFastGetExtent) )
    {
        return m_oExtent.IsInit();
    }
    if( EQUAL(pszCap, OLCStringsAsUTF8) )
    {
        return TRUE;
    }
    // Don't advertise OLCRandomRead as it requires a GET per feature
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGROAPIFDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGROAPIFDriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return nullptr;
    std::unique_ptr<OGROAPIFDataset> poDataset(new OGROAPIFDataset());
    if( !poDataset->Open(poOpenInfo) )
        return nullptr;
    return poDataset.release();
}

/************************************************************************/
/*                           RegisterOGROAPIF()                         */
/************************************************************************/

void RegisterOGROAPIF()

{
    if( GDALGetDriverByName( "OAPIF" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "OAPIF" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "OGC API - Features" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/oapif.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "OAPIF:" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='URL' type='string' "
        "description='URL to the landing page or a /collections/{id}' required='true'/>"
"  <Option name='PAGE_SIZE' type='int' "
        "description='Maximum number of features to retrieve in a single request'/>"
"  <Option name='USERPWD' type='string' "
        "description='Basic authentication as username:password'/>"
"  <Option name='IGNORE_SCHEMA' type='boolean' "
        "description='Whether the XML Schema or JSON Schema should be ignored' default='NO'/>"
"</OpenOptionList>" );

    poDriver->pfnIdentify = OGROAPIFDriverIdentify;
    poDriver->pfnOpen = OGROAPIFDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
