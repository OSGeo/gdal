/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONDriver class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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

#include "cpl_port.h"
#include "ogr_geojson.h"

#include <stdlib.h>
#include <string.h>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
// #include "json_object.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogrgeojsonutils.h"
#include "ogrsf_frmts.h"

static CPLMutex *ghMutex = nullptr;
static char *gpszSource = nullptr;
static char *gpszText = nullptr;

class OGRESRIFeatureServiceDataset;

/************************************************************************/
/*                      OGRESRIFeatureServiceLayer                      */
/************************************************************************/

class OGRESRIFeatureServiceLayer final : public OGRLayer
{
    OGRESRIFeatureServiceDataset *poDS;
    OGRFeatureDefn *poFeatureDefn;
    GIntBig nFeaturesRead;
    GIntBig nFirstFID;
    GIntBig nLastFID;
    bool bOtherPage;
    bool bUseSequentialFID;

  public:
    explicit OGRESRIFeatureServiceLayer(OGRESRIFeatureServiceDataset *poDS);
    virtual ~OGRESRIFeatureServiceLayer();

    void ResetReading() override;
    OGRFeature *GetNextFeature() override;
    GIntBig GetFeatureCount(int bForce = TRUE) override;
    OGRErr GetExtent(OGREnvelope *psExtent, int bForce = TRUE) override;

    virtual OGRErr GetExtent(int iGeomField, OGREnvelope *psExtent,
                             int bForce) override
    {
        return OGRLayer::GetExtent(iGeomField, psExtent, bForce);
    }

    int TestCapability(const char *pszCap) override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return poFeatureDefn;
    }
};

/************************************************************************/
/*                       OGRESRIFeatureServiceDataset                   */
/************************************************************************/

class OGRESRIFeatureServiceDataset final : public GDALDataset
{
    CPLString m_osURL{};
    GIntBig m_nFirstOffset = 0;
    GIntBig m_nLastOffset = 0;
    std::unique_ptr<OGRGeoJSONDataSource> m_poCurrent{};
    std::unique_ptr<OGRESRIFeatureServiceLayer> m_poLayer{};
    GeoJSONSourceType m_nSrcType = eGeoJSONSourceUnknown;

    bool LoadPage();

  public:
    OGRESRIFeatureServiceDataset(
        const std::string &osURL,
        std::unique_ptr<OGRGeoJSONDataSource> &&poFirst,
        GeoJSONSourceType nSrcType);

    int GetLayerCount() override
    {
        return 1;
    }

    OGRLayer *GetLayer(int nLayer) override
    {
        return (nLayer == 0) ? m_poLayer.get() : nullptr;
    }

    OGRLayer *GetUnderlyingLayer()
    {
        return m_poCurrent->GetLayer(0);
    }

    bool MyResetReading();
    bool LoadNextPage();

    const CPLString &GetURL() const
    {
        return m_osURL;
    }
};

/************************************************************************/
/*                       OGRESRIFeatureServiceLayer()                   */
/************************************************************************/

OGRESRIFeatureServiceLayer::OGRESRIFeatureServiceLayer(
    OGRESRIFeatureServiceDataset *poDSIn)
    : poDS(poDSIn), nFeaturesRead(0), nFirstFID(0), nLastFID(0),
      bOtherPage(false), bUseSequentialFID(false)
{
    OGRFeatureDefn *poSrcFeatDefn = poDS->GetUnderlyingLayer()->GetLayerDefn();
    poFeatureDefn = new OGRFeatureDefn(poSrcFeatDefn->GetName());
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    for (int i = 0; i < poSrcFeatDefn->GetFieldCount(); i++)
        poFeatureDefn->AddFieldDefn(poSrcFeatDefn->GetFieldDefn(i));

    for (int i = 0; i < poSrcFeatDefn->GetGeomFieldCount(); i++)
        poFeatureDefn->AddGeomFieldDefn(poSrcFeatDefn->GetGeomFieldDefn(i));
}

/************************************************************************/
/*                      ~OGRESRIFeatureServiceLayer()                   */
/************************************************************************/

OGRESRIFeatureServiceLayer::~OGRESRIFeatureServiceLayer()
{
    poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRESRIFeatureServiceLayer::ResetReading()
{
    poDS->MyResetReading();
    nFeaturesRead = 0;
    nLastFID = 0;
    bOtherPage = false;
    bUseSequentialFID = false;
}

/************************************************************************/
/*                            GetNextFeature()                          */
/************************************************************************/

OGRFeature *OGRESRIFeatureServiceLayer::GetNextFeature()
{
    while (true)
    {
        const bool bWasInFirstPage = !bOtherPage;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        OGRFeature *poSrcFeat = poDS->GetUnderlyingLayer()->GetNextFeature();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        if (poSrcFeat == nullptr)
        {
            if (!poDS->LoadNextPage())
                return nullptr;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
            poSrcFeat = poDS->GetUnderlyingLayer()->GetNextFeature();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
            if (poSrcFeat == nullptr)
                return nullptr;
            bOtherPage = true;
            if (bWasInFirstPage && poSrcFeat->GetFID() != 0 &&
                poSrcFeat->GetFID() == nFirstFID)
            {
                // End-less looping
                CPLDebug("ESRIJSON", "Scrolling not working. Stopping");
                delete poSrcFeat;
                return nullptr;
            }
            if (bWasInFirstPage && poSrcFeat->GetFID() == 0 &&
                nLastFID == nFeaturesRead - 1)
            {
                bUseSequentialFID = true;
            }
        }
        if (nFeaturesRead == 0)
            nFirstFID = poSrcFeat->GetFID();

        OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFrom(poSrcFeat);
        if (bUseSequentialFID)
            poFeature->SetFID(nFeaturesRead);
        else
            poFeature->SetFID(poSrcFeat->GetFID());
        nLastFID = poFeature->GetFID();
        nFeaturesRead++;
        delete poSrcFeat;

        if ((m_poFilterGeom == nullptr ||
             FilterGeometry(poFeature->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(poFeature)))
        {
            return poFeature;
        }
        delete poFeature;
    }
}

/************************************************************************/
/*                          TestCapability()                            */
/************************************************************************/

int OGRESRIFeatureServiceLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poAttrQuery == nullptr && m_poFilterGeom == nullptr;
    if (EQUAL(pszCap, OLCFastGetExtent))
        return FALSE;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
    auto poUnderlyingLayer = poDS->GetUnderlyingLayer();
    return poUnderlyingLayer->TestCapability(pszCap);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRESRIFeatureServiceLayer::GetFeatureCount(int bForce)
{
    GIntBig nFeatureCount = -1;
    if (m_poAttrQuery == nullptr && m_poFilterGeom == nullptr)
    {
        CPLString osNewURL =
            CPLURLAddKVP(poDS->GetURL(), "returnCountOnly", "true");
        osNewURL = CPLURLAddKVP(osNewURL, "resultRecordCount", nullptr);
        CPLErrorReset();
        CPLHTTPResult *pResult = CPLHTTPFetch(osNewURL, nullptr);
        if (pResult != nullptr && pResult->nDataLen != 0 &&
            CPLGetLastErrorNo() == 0 && pResult->nStatus == 0)
        {
            const char *pszCount =
                strstr((const char *)pResult->pabyData, "\"count\"");
            if (pszCount)
            {
                pszCount = strchr(pszCount, ':');
                if (pszCount)
                {
                    pszCount++;
                    nFeatureCount = CPLAtoGIntBig(pszCount);
                }
            }
        }
        CPLHTTPDestroyResult(pResult);
    }
    if (nFeatureCount < 0)
        nFeatureCount = OGRLayer::GetFeatureCount(bForce);
    return nFeatureCount;
}

/************************************************************************/
/*                               GetExtent()                            */
/************************************************************************/

OGRErr OGRESRIFeatureServiceLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    OGRErr eErr = OGRERR_FAILURE;
    CPLString osNewURL =
        CPLURLAddKVP(poDS->GetURL(), "returnExtentOnly", "true");
    osNewURL = CPLURLAddKVP(osNewURL, "resultRecordCount", nullptr);
    osNewURL = CPLURLAddKVP(osNewURL, "f", "geojson");
    CPLErrorReset();
    CPLHTTPResult *pResult = CPLHTTPFetch(osNewURL, nullptr);
    if (pResult != nullptr && pResult->nDataLen != 0 &&
        CPLGetLastErrorNo() == 0 && pResult->nStatus == 0)
    {
        const char *pszBBox =
            strstr((const char *)pResult->pabyData, "\"bbox\"");
        if (pszBBox)
        {
            pszBBox = strstr(pszBBox, ":[");
            if (pszBBox)
            {
                pszBBox += 2;
                char **papszTokens = CSLTokenizeString2(pszBBox, ",", 0);
                if (CSLCount(papszTokens) >= 4)
                {
                    psExtent->MinX = CPLAtof(papszTokens[0]);
                    psExtent->MinY = CPLAtof(papszTokens[1]);
                    psExtent->MaxX = CPLAtof(papszTokens[2]);
                    psExtent->MaxY = CPLAtof(papszTokens[3]);
                    eErr = OGRERR_NONE;
                }
                CSLDestroy(papszTokens);
            }
        }
    }
    CPLHTTPDestroyResult(pResult);
    if (eErr == OGRERR_FAILURE)
        eErr = OGRLayer::GetExtent(psExtent, bForce);
    return eErr;
}

/************************************************************************/
/*                      OGRESRIFeatureServiceDataset()                  */
/************************************************************************/

OGRESRIFeatureServiceDataset::OGRESRIFeatureServiceDataset(
    const std::string &osURL, std::unique_ptr<OGRGeoJSONDataSource> &&poFirst,
    GeoJSONSourceType nSrcType)
    : m_osURL(osURL), m_poCurrent(std::move(poFirst)), m_nSrcType(nSrcType)
{
    m_poLayer = std::make_unique<OGRESRIFeatureServiceLayer>(this);
    if (CPLURLGetValue(m_osURL, "resultRecordCount").empty())
    {
        // We assume that if the server sets the exceededTransferLimit, the
        // and resultRecordCount is not set, the number of features returned
        // in our first request is the maximum allowed by the server
        // So set it for following requests.
        m_osURL = CPLURLAddKVP(
            m_osURL, "resultRecordCount",
            CPLSPrintf("%d", static_cast<int>(
                                 m_poCurrent->GetLayer(0)->GetFeatureCount())));
    }
    else
    {
        const int nUserSetRecordCount =
            atoi(CPLURLGetValue(m_osURL, "resultRecordCount"));
        if (nUserSetRecordCount > m_poCurrent->GetLayer(0)->GetFeatureCount())
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "Specified resultRecordCount=%d is greater than "
                "the maximum %d supported by the server",
                nUserSetRecordCount,
                static_cast<int>(m_poCurrent->GetLayer(0)->GetFeatureCount()));
        }
    }
    m_nFirstOffset = CPLAtoGIntBig(CPLURLGetValue(m_osURL, "resultOffset"));
    m_nLastOffset = m_nFirstOffset;
}

/************************************************************************/
/*                           MyResetReading()                           */
/************************************************************************/

bool OGRESRIFeatureServiceDataset::MyResetReading()
{
    if (m_nLastOffset > m_nFirstOffset)
    {
        m_nLastOffset = m_nFirstOffset;
        return LoadPage();
    }

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
    m_poCurrent->GetLayer(0)->ResetReading();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    return true;
}

/************************************************************************/
/*                             LoadNextPage()                           */
/************************************************************************/

bool OGRESRIFeatureServiceDataset::LoadNextPage()
{
    if (!m_poCurrent->HasOtherPages())
        return false;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
    const auto nCurPageFC = m_poCurrent->GetLayer(0)->GetFeatureCount();
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (m_nLastOffset > std::numeric_limits<GIntBig>::max() - nCurPageFC)
        return false;
    m_nLastOffset += nCurPageFC;
    return LoadPage();
}

/************************************************************************/
/*                                 LoadPage()                           */
/************************************************************************/

bool OGRESRIFeatureServiceDataset::LoadPage()
{
    CPLString osNewURL = CPLURLAddKVP(m_osURL, "resultOffset",
                                      CPLSPrintf(CPL_FRMT_GIB, m_nLastOffset));
    auto poDS = std::make_unique<OGRGeoJSONDataSource>();
    GDALOpenInfo oOpenInfo(osNewURL, GA_ReadOnly);
    if (!poDS->Open(&oOpenInfo, m_nSrcType, m_poCurrent->GetJSonFlavor()) ||
        poDS->GetLayerCount() == 0)
    {
        return false;
    }
    m_poCurrent = std::move(poDS);
    return true;
}

/************************************************************************/
/*                        OGRGeoJSONDriverIdentify()                    */
/************************************************************************/

static int OGRGeoJSONDriverIdentifyInternal(GDALOpenInfo *poOpenInfo,
                                            GeoJSONSourceType &nSrcType)
{
    /* -------------------------------------------------------------------- */
    /*      Determine type of data source: text file (.geojson, .json),     */
    /*      Web Service or text passed directly and load data.              */
    /* -------------------------------------------------------------------- */

    nSrcType = GeoJSONGetSourceType(poOpenInfo);
    if (nSrcType == eGeoJSONSourceUnknown)
    {
        const char *pszHeader =
            reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
        if (pszHeader && STARTS_WITH(pszHeader, "{\"properties\":{"))
            return GDAL_IDENTIFY_UNKNOWN;

        return FALSE;
    }
    if (nSrcType == eGeoJSONSourceService &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSON:"))
    {
        return -1;
    }

    // If this looks like a file that can be handled by the STACTA driver,
    // and that one is available, then don't identify the file.
    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if (pszHeader != nullptr &&
        strstr(pszHeader, "\"stac_extensions\"") != nullptr &&
        strstr(pszHeader, "\"tiled-assets\"") != nullptr &&
        GDALGetDriverByName("STACTA") != nullptr)
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                        OGRGeoJSONDriverIdentify()                    */
/************************************************************************/

static int OGRGeoJSONDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType;
    return OGRGeoJSONDriverIdentifyInternal(poOpenInfo, nSrcType);
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset *OGRGeoJSONDriverOpen(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType nSrcType;
    if (OGRGeoJSONDriverIdentifyInternal(poOpenInfo, nSrcType) == FALSE)
    {
        return nullptr;
    }
    return OGRGeoJSONDriverOpenInternal(poOpenInfo, nSrcType, "GeoJSON");
}

/************************************************************************/
/*                     OGRGeoJSONDriverOpenInternal()                   */
/************************************************************************/

GDALDataset *OGRGeoJSONDriverOpenInternal(GDALOpenInfo *poOpenInfo,
                                          GeoJSONSourceType nSrcType,
                                          const char *pszJSonFlavor)
{
    auto poDS = std::make_unique<OGRGeoJSONDataSource>();

    /* -------------------------------------------------------------------- */
    /*      Processing configuration options.                               */
    /* -------------------------------------------------------------------- */

    // TODO: Currently, options are based on environment variables.
    //       This is workaround for not yet implemented Andrey's concept
    //       described in document 'RFC 10: OGR Open Parameters'.

    poDS->SetGeometryTranslation(OGRGeoJSONDataSource::eGeometryPreserve);
    const char *pszOpt = CPLGetConfigOption("GEOMETRY_AS_COLLECTION", nullptr);
    if (nullptr != pszOpt && STARTS_WITH_CI(pszOpt, "YES"))
    {
        poDS->SetGeometryTranslation(
            OGRGeoJSONDataSource::eGeometryAsCollection);
    }

    poDS->SetAttributesTranslation(OGRGeoJSONDataSource::eAttributesPreserve);
    pszOpt = CPLGetConfigOption("ATTRIBUTES_SKIP", nullptr);
    if (nullptr != pszOpt && STARTS_WITH_CI(pszOpt, "YES"))
    {
        poDS->SetAttributesTranslation(OGRGeoJSONDataSource::eAttributesSkip);
    }

    /* -------------------------------------------------------------------- */
    /*      Open and start processing GeoJSON datasource to OGR objects.    */
    /* -------------------------------------------------------------------- */
    if (!poDS->Open(poOpenInfo, nSrcType, pszJSonFlavor))
    {
        poDS.reset();
    }

    if (poDS != nullptr && poDS->HasOtherPages())
    {
        const char *pszFilename = poOpenInfo->pszFilename;
        if (STARTS_WITH_CI(pszFilename, "ESRIJSON:"))
            pszFilename += strlen("ESRIJSON:");
        if (STARTS_WITH(pszFilename, "http") ||
            STARTS_WITH(pszFilename, "/vsimem/"))
        {
            const char *pszFSP = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                                   "FEATURE_SERVER_PAGING");
            const bool bHasResultOffset =
                !CPLURLGetValue(pszFilename, "resultOffset").empty();
            if ((!bHasResultOffset &&
                 (pszFSP == nullptr || CPLTestBool(pszFSP))) ||
                (bHasResultOffset && pszFSP != nullptr && CPLTestBool(pszFSP)))
            {
                return new OGRESRIFeatureServiceDataset(
                    pszFilename, std::move(poDS), nSrcType);
            }
        }
    }

    return poDS.release();
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRGeoJSONDriverCreate(const char *pszName,
                                           int /* nBands */, int /* nXSize */,
                                           int /* nYSize */,
                                           GDALDataType /* eDT */,
                                           char **papszOptions)
{
    OGRGeoJSONDataSource *poDS = new OGRGeoJSONDataSource();

    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRGeoJSONDriverDelete(const char *pszFilename)
{
    if (VSIUnlink(pszFilename) == 0)
    {
        return CE_None;
    }

    CPLDebug("GeoJSON", "Failed to delete \'%s\'", pszFilename);

    return CE_Failure;
}

/************************************************************************/
/*                      OGRGeoJSONDriverStoreContent()                  */
/************************************************************************/

void OGRGeoJSONDriverStoreContent(const char *pszSource, char *pszText)
{
    CPLMutexHolderD(&ghMutex);
    CPLAssert(pszSource);
    CPLAssert(pszText);

    CPLFree(gpszSource);
    CPLFree(gpszText);
    gpszSource = CPLStrdup(pszSource);
    gpszText = pszText;
}

/************************************************************************/
/*                    OGRGeoJSONDriverStealStoredContent()              */
/************************************************************************/

char *OGRGeoJSONDriverStealStoredContent(const char *pszSource)
{
    CPLMutexHolderD(&ghMutex);
    if (gpszSource && EQUAL(pszSource, gpszSource))
    {
        char *pszRet = gpszText;
        CPLFree(gpszSource);
        gpszSource = nullptr;
        gpszText = nullptr;
        return pszRet;
    }
    return nullptr;
}

/************************************************************************/
/*                        OGRGeoJSONDriverUnload()                      */
/************************************************************************/

static void OGRGeoJSONDriverUnload(GDALDriver *)
{
    if (ghMutex)
        CPLDestroyMutex(ghMutex);
    ghMutex = nullptr;
    CPLFree(gpszSource);
    CPLFree(gpszText);
    gpszSource = nullptr;
    gpszText = nullptr;
}

/************************************************************************/
/*                           RegisterOGRGeoJSON()                       */
/************************************************************************/

void RegisterOGRGeoJSON()
{
    if (!GDAL_CHECK_VERSION("OGR/GeoJSON driver"))
        return;

    if (GDALGetDriverByName("GeoJSON") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("GeoJSON");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_REORDER_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS, "Name Type");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GeoJSON");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "json geojson");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/geojson.html");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='FLATTEN_NESTED_ATTRIBUTES' type='boolean' "
        "description='Whether to recursively explore nested objects and "
        "produce flatten OGR attributes' default='NO'/>"
        "  <Option name='NESTED_ATTRIBUTE_SEPARATOR' type='string' "
        "description='Separator between components of nested attributes' "
        "default='_'/>"
        "  <Option name='FEATURE_SERVER_PAGING' type='boolean' "
        "description='Whether to automatically scroll through results with a "
        "ArcGIS Feature Service endpoint'/>"
        "  <Option name='NATIVE_DATA' type='boolean' description='Whether to "
        "store the native JSon representation at FeatureCollection and Feature "
        "level' default='NO'/>"
        "  <Option name='ARRAY_AS_STRING' type='boolean' description='Whether "
        "to expose JSon arrays of strings, integers or reals as a OGR String' "
        "default='NO'/>"
        "  <Option name='DATE_AS_STRING' type='boolean' description='Whether "
        "to expose date/time/date-time content using dedicated OGR "
        "date/time/date-time types or as a OGR String' default='NO'/>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
                              "<CreationOptionList/>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='WRITE_BBOX' type='boolean' description='whether to "
        "write a bbox property with the bounding box of the geometries at the "
        "feature and feature collection level' default='NO'/>"
        "  <Option name='COORDINATE_PRECISION' type='int' description='Number "
        "of decimal for coordinates. Default is 15 for GJ2008 and 7 for "
        "RFC7946'/>"
        "  <Option name='SIGNIFICANT_FIGURES' type='int' description='Number "
        "of significant figures for floating-point values' default='17'/>"
        "  <Option name='NATIVE_DATA' type='string' "
        "description='FeatureCollection level elements.'/>"
        "  <Option name='NATIVE_MEDIA_TYPE' type='string' description='Format "
        "of NATIVE_DATA. Must be \"application/vnd.geo+json\", otherwise "
        "NATIVE_DATA will be ignored.'/>"
        "  <Option name='RFC7946' type='boolean' description='Whether to use "
        "RFC 7946 standard. Otherwise GeoJSON 2008 initial version will be "
        "used' default='NO'/>"
        "  <Option name='WRAPDATELINE' type='boolean' description='Whether to "
        "apply heuristics to split geometries that cross dateline.' "
        "default='YES'/>"
        "  <Option name='WRITE_NAME' type='boolean' description='Whether to "
        "write a &quot;name&quot; property at feature collection level with "
        "layer name' default='YES'/>"
        "  <Option name='DESCRIPTION' type='string' description='(Long) "
        "description to write in a &quot;description&quot; property at feature "
        "collection level'/>"
        "  <Option name='ID_FIELD' type='string' description='Name of the "
        "source field that must be used as the id member of Feature features'/>"
        "  <Option name='ID_TYPE' type='string-select' description='Type of "
        "the id member of Feature features'>"
        "    <Value>AUTO</Value>"
        "    <Value>String</Value>"
        "    <Value>Integer</Value>"
        "  </Option>"
        "  <Option name='ID_GENERATE' type='boolean' "
        "description='Auto-generate feature ids' />"
        "  <Option name='WRITE_NON_FINITE_VALUES' type='boolean' "
        "description='Whether to write NaN / Infinity values' default='NO'/>"
        "  <Option name='AUTODETECT_JSON_STRINGS' type='boolean' "
        "description='Whether to try to interpret string fields as JSON "
        "arrays or objects' default='YES'/>"
        "  <Option name='FOREIGN_MEMBERS_FEATURE' type='string' "
        "description='Extra JSON content to add in each feature as a foreign "
        "members'/>"
        "  <Option name='FOREIGN_MEMBERS_COLLECTION' type='string' "
        "description='Extra JSON content to add to the feature collection as "
        "a foreign members'/>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String IntegerList "
        "Integer64List RealList StringList Date DateTime");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FLUSHCACHE_CONSISTENT_STATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION, "YES");

    poDriver->pfnOpen = OGRGeoJSONDriverOpen;
    poDriver->pfnIdentify = OGRGeoJSONDriverIdentify;
    poDriver->pfnCreate = OGRGeoJSONDriverCreate;
    poDriver->pfnDelete = OGRGeoJSONDriverDelete;
    poDriver->pfnUnloadDriver = OGRGeoJSONDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
