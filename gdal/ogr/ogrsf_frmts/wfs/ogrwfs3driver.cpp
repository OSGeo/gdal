/******************************************************************************
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGRWFSDriver.
 * Author:   Even Rouault, even dot rouault at spatialys dot com
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
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
#include "swq.h"

#include <memory>
#include <vector>
#include <set>

// g++ -Wshadow -Wextra -std=c++11 -fPIC -g -Wall ogr/ogrsf_frmts/wfs/ogrwfs3*.cpp -shared -o ogr_WFS3.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gml -Iogr/ogrsf_frmts/wfs -L. -lgdal

extern "C" void RegisterOGRWFS3();

/************************************************************************/
/*                           OGRWFS3Dataset                             */
/************************************************************************/
class OGRWFS3Layer;

class OGRWFS3Dataset final: public GDALDataset
{
        friend class OGRWFS3Layer;

        CPLString                              m_osRootURL;
        CPLString                              m_osUserPwd;
        int                                    m_nPageSize = 10;
        std::vector<std::unique_ptr<OGRLayer>> m_apoLayers;
        bool                                   m_bAPIDocLoaded = false;
        CPLJSONDocument                        m_oAPIDoc;

        bool                    Download(
            const CPLString& osURL,
            const char* pszAccept,
            CPLString& osResult,
            CPLString& osContentType,
            CPLStringList* paosHeaders = nullptr );

        bool                    DownloadJSon(
            const CPLString& osURL,
            CPLJSONDocument& oDoc,
            const char* pszAccept = "application/geo+json, application/json",
            CPLStringList* paosHeaders = nullptr);

    public:
        OGRWFS3Dataset() {}
        ~OGRWFS3Dataset() {}

        int GetLayerCount() override
                            { return static_cast<int>(m_apoLayers.size()); }
        OGRLayer* GetLayer(int idx) override;

        bool Open(GDALOpenInfo*);
        const CPLJSONDocument& GetAPIDoc();
};

/************************************************************************/
/*                            OGRWFS3Layer                              */
/************************************************************************/

class OGRWFS3Layer final: public OGRLayer
{
        OGRWFS3Dataset* m_poDS = nullptr;
        OGRFeatureDefn* m_poFeatureDefn = nullptr;
        CPLString       m_osURL;
        CPLString       m_osPath;
        OGREnvelope     m_oExtent;
        bool            m_bFeatureDefnEstablished = false;
        std::unique_ptr<GDALDataset> m_poUnderlyingDS;
        OGRLayer*       m_poUnderlyingLayer = nullptr;
        GIntBig         m_nFID = 1;
        CPLString       m_osGetURL;
        CPLString       m_osAttributeFilter;
        bool            m_bFilterMustBeClientSideEvaluated = false;
        bool            m_bGotQueriableAttributes = false;
        std::set<CPLString> m_aoSetQueriableAttributes;

        void            EstablishFeatureDefn();
        OGRFeature     *GetNextRawFeature();
        CPLString       AddFilters(const CPLString& osURL);
        CPLString       BuildFilter(swq_expr_node* poNode);
        bool            SupportsResultTypeHits();
        void            GetQueriableAttributes();

    public:
        OGRWFS3Layer(OGRWFS3Dataset* poDS,
                     const CPLString& osName,
                     const CPLString& osTitle,
                     const CPLString& osDescription,
                     const CPLJSONArray& oBBOX,
                     const CPLJSONArray& oLinks,
                     const CPLJSONArray& oCRS);
        OGRWFS3Layer(OGRWFS3Dataset* poDS,
                     const CPLString& osName,
                     const CPLString& osTitle,
                     const CPLString& osURL,
                     const OGREnvelope& oEnvelope);
       ~OGRWFS3Layer();

       const char*     GetName() override { return GetDescription(); }
       OGRFeatureDefn* GetLayerDefn() override;
       void            ResetReading() override;
       OGRFeature*     GetNextFeature() override;
       int             TestCapability(const char*) override { return false; }
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
/*                              Download()                              */
/************************************************************************/

bool OGRWFS3Dataset::Download(
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
        CPLDebug("WFS3", "Reading %s", osURL.c_str());
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
    CPLHTTPResult* psResult = CPLHTTPFetch(osURL, papszOptions);
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
    // FIXME ?
    if( strstr(pszAccept, "json") )
    {
        if( strstr(osURL, "raw.githubusercontent.com") &&
            strstr(osURL, ".json") )
        {
            bFoundExpectedContentType = true;
        }
        else if( psResult->pszContentType != nullptr &&
            (strstr(psResult->pszContentType, "application/json") != nullptr ||
            strstr(psResult->pszContentType, "application/geo+json") != nullptr) )
        {
            bFoundExpectedContentType = true;
        }
    }
    if( strstr(pszAccept, "xml") &&
        psResult->pszContentType != nullptr &&
        strstr(psResult->pszContentType, "text/xml") != nullptr )
    {
        bFoundExpectedContentType = true;
    }
    if( strstr(pszAccept, "application/openapi+json;version=3.0") &&
        psResult->pszContentType != nullptr &&
        strstr(psResult->pszContentType, "application/openapi+json;version=3.0") != nullptr )
    {
        bFoundExpectedContentType = true;
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

bool OGRWFS3Dataset::DownloadJSon(const CPLString& osURL,
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
/*                            GetAPIDoc()                               */
/************************************************************************/

const CPLJSONDocument& OGRWFS3Dataset::GetAPIDoc()
{
    if( m_bAPIDocLoaded )
        return m_oAPIDoc;
    m_bAPIDocLoaded = true;
#ifndef REMOVE_HACK
    CPLPushErrorHandler(CPLQuietErrorHandler);
#endif
    CPLString osURL(m_osRootURL + "/api");
#ifndef REMOVE_HACK
    osURL = CPLGetConfigOption("OGR_WFS3_API_URL", osURL.c_str());
#endif
    bool bOK = DownloadJSon(osURL, m_oAPIDoc,
                     "application/openapi+json;version=3.0, application/json");
#ifndef REMOVE_HACK
    CPLPopErrorHandler();
    CPLErrorReset();
#endif
    if( bOK )
    {
        return m_oAPIDoc;
    }

#ifndef REMOVE_HACK
    if( DownloadJSon(m_osRootURL + "/api/", m_oAPIDoc,
                     "application/openapi+json;version=3.0, application/json") )
    {
        return m_oAPIDoc;
    }
#endif
    return m_oAPIDoc;
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

bool OGRWFS3Dataset::Open(GDALOpenInfo* poOpenInfo)
{
    m_osRootURL =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "URL",
            poOpenInfo->pszFilename + strlen("WFS3:"));
    m_nPageSize = atoi( CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                            "PAGE_SIZE",CPLSPrintf("%d", m_nPageSize)) );
    m_osUserPwd =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "USERPWD", "");
    CPLString osResult;
    CPLString osContentType;
    // FIXME: json would be preferable in first position, but
    // http://www.pvretano.com/cubewerx/cubeserv/default/wfs/3.0.0/foundation doesn't like it
    if( !Download(m_osRootURL + "/collections",
            "text/xml, application/json",
            osResult, osContentType) )
        return false;

    if( osContentType.find("json") != std::string::npos )
    {
        CPLJSONDocument oDoc;
        if( !oDoc.LoadMemory(osResult) )
        {
            return false;
        }
        CPLJSONArray oCollections = oDoc.GetRoot().GetArray("collections");
        if( !oCollections.IsValid() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "No collections array");
            return false;
        }

        for( int i = 0; i < oCollections.Size(); i++ )
        {
            CPLJSONObject oCollection = oCollections[i];
            if( oCollection.GetType() != CPLJSONObject::Type::Object )
                continue;
            CPLString osName( oCollection.GetString("name") );
#ifndef REMOVE_HACK
            if( osName.empty() )
                osName = oCollection.GetString("collectionId");
#endif
            // "name" will be soon be replaced by "id"
            // https://github.com/opengeospatial/WFS_FES/issues/171
            if( osName.empty() )
                osName = oCollection.GetString("id");

            if( osName.empty() )
                continue;
            CPLString osTitle( oCollection.GetString("title") );
            CPLString osDescription( oCollection.GetString("description") );
            CPLJSONArray oBBOX = oCollection.GetArray("extent/spatial");
            CPLJSONArray oLinks = oCollection.GetArray("links");
            CPLJSONArray oCRS = oCollection.GetArray("crs");
            m_apoLayers.push_back( std::unique_ptr<OGRWFS3Layer>( new
                OGRWFS3Layer(this, osName, osTitle, osDescription,
                            oBBOX, oLinks, oCRS) ) );
        }
    }
    else if( osContentType.find("xml") != std::string::npos )
    {
        CPLXMLNode* psDoc = CPLParseXMLString(osResult);
        if( !psDoc )
            return false;
        CPLXMLTreeCloser oCloser(psDoc);
        CPLStripXMLNamespace(psDoc, nullptr, true);
        CPLXMLNode* psCollections = CPLGetXMLNode(psDoc, "=Collections");
        if( !psCollections )
            return false;
        for( CPLXMLNode* psIter = psCollections->psChild;
                                            psIter; psIter = psIter->psNext )
        {
            if( psIter->eType == CXT_Element &&
                strcmp(psIter->pszValue, "Collection") == 0 )
            {
                CPLString osHref;
                OGREnvelope oEnvelope;
                for( CPLXMLNode* psCollIter = psIter->psChild;
                                psCollIter; psCollIter = psCollIter->psNext )
                {
                    if( psCollIter->eType == CXT_Element &&
                        strcmp(psCollIter->pszValue, "link") == 0 )
                    {
                        CPLString osRel(CPLGetXMLValue(psCollIter, "rel", ""));
                        if( osRel == "collection" )
                        {
                            osHref = CPLGetXMLValue(psCollIter, "href", "");
                            break;
                        }
                    }
                }
                CPLString osName(CPLGetXMLValue(psIter, "Name", ""));
                CPLString osTitle(CPLGetXMLValue(psIter, "Title", ""));
                CPLString osLC(CPLGetXMLValue(psIter,
                                        "WGS84BoundingBox.LowerCorner", ""));
                CPLString osUC(CPLGetXMLValue(psIter,
                                        "WGS84BoundingBox.UpperCorner", ""));
                CPLStringList aosLC(CSLTokenizeString2(osLC, " ", 0));
                CPLStringList aosUC(CSLTokenizeString2(osUC, " ", 0));
                if( aosLC.size() == 2 && aosUC.size() == 2 )
                {
                    oEnvelope.MinX = CPLAtof(aosLC[0]);
                    oEnvelope.MinY = CPLAtof(aosLC[1]);
                    oEnvelope.MaxX = CPLAtof(aosUC[0]);
                    oEnvelope.MaxY = CPLAtof(aosUC[1]);
                }
                if( !osHref.empty() )
                {
                    m_apoLayers.push_back( std::unique_ptr<OGRWFS3Layer>( new
                        OGRWFS3Layer(
                            this, osName, osTitle, osHref, oEnvelope) ) );
                }
            }
        }
    }

    return true;
}

/************************************************************************/
/*                             GetLayer()                               */
/************************************************************************/

OGRLayer* OGRWFS3Dataset::GetLayer(int nIndex)
{
    if( nIndex < 0 || nIndex >= GetLayerCount() )
        return nullptr;
    return m_apoLayers[nIndex].get();
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

static int OGRWFS3DriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "WFS3:");
}

/************************************************************************/
/*                           OGRWFS3Layer()                             */
/************************************************************************/

OGRWFS3Layer::OGRWFS3Layer(OGRWFS3Dataset* poDS,
                           const CPLString& osName,
                           const CPLString& osTitle,
                           const CPLString& osDescription,
                           const CPLJSONArray& oBBOX,
                           const CPLJSONArray& /* oLinks */,
                           const CPLJSONArray& oCRS) :
    m_poDS(poDS)
{
    m_poFeatureDefn = new OGRFeatureDefn(osName);
    m_poFeatureDefn->Reference();
    SetDescription(osName);
    if( !osTitle.empty() )
        SetMetadataItem("TITLE", osTitle.c_str());
    if( !osDescription.empty() )
        SetMetadataItem("DESCRIPTION", osDescription.c_str());
    if( oBBOX.IsValid() && oBBOX.Size() == 4 )
    {
        m_oExtent.MinX = oBBOX[0].ToDouble();
        m_oExtent.MinY = oBBOX[1].ToDouble();
        m_oExtent.MaxX = oBBOX[2].ToDouble();
        m_oExtent.MaxY = oBBOX[3].ToDouble();

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
    if( !oCRS.IsValid() || oCRS.Size() == 0 )
    {
        OGRSpatialReference* poSRS = new OGRSpatialReference();
        poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        poSRS->Release();
    }

    m_osURL = m_poDS->m_osRootURL + "/collections/" + osName + "/items"; // FIXME
    m_osPath = "/collections/" + osName + "/items"; // FIXME

    OGRWFS3Layer::ResetReading();
}

/************************************************************************/
/*                           OGRWFS3Layer()                             */
/************************************************************************/

OGRWFS3Layer::OGRWFS3Layer(OGRWFS3Dataset* poDS,
                           const CPLString& osName,
                           const CPLString& osTitle,
                           const CPLString& osURL,
                           const OGREnvelope& oEnvelope) :
    m_poDS(poDS),
    m_osURL(osURL)
{
    m_poFeatureDefn = new OGRFeatureDefn(osName);
    m_poFeatureDefn->Reference();
    SetDescription(osName);
    if( !osTitle.empty() )
        SetMetadataItem("TITLE", osTitle.c_str());
    if( oEnvelope.IsInit() )
    {
        m_oExtent = oEnvelope;
    }

    OGRSpatialReference* poSRS = new OGRSpatialReference();
    poSRS->SetFromUserInput(SRS_WKT_WGS84_LAT_LONG);
    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poSRS->Release();

    size_t nPos = osURL.rfind('/');
    if( nPos != std::string::npos )
        m_osPath = osURL.substr(nPos);

    ResetReading();
}

/************************************************************************/
/*                          ~OGRWFS3Layer()                             */
/************************************************************************/

OGRWFS3Layer::~OGRWFS3Layer()
{
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn* OGRWFS3Layer::GetLayerDefn()
{
    if( !m_bFeatureDefnEstablished )
        EstablishFeatureDefn();
    return m_poFeatureDefn;
}

/************************************************************************/
/*                        EstablishFeatureDefn()                        */
/************************************************************************/

void OGRWFS3Layer::EstablishFeatureDefn()
{
    CPLAssert(!m_bFeatureDefnEstablished);
    m_bFeatureDefnEstablished = true;

    CPLJSONDocument oDoc;
    CPLString osURL(m_osURL);
    osURL = CPLURLAddKVP(osURL, "limit",
                            CPLSPrintf("%d", m_poDS->m_nPageSize));
    if( !m_poDS->DownloadJSon(osURL, oDoc) )
        return;

    CPLString osTmpFilename(CPLSPrintf("/vsimem/wfs3_%p.json", this));
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
    m_poFeatureDefn->SetGeomType( poFeatureDefn->GetGeomType() );
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        m_poFeatureDefn->AddFieldDefn( poFeatureDefn->GetFieldDefn(i) );
    }
}

/************************************************************************/
/*                           ResetReading()                             */
/************************************************************************/

void OGRWFS3Layer::ResetReading()
{
    m_poUnderlyingDS.reset();
    m_poUnderlyingLayer = nullptr;
    m_nFID = 1;
    m_osGetURL = m_osURL;
    if( m_poDS->m_nPageSize > 0 )
    {
        m_osGetURL = CPLURLAddKVP(m_osGetURL, "limit",
                            CPLSPrintf("%d", m_poDS->m_nPageSize));
    }
    m_osGetURL = AddFilters(m_osGetURL);
}

/************************************************************************/
/*                           AddFilters()                               */
/************************************************************************/

CPLString OGRWFS3Layer::AddFilters(const CPLString& osURL)
{
    CPLString osURLNew(osURL);
    if( m_poFilterGeom )
    {
        osURLNew = CPLURLAddKVP(osURLNew, "bbox",
            CPLSPrintf("%.18g,%.18g,%.18g,%.18g",
                       m_sFilterEnvelope.MinX,
                       m_sFilterEnvelope.MinY,
                       m_sFilterEnvelope.MaxX,
                       m_sFilterEnvelope.MaxY));
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

OGRFeature* OGRWFS3Layer::GetNextRawFeature()
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
                                      "application/geo+json, application/json",
                                      &aosHeaders) )
            {
                return nullptr;
            }

            CPLString osTmpFilename(CPLSPrintf("/vsimem/wfs3_%p.json", this));
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
            if( m_poUnderlyingLayer->GetFeatureCount() > 0 )
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
                            if (type == "application/geo+json" ||
                                type == "application/json" )
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

                if( m_osGetURL.empty() )
                {
                    for( int i = 0; i < aosHeaders.size(); i++ )
                    {
                        CPLDebug("WFS3", "%s", aosHeaders[i]);
                        if( STARTS_WITH_CI(aosHeaders[i], "Link=") &&
                            strstr(aosHeaders[i], "rel=\"next\"") &&
                            strstr(aosHeaders[i], "type=\"application/geo+json\"") )
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

                // If source URL is https://user:pwd@server.com/bla
                // and link only contains https://server.com/bla, then insert
                // into it user:pwd
                const auto nArobaseInURLPos = osURL.find('@');
                if( !m_osGetURL.empty() &&
                    STARTS_WITH(osURL, "https://") &&
                    STARTS_WITH(m_osGetURL, "https://") &&
                    nArobaseInURLPos != std::string::npos &&
                    m_osGetURL.find('@') == std::string::npos )
                {
                    const auto nFirstSlashPos = osURL.find('/', strlen("https://"));
                    if( nFirstSlashPos != std::string::npos &&
                        nFirstSlashPos > nArobaseInURLPos )
                    {
                        auto osUserPwd = osURL.substr(strlen("https://"),
                                        nArobaseInURLPos - strlen("https://"));
                        auto osServer = osURL.substr(nArobaseInURLPos + 1,
                                                     nFirstSlashPos - nArobaseInURLPos);
                        if( STARTS_WITH(m_osGetURL, ("https://" + osServer).c_str()) )
                        {
                            m_osGetURL = "https://" + osUserPwd + "@" +
                                    m_osGetURL.substr(strlen("https://"));
                        }
                    }
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
    poFeature->SetFID(m_nFID);
    m_nFID ++;
    delete poSrcFeature;
    return poFeature;
}

/************************************************************************/
/*                         GetNextFeature()                             */
/************************************************************************/

OGRFeature* OGRWFS3Layer::GetNextFeature()
{
    while( true )
    {
        OGRFeature  *poFeature = GetNextRawFeature();
        if (poFeature == nullptr)
            return nullptr;

        if( (m_poFilterGeom == nullptr ||
                FilterGeometry(poFeature->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr ||
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

bool OGRWFS3Layer::SupportsResultTypeHits()
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

GIntBig OGRWFS3Layer::GetFeatureCount(int bForce)
{
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
            if( m_poDS->Download(osURL, "text/xml", osResult, osContentType) )
            {
                CPLXMLNode* psDoc = CPLParseXMLString(osResult);
                if( psDoc )
                {
                    CPLXMLTreeCloser oCloser(psDoc);
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

OGRErr OGRWFS3Layer::GetExtent(OGREnvelope* psEnvelope, int bForce)
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

void OGRWFS3Layer::SetSpatialFilter(  OGRGeometry *poGeomIn )
{
    InstallFilter( poGeomIn );

    ResetReading();
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

CPLString OGRWFS3Layer::BuildFilter(swq_expr_node* poNode)
{
    if( poNode->eNodeType == SNT_OPERATION &&
        poNode->nOperation == SWQ_AND && poNode->nSubExprCount == 2 )
    {
         // For AND, we can deal with a failure in one of the branch
        // since client-side will do that extra filtering
        CPLString osFilter1 = BuildFilter(poNode->papoSubExpr[0]);
        CPLString osFilter2 = BuildFilter(poNode->papoSubExpr[1]);
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
        OGRFieldDefn* poFieldDefn = GetLayerDefn()->GetFieldDefn(nFieldIdx);
        if( poFieldDefn &&
            m_aoSetQueriableAttributes.find(poFieldDefn->GetNameRef()) !=
                m_aoSetQueriableAttributes.end() )
        {
            if( poNode->papoSubExpr[1]->field_type == SWQ_STRING )
            {
                char* pszEscapedValue = CPLEscapeString(
                    poNode->papoSubExpr[1]->string_value, -1, CPLES_URL);
                CPLString osRet(poFieldDefn->GetNameRef());
                osRet += "=";
                osRet += pszEscapedValue;
                CPLFree(pszEscapedValue);
                return osRet;
            }
            if( poNode->papoSubExpr[1]->field_type == SWQ_INTEGER )
            {
                CPLString osRet(poFieldDefn->GetNameRef());
                osRet += "=";
                osRet += CPLSPrintf(CPL_FRMT_GIB,
                                    poNode->papoSubExpr[1]->int_value);
                return osRet;
            }
        }
    }
    m_bFilterMustBeClientSideEvaluated = true;
    return CPLString();
}

/************************************************************************/
/*                        GetQueriableAttributes()                      */
/************************************************************************/

void OGRWFS3Layer::GetQueriableAttributes()
{
    if( m_bGotQueriableAttributes )
        return;
    m_bGotQueriableAttributes = true;
    CPLJSONDocument oDoc = m_poDS->GetAPIDoc();
    if( oDoc.GetRoot().GetString("openapi").empty() )
        return;

    CPLJSONArray oParameters = oDoc.GetRoot().GetObj("paths")
                                  .GetObj(m_osPath)
                                  .GetObj("get")
                                  .GetArray("parameters");
    if( !oParameters.IsValid() )
        return;
    for( int i = 0; i < oParameters.Size(); i++ )
    {
        CPLJSONObject oParam = oParameters[i];
        CPLString osRef = oParam.GetString("$ref");
        if( !osRef.empty() && osRef.find("#/") == 0 )
        {
            oParam = oDoc.GetRoot().GetObj(osRef.substr(2));
        }
        if( oParam.GetString("in") == "query" &&
            GetLayerDefn()->GetFieldIndex(
                                oParam.GetString("name").c_str()) >= 0 )
        {
            m_aoSetQueriableAttributes.insert(oParam.GetString("name"));
        }
    }
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRWFS3Layer::SetAttributeFilter( const char *pszQuery )

{
    if( !m_bFeatureDefnEstablished )
        EstablishFeatureDefn();

    OGRErr eErr = OGRLayer::SetAttributeFilter(pszQuery);

    m_osAttributeFilter.clear();
    m_bFilterMustBeClientSideEvaluated = false;
    if( m_poAttrQuery != nullptr )
    {
        GetQueriableAttributes();

        swq_expr_node* poNode = (swq_expr_node*) m_poAttrQuery->GetSWQExpr();

        poNode->ReplaceBetweenByGEAndLERecurse();

        m_osAttributeFilter = BuildFilter(poNode);
        if( m_osAttributeFilter.empty() )
        {
            CPLDebug("WFS3",
                        "Full filter will be evaluated on client side.");
        }
        else if( m_bFilterMustBeClientSideEvaluated )
        {
            CPLDebug("WFS3",
                "Only part of the filter will be evaluated on server side.");
        }
    }

    ResetReading();

    return eErr;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRWFS3DriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRWFS3DriverIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return nullptr;
    std::unique_ptr<OGRWFS3Dataset> poDataset(new OGRWFS3Dataset());
    if( !poDataset->Open(poOpenInfo) )
        return nullptr;
    return poDataset.release();
}

/************************************************************************/
/*                           RegisterOGRWFS()                           */
/************************************************************************/

void RegisterOGRWFS3()

{
    if( GDALGetDriverByName( "WFS3" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "WFS3" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "OGC WFS 3 client (Web Feature Service)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_wfs3.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "WFS3:" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='URL' type='string' "
        "description='URL to the WFS server endpoint' required='true'/>"
"  <Option name='PAGE_SIZE' type='int' "
        "description='Maximum number of features to retrieve in a single request'/>"
"  <Option name='USERPWD' type='string' "
        "description='Basic authentication as username:password'/>"
"</OpenOptionList>" );

    poDriver->pfnIdentify = OGRWFS3DriverIdentify;
    poDriver->pfnOpen = OGRWFS3DriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
