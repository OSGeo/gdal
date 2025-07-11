/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  OGC API interface
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_error.h"
#include "cpl_json.h"
#include "cpl_http.h"
#include "gdal_priv.h"
#include "tilematrixset.hpp"
#include "gdal_utils.h"
#include "ogrsf_frmts.h"
#include "ogr_spatialref.h"

#include "parsexsd.h"

#include <algorithm>
#include <memory>
#include <vector>

// g++ -Wall -Wextra -std=c++11 -Wall -g -fPIC
// frmts/ogcapi/gdalogcapidataset.cpp -shared -o gdal_OGCAPI.so -Iport -Igcore
// -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gml -Iapps -L. -lgdal

extern "C" void GDALRegister_OGCAPI();

#define MEDIA_TYPE_OAPI_3_0 "application/vnd.oai.openapi+json;version=3.0"
#define MEDIA_TYPE_OAPI_3_0_ALT "application/openapi+json;version=3.0"
#define MEDIA_TYPE_JSON "application/json"
#define MEDIA_TYPE_GEOJSON "application/geo+json"
#define MEDIA_TYPE_TEXT_XML "text/xml"
#define MEDIA_TYPE_APPLICATION_XML "application/xml"
#define MEDIA_TYPE_JSON_SCHEMA "application/schema+json"

/************************************************************************/
/* ==================================================================== */
/*                           OGCAPIDataset                              */
/* ==================================================================== */
/************************************************************************/

class OGCAPIDataset final : public GDALDataset
{
    friend class OGCAPIMapWrapperBand;
    friend class OGCAPITilesWrapperBand;
    friend class OGCAPITiledLayer;

    bool m_bMustCleanPersistent = false;
    CPLString m_osRootURL{};
    CPLString m_osUserPwd{};
    CPLString m_osUserQueryParams{};
    GDALGeoTransform m_gt{};

    OGRSpatialReference m_oSRS{};
    CPLString m_osTileData{};

    // Classic OGC API features /items access
    std::unique_ptr<GDALDataset> m_poOAPIFDS{};

    // Map API
    std::unique_ptr<GDALDataset> m_poWMSDS{};

    // Tiles API
    std::vector<std::unique_ptr<GDALDataset>> m_apoDatasetsElementary{};
    std::vector<std::unique_ptr<GDALDataset>> m_apoDatasetsAssembled{};
    std::vector<std::unique_ptr<GDALDataset>> m_apoDatasetsCropped{};

    std::vector<std::unique_ptr<OGRLayer>> m_apoLayers{};

    CPLString BuildURL(const std::string &href) const;
    void SetRootURLFromURL(const std::string &osURL);
    int FigureBands(const std::string &osContentType,
                    const CPLString &osImageURL);

    bool InitFromFile(GDALOpenInfo *poOpenInfo);
    bool InitFromURL(GDALOpenInfo *poOpenInfo);
    bool ProcessScale(const CPLJSONObject &oScaleDenominator,
                      const double dfXMin, const double dfYMin,
                      const double dfXMax, const double dfYMax);
    bool InitFromCollection(GDALOpenInfo *poOpenInfo, CPLJSONDocument &oDoc);
    bool Download(const CPLString &osURL, const char *pszPostContent,
                  const char *pszAccept, CPLString &osResult,
                  CPLString &osContentType, bool bEmptyContentOK,
                  CPLStringList *paosHeaders);

    bool DownloadJSon(const CPLString &osURL, CPLJSONDocument &oDoc,
                      const char *pszPostContent = nullptr,
                      const char *pszAccept = MEDIA_TYPE_GEOJSON
                      ", " MEDIA_TYPE_JSON,
                      CPLStringList *paosHeaders = nullptr);

    std::unique_ptr<GDALDataset>
    OpenTile(const CPLString &osURLPattern, int nMatrix, int nColumn, int nRow,
             bool &bEmptyContent, unsigned int nOpenTileFlags = 0,
             const CPLString &osPrefix = {},
             const char *const *papszOpenOptions = nullptr);

    bool InitWithMapAPI(GDALOpenInfo *poOpenInfo,
                        const CPLJSONObject &oCollection, double dfXMin,
                        double dfYMin, double dfXMax, double dfYMax);
    bool InitWithTilesAPI(GDALOpenInfo *poOpenInfo, const CPLString &osTilesURL,
                          bool bIsMap, double dfXMin, double dfYMin,
                          double dfXMax, double dfYMax, bool bBBOXIsInCRS84,
                          const CPLJSONObject &oJsonCollection);
    bool InitWithCoverageAPI(GDALOpenInfo *poOpenInfo,
                             const CPLString &osTilesURL, double dfXMin,
                             double dfYMin, double dfXMax, double dfYMax,
                             const CPLJSONObject &oJsonCollection);

  protected:
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount,
                     BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    int CloseDependentDatasets() override;

  public:
    OGCAPIDataset() = default;
    ~OGCAPIDataset();

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    const OGRSpatialReference *GetSpatialRef() const override;

    int GetLayerCount() override
    {
        return m_poOAPIFDS ? m_poOAPIFDS->GetLayerCount()
                           : static_cast<int>(m_apoLayers.size());
    }

    OGRLayer *GetLayer(int idx) override
    {
        return m_poOAPIFDS                         ? m_poOAPIFDS->GetLayer(idx)
               : idx >= 0 && idx < GetLayerCount() ? m_apoLayers[idx].get()
                                                   : nullptr;
    }

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
};

/************************************************************************/
/* ==================================================================== */
/*                      OGCAPIMapWrapperBand                            */
/* ==================================================================== */
/************************************************************************/

class OGCAPIMapWrapperBand final : public GDALRasterBand
{
  public:
    OGCAPIMapWrapperBand(OGCAPIDataset *poDS, int nBand);

    virtual GDALRasterBand *GetOverview(int nLevel) override;
    virtual int GetOverviewCount() override;
    virtual GDALColorInterp GetColorInterpretation() override;

  protected:
    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff,
                              void *pImage) override;
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, GSpacing, GSpacing,
                             GDALRasterIOExtraArg *psExtraArg) override;
};

/************************************************************************/
/* ==================================================================== */
/*                     OGCAPITilesWrapperBand                           */
/* ==================================================================== */
/************************************************************************/

class OGCAPITilesWrapperBand final : public GDALRasterBand
{
  public:
    OGCAPITilesWrapperBand(OGCAPIDataset *poDS, int nBand);

    virtual GDALRasterBand *GetOverview(int nLevel) override;
    virtual int GetOverviewCount() override;
    virtual GDALColorInterp GetColorInterpretation() override;

  protected:
    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff,
                              void *pImage) override;
    virtual CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                             GDALDataType, GSpacing, GSpacing,
                             GDALRasterIOExtraArg *psExtraArg) override;
};

/************************************************************************/
/* ==================================================================== */
/*                           OGCAPITiledLayer                           */
/* ==================================================================== */
/************************************************************************/

class OGCAPITiledLayer;

class OGCAPITiledLayerFeatureDefn final : public OGRFeatureDefn
{
    OGCAPITiledLayer *m_poLayer = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(OGCAPITiledLayerFeatureDefn)

  public:
    OGCAPITiledLayerFeatureDefn(OGCAPITiledLayer *poLayer, const char *pszName)
        : OGRFeatureDefn(pszName), m_poLayer(poLayer)
    {
    }

    int GetFieldCount() const override;

    void InvalidateLayer()
    {
        m_poLayer = nullptr;
    }
};

class OGCAPITiledLayer final
    : public OGRLayer,
      public OGRGetNextFeatureThroughRaw<OGCAPITiledLayer>
{
    OGCAPIDataset *m_poDS = nullptr;
    bool m_bFeatureDefnEstablished = false;
    bool m_bEstablishFieldsCalled =
        false;  // prevent recursion in EstablishFields()
    OGCAPITiledLayerFeatureDefn *m_poFeatureDefn = nullptr;
    OGREnvelope m_sEnvelope{};
    std::unique_ptr<GDALDataset> m_poUnderlyingDS{};
    OGRLayer *m_poUnderlyingLayer = nullptr;
    int m_nCurY = 0;
    int m_nCurX = 0;

    CPLString m_osTileURL{};
    bool m_bIsMVT = false;

    const gdal::TileMatrixSet::TileMatrix m_oTileMatrix{};
    bool m_bInvertAxis = false;

    // absolute bounds
    int m_nMinX = 0;
    int m_nMaxX = 0;
    int m_nMinY = 0;
    int m_nMaxY = 0;

    // depends on spatial filter
    int m_nCurMinX = 0;
    int m_nCurMaxX = 0;
    int m_nCurMinY = 0;
    int m_nCurMaxY = 0;

    int GetCoalesceFactorForRow(int nRow) const;
    bool IncrementTileIndices();
    OGRFeature *GetNextRawFeature();
    GDALDataset *OpenTile(int nX, int nY, bool &bEmptyContent);
    void FinalizeFeatureDefnWithLayer(OGRLayer *poUnderlyingLayer);
    OGRFeature *BuildFeature(OGRFeature *poSrcFeature, int nX, int nY);

    CPL_DISALLOW_COPY_ASSIGN(OGCAPITiledLayer)

  protected:
    friend class OGCAPITiledLayerFeatureDefn;
    void EstablishFields();

  public:
    OGCAPITiledLayer(OGCAPIDataset *poDS, bool bInvertAxis,
                     const CPLString &osTileURL, bool bIsMVT,
                     const gdal::TileMatrixSet::TileMatrix &tileMatrix,
                     OGRwkbGeometryType eGeomType);
    ~OGCAPITiledLayer();

    void SetExtent(double dfXMin, double dfYMin, double dfXMax, double dfYMax);
    void SetFields(const std::vector<std::unique_ptr<OGRFieldDefn>> &apoFields);
    void SetMinMaxXY(int minCol, int minRow, int maxCol, int maxRow);

    void ResetReading() override;

    OGRFeatureDefn *GetLayerDefn() override
    {
        return m_poFeatureDefn;
    }

    const char *GetName() override
    {
        return m_poFeatureDefn->GetName();
    }

    OGRwkbGeometryType GetGeomType() override
    {
        return m_poFeatureDefn->GetGeomType();
    }
    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGCAPITiledLayer)

    GIntBig GetFeatureCount(int /* bForce */) override
    {
        return -1;
    }

    OGRErr IGetExtent(int iGeomField, OGREnvelope *psExtent,
                      bool bForce) override;

    OGRErr ISetSpatialFilter(int iGeomField,
                             const OGRGeometry *poGeom) override;

    OGRFeature *GetFeature(GIntBig nFID) override;
    int TestCapability(const char *) override;
};

/************************************************************************/
/*                            GetFieldCount()                           */
/************************************************************************/

int OGCAPITiledLayerFeatureDefn::GetFieldCount() const
{
    if (m_poLayer)
    {
        m_poLayer->EstablishFields();
    }
    return OGRFeatureDefn::GetFieldCount();
}

/************************************************************************/
/*                           ~OGCAPIDataset()                           */
/************************************************************************/

OGCAPIDataset::~OGCAPIDataset()
{
    if (m_bMustCleanPersistent)
    {
        char **papszOptions = CSLSetNameValue(nullptr, "CLOSE_PERSISTENT",
                                              CPLSPrintf("OGCAPI:%p", this));
        CPLHTTPDestroyResult(CPLHTTPFetch(m_osRootURL, papszOptions));
        CSLDestroy(papszOptions);
    }

    OGCAPIDataset::CloseDependentDatasets();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int OGCAPIDataset::CloseDependentDatasets()
{
    if (m_apoDatasetsElementary.empty())
        return false;

    // in this order
    m_apoDatasetsCropped.clear();
    m_apoDatasetsAssembled.clear();
    m_apoDatasetsElementary.clear();
    return true;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr OGCAPIDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    gt = m_gt;
    return CE_None;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *OGCAPIDataset::GetSpatialRef() const
{
    return !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                          CheckContentType()                          */
/************************************************************************/

// We may ask for "application/openapi+json;version=3.0"
// and the server returns "application/openapi+json; charset=utf-8; version=3.0"
static bool CheckContentType(const char *pszGotContentType,
                             const char *pszExpectedContentType)
{
    CPLStringList aosGotTokens(CSLTokenizeString2(pszGotContentType, "; ", 0));
    CPLStringList aosExpectedTokens(
        CSLTokenizeString2(pszExpectedContentType, "; ", 0));
    for (int i = 0; i < aosExpectedTokens.size(); i++)
    {
        bool bFound = false;
        for (int j = 0; j < aosGotTokens.size(); j++)
        {
            if (EQUAL(aosExpectedTokens[i], aosGotTokens[j]))
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
            return false;
    }
    return true;
}

/************************************************************************/
/*                              Download()                              */
/************************************************************************/

bool OGCAPIDataset::Download(const CPLString &osURL, const char *pszPostContent,
                             const char *pszAccept, CPLString &osResult,
                             CPLString &osContentType, bool bEmptyContentOK,
                             CPLStringList *paosHeaders)
{
    char **papszOptions = nullptr;
    CPLString osHeaders;
    if (pszAccept)
    {
        osHeaders += "Accept: ";
        osHeaders += pszAccept;
    }
    if (pszPostContent)
    {
        if (!osHeaders.empty())
        {
            osHeaders += "\r\n";
        }
        osHeaders += "Content-Type: application/json";
    }
    if (!osHeaders.empty())
    {
        papszOptions =
            CSLSetNameValue(papszOptions, "HEADERS", osHeaders.c_str());
    }
    if (!m_osUserPwd.empty())
    {
        papszOptions =
            CSLSetNameValue(papszOptions, "USERPWD", m_osUserPwd.c_str());
    }
    m_bMustCleanPersistent = true;
    papszOptions =
        CSLAddString(papszOptions, CPLSPrintf("PERSISTENT=OGCAPI:%p", this));
    CPLString osURLWithQueryParameters(osURL);
    if (!m_osUserQueryParams.empty() &&
        osURL.find('?' + m_osUserQueryParams) == std::string::npos &&
        osURL.find('&' + m_osUserQueryParams) == std::string::npos)
    {
        if (osURL.find('?') == std::string::npos)
        {
            osURLWithQueryParameters += '?';
        }
        else
        {
            osURLWithQueryParameters += '&';
        }
        osURLWithQueryParameters += m_osUserQueryParams;
    }
    if (pszPostContent)
    {
        papszOptions =
            CSLSetNameValue(papszOptions, "POSTFIELDS", pszPostContent);
    }
    CPLHTTPResult *psResult =
        CPLHTTPFetch(osURLWithQueryParameters, papszOptions);
    CSLDestroy(papszOptions);
    if (!psResult)
        return false;

    if (paosHeaders)
    {
        *paosHeaders = CSLDuplicate(psResult->papszHeaders);
    }

    if (psResult->pszErrBuf != nullptr)
    {
        std::string osErrorMsg(psResult->pszErrBuf);
        const char *pszData =
            reinterpret_cast<const char *>(psResult->pabyData);
        if (pszData)
        {
            osErrorMsg += ", ";
            osErrorMsg.append(pszData, CPLStrnlen(pszData, 1000));
        }
        CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMsg.c_str());
        CPLHTTPDestroyResult(psResult);
        return false;
    }

    if (psResult->pszContentType)
        osContentType = psResult->pszContentType;

    if (pszAccept != nullptr)
    {
        bool bFoundExpectedContentType = false;
        if (strstr(pszAccept, "xml") && psResult->pszContentType != nullptr &&
            (CheckContentType(psResult->pszContentType, MEDIA_TYPE_TEXT_XML) ||
             CheckContentType(psResult->pszContentType,
                              MEDIA_TYPE_APPLICATION_XML)))
        {
            bFoundExpectedContentType = true;
        }

        if (strstr(pszAccept, MEDIA_TYPE_JSON_SCHEMA) &&
            psResult->pszContentType != nullptr &&
            (CheckContentType(psResult->pszContentType, MEDIA_TYPE_JSON) ||
             CheckContentType(psResult->pszContentType,
                              MEDIA_TYPE_JSON_SCHEMA)))
        {
            bFoundExpectedContentType = true;
        }

        for (const char *pszMediaType : {
                 MEDIA_TYPE_JSON,
                 MEDIA_TYPE_GEOJSON,
                 MEDIA_TYPE_OAPI_3_0,
             })
        {
            if (strstr(pszAccept, pszMediaType) &&
                psResult->pszContentType != nullptr &&
                CheckContentType(psResult->pszContentType, pszMediaType))
            {
                bFoundExpectedContentType = true;
                break;
            }
        }

        if (!bFoundExpectedContentType)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unexpected Content-Type: %s",
                     psResult->pszContentType ? psResult->pszContentType
                                              : "(null)");
            CPLHTTPDestroyResult(psResult);
            return false;
        }
    }

    if (psResult->pabyData == nullptr)
    {
        osResult.clear();
        if (!bEmptyContentOK)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Empty content returned by server");
            CPLHTTPDestroyResult(psResult);
            return false;
        }
    }
    else
    {
        osResult.assign(reinterpret_cast<const char *>(psResult->pabyData),
                        psResult->nDataLen);
#ifdef DEBUG_VERBOSE
        CPLDebug("OGCAPI", "%s", osResult.c_str());
#endif
    }
    CPLHTTPDestroyResult(psResult);
    return true;
}

/************************************************************************/
/*                           DownloadJSon()                             */
/************************************************************************/

bool OGCAPIDataset::DownloadJSon(const CPLString &osURL, CPLJSONDocument &oDoc,
                                 const char *pszPostContent,
                                 const char *pszAccept,
                                 CPLStringList *paosHeaders)
{
    CPLString osResult;
    CPLString osContentType;
    if (!Download(osURL, pszPostContent, pszAccept, osResult, osContentType,
                  false, paosHeaders))
        return false;
    return oDoc.LoadMemory(osResult);
}

/************************************************************************/
/*                            OpenTile()                                */
/************************************************************************/

std::unique_ptr<GDALDataset>
OGCAPIDataset::OpenTile(const CPLString &osURLPattern, int nMatrix, int nColumn,
                        int nRow, bool &bEmptyContent,
                        unsigned int nOpenTileFlags, const CPLString &osPrefix,
                        const char *const *papszOpenTileOptions)
{
    CPLString osURL(osURLPattern);
    osURL.replaceAll("{tileMatrix}", CPLSPrintf("%d", nMatrix));
    osURL.replaceAll("{tileCol}", CPLSPrintf("%d", nColumn));
    osURL.replaceAll("{tileRow}", CPLSPrintf("%d", nRow));

    CPLString osContentType;
    if (!this->Download(osURL, nullptr, nullptr, m_osTileData, osContentType,
                        true, nullptr))
    {
        return nullptr;
    }

    bEmptyContent = m_osTileData.empty();
    if (bEmptyContent)
        return nullptr;

    const CPLString osTempFile(VSIMemGenerateHiddenFilename("ogcapi"));
    VSIFCloseL(VSIFileFromMemBuffer(osTempFile.c_str(),
                                    reinterpret_cast<GByte *>(&m_osTileData[0]),
                                    m_osTileData.size(), false));

    GDALDataset *result = nullptr;

    if (osPrefix.empty())
        result = GDALDataset::Open(osTempFile.c_str(), nOpenTileFlags, nullptr,
                                   papszOpenTileOptions);
    else
        result =
            GDALDataset::Open((osPrefix + ":" + osTempFile).c_str(),
                              nOpenTileFlags, nullptr, papszOpenTileOptions);

    VSIUnlink(osTempFile);

    return std::unique_ptr<GDALDataset>(result);
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int OGCAPIDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "OGCAPI:"))
        return TRUE;
    if (poOpenInfo->IsExtensionEqualToCI("moaw"))
        return TRUE;
    if (poOpenInfo->IsSingleAllowedDriver("OGCAPI"))
    {
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                            BuildURL()                                */
/************************************************************************/

CPLString OGCAPIDataset::BuildURL(const std::string &href) const
{
    if (!href.empty() && href[0] == '/')
        return m_osRootURL + href;
    return href;
}

/************************************************************************/
/*                         SetRootURLFromURL()                          */
/************************************************************************/

void OGCAPIDataset::SetRootURLFromURL(const std::string &osURL)
{
    const char *pszStr = osURL.c_str();
    const char *pszPtr = pszStr;
    if (STARTS_WITH(pszPtr, "http://"))
        pszPtr += strlen("http://");
    else if (STARTS_WITH(pszPtr, "https://"))
        pszPtr += strlen("https://");
    pszPtr = strchr(pszPtr, '/');
    if (pszPtr)
        m_osRootURL.assign(pszStr, pszPtr - pszStr);
}

/************************************************************************/
/*                          FigureBands()                               */
/************************************************************************/

int OGCAPIDataset::FigureBands(const std::string &osContentType,
                               const CPLString &osImageURL)
{
    int result = 0;

    if (osContentType == "image/png")
    {
        result = 4;
    }
    else if (osContentType == "image/jpeg")
    {
        result = 3;
    }
    else
    {
        // Since we don't know the format download a tile and find out
        bool bEmptyContent = false;
        std::unique_ptr<GDALDataset> dataset =
            OpenTile(osImageURL, 0, 0, 0, bEmptyContent, GDAL_OF_RASTER);

        // Return the bands from the image, if we didn't get an image then assume 3.
        result = dataset ? static_cast<int>(dataset->GetBands().size()) : 3;
    }

    return result;
}

/************************************************************************/
/*                           InitFromFile()                             */
/************************************************************************/

bool OGCAPIDataset::InitFromFile(GDALOpenInfo *poOpenInfo)
{
    CPLJSONDocument oDoc;
    if (!oDoc.Load(poOpenInfo->pszFilename))
        return false;
    auto oProcess = oDoc.GetRoot()["process"];
    if (oProcess.GetType() != CPLJSONObject::Type::String)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find 'process' key in .moaw file");
        return false;
    }

    const CPLString osURLProcess(oProcess.ToString());
    SetRootURLFromURL(osURLProcess);

    GByte *pabyContent = nullptr;
    vsi_l_offset nSize = 0;
    if (!VSIIngestFile(poOpenInfo->fpL, nullptr, &pabyContent, &nSize,
                       1024 * 1024))
        return false;
    CPLString osPostContent(reinterpret_cast<const char *>(pabyContent));
    CPLFree(pabyContent);
    if (!DownloadJSon(osURLProcess.c_str(), oDoc, osPostContent.c_str()))
        return false;

    return InitFromCollection(poOpenInfo, oDoc);
}

/************************************************************************/
/*                        ProcessScale()                          */
/************************************************************************/

bool OGCAPIDataset::ProcessScale(const CPLJSONObject &oScaleDenominator,
                                 const double dfXMin, const double dfYMin,
                                 const double dfXMax, const double dfYMax)

{
    double dfRes = 1e-8;  // arbitrary
    if (oScaleDenominator.IsValid())
    {
        const double dfScaleDenominator = oScaleDenominator.ToDouble();
        constexpr double HALF_CIRCUMFERENCE = 6378137 * M_PI;
        dfRes = dfScaleDenominator / ((HALF_CIRCUMFERENCE / 180) / 0.28e-3);
    }
    if (dfRes == 0.0)
        return false;

    double dfXSize = (dfXMax - dfXMin) / dfRes;
    double dfYSize = (dfYMax - dfYMin) / dfRes;
    while (dfXSize > INT_MAX || dfYSize > INT_MAX)
    {
        dfXSize /= 2;
        dfYSize /= 2;
    }

    nRasterXSize = std::max(1, static_cast<int>(0.5 + dfXSize));
    nRasterYSize = std::max(1, static_cast<int>(0.5 + dfYSize));
    m_gt[0] = dfXMin;
    m_gt[1] = (dfXMax - dfXMin) / nRasterXSize;
    m_gt[3] = dfYMax;
    m_gt[5] = -(dfYMax - dfYMin) / nRasterYSize;

    return true;
}

/************************************************************************/
/*                        InitFromCollection()                          */
/************************************************************************/

bool OGCAPIDataset::InitFromCollection(GDALOpenInfo *poOpenInfo,
                                       CPLJSONDocument &oDoc)
{
    const CPLJSONObject oRoot = oDoc.GetRoot();
    auto osTitle = oRoot.GetString("title");
    if (!osTitle.empty())
    {
        SetMetadataItem("TITLE", osTitle.c_str());
    }

    auto oLinks = oRoot.GetArray("links");
    if (!oLinks.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing links");
        return false;
    }
    auto oBboxes = oRoot["extent"]["spatial"]["bbox"].ToArray();
    if (oBboxes.Size() != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing bbox");
        return false;
    }
    auto oBbox = oBboxes[0].ToArray();
    if (oBbox.Size() != 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid bbox");
        return false;
    }
    const bool bBBOXIsInCRS84 =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINX") == nullptr;
    const double dfXMin =
        CPLAtof(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "MINX",
                                     CPLSPrintf("%.17g", oBbox[0].ToDouble())));
    const double dfYMin =
        CPLAtof(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "MINY",
                                     CPLSPrintf("%.17g", oBbox[1].ToDouble())));
    const double dfXMax =
        CPLAtof(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "MAXX",
                                     CPLSPrintf("%.17g", oBbox[2].ToDouble())));
    const double dfYMax =
        CPLAtof(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "MAXY",
                                     CPLSPrintf("%.17g", oBbox[3].ToDouble())));

    auto oScaleDenominator = oRoot["scaleDenominator"];

    if (!ProcessScale(oScaleDenominator, dfXMin, dfYMin, dfXMax, dfYMax))
        return false;

    bool bFoundMap = false;

    CPLString osTilesetsMapURL;
    bool bTilesetsMapURLJson = false;

    CPLString osTilesetsVectorURL;
    bool bTilesetsVectorURLJson = false;

    CPLString osCoverageURL;
    bool bCoverageGeotiff = false;

    CPLString osItemsURL;
    bool bItemsJson = false;

    CPLString osSelfURL;
    bool bSelfJson = false;

    for (const auto &oLink : oLinks)
    {
        const auto osRel = oLink.GetString("rel");
        const auto osType = oLink.GetString("type");
        if ((osRel == "http://www.opengis.net/def/rel/ogc/1.0/map" ||
             osRel == "[ogc-rel:map]") &&
            (osType == "image/png" || osType == "image/jpeg"))
        {
            bFoundMap = true;
        }
        else if (!bTilesetsMapURLJson &&
                 (osRel ==
                      "http://www.opengis.net/def/rel/ogc/1.0/tilesets-map" ||
                  osRel == "[ogc-rel:tilesets-map]"))
        {
            if (osType == MEDIA_TYPE_JSON)
            {
                bTilesetsMapURLJson = true;
                osTilesetsMapURL = BuildURL(oLink["href"].ToString());
            }
            else if (osType.empty())
            {
                osTilesetsMapURL = BuildURL(oLink["href"].ToString());
            }
        }
        else if (!bTilesetsVectorURLJson &&
                 (osRel == "http://www.opengis.net/def/rel/ogc/1.0/"
                           "tilesets-vector" ||
                  osRel == "[ogc-rel:tilesets-vector]"))
        {
            if (osType == MEDIA_TYPE_JSON)
            {
                bTilesetsVectorURLJson = true;
                osTilesetsVectorURL = BuildURL(oLink["href"].ToString());
            }
            else if (osType.empty())
            {
                osTilesetsVectorURL = BuildURL(oLink["href"].ToString());
            }
        }
        else if ((osRel == "http://www.opengis.net/def/rel/ogc/1.0/coverage" ||
                  osRel == "[ogc-rel:coverage]") &&
                 (osType == "image/tiff; application=geotiff" ||
                  osType == "application/x-geotiff"))
        {
            if (!bCoverageGeotiff)
            {
                osCoverageURL = BuildURL(oLink["href"].ToString());
                bCoverageGeotiff = true;
            }
        }
        else if ((osRel == "http://www.opengis.net/def/rel/ogc/1.0/coverage" ||
                  osRel == "[ogc-rel:coverage]") &&
                 osType.empty())
        {
            osCoverageURL = BuildURL(oLink["href"].ToString());
        }
        else if (!bItemsJson && osRel == "items")
        {
            if (osType == MEDIA_TYPE_GEOJSON || osType == MEDIA_TYPE_JSON)
            {
                bItemsJson = true;
                osItemsURL = BuildURL(oLink["href"].ToString());
            }
            else if (osType.empty())
            {
                osItemsURL = BuildURL(oLink["href"].ToString());
            }
        }
        else if (!bSelfJson && osRel == "self")
        {
            if (osType == "application/json")
            {
                bSelfJson = true;
                osSelfURL = BuildURL(oLink["href"].ToString());
            }
            else if (osType.empty())
            {
                osSelfURL = BuildURL(oLink["href"].ToString());
            }
        }
    }

    if (!bFoundMap && osTilesetsMapURL.empty() && osTilesetsVectorURL.empty() &&
        osCoverageURL.empty() && osSelfURL.empty() && osItemsURL.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing map, tilesets, coverage or items relation in links");
        return false;
    }

    const char *pszAPI =
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "API", "AUTO");
    if ((EQUAL(pszAPI, "AUTO") || EQUAL(pszAPI, "COVERAGE")) &&
        !osCoverageURL.empty())
    {
        return InitWithCoverageAPI(poOpenInfo, osCoverageURL, dfXMin, dfYMin,
                                   dfXMax, dfYMax, oDoc.GetRoot());
    }
    else if ((EQUAL(pszAPI, "AUTO") || EQUAL(pszAPI, "TILES")) &&
             (!osTilesetsMapURL.empty() || !osTilesetsVectorURL.empty()))
    {
        bool bRet = false;
        if (!osTilesetsMapURL.empty())
            bRet = InitWithTilesAPI(poOpenInfo, osTilesetsMapURL, true, dfXMin,
                                    dfYMin, dfXMax, dfYMax, bBBOXIsInCRS84,
                                    oDoc.GetRoot());
        if (!bRet && !osTilesetsVectorURL.empty())
            bRet = InitWithTilesAPI(poOpenInfo, osTilesetsVectorURL, false,
                                    dfXMin, dfYMin, dfXMax, dfYMax,
                                    bBBOXIsInCRS84, oDoc.GetRoot());
        return bRet;
    }
    else if ((EQUAL(pszAPI, "AUTO") || EQUAL(pszAPI, "MAP")) && bFoundMap)
    {
        return InitWithMapAPI(poOpenInfo, oRoot, dfXMin, dfYMin, dfXMax,
                              dfYMax);
    }
    else if ((EQUAL(pszAPI, "AUTO") || EQUAL(pszAPI, "ITEMS")) &&
             !osSelfURL.empty() && !osItemsURL.empty() &&
             (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0)
    {
        m_poOAPIFDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            ("OAPIF_COLLECTION:" + osSelfURL).c_str(), GDAL_OF_VECTOR));
        if (m_poOAPIFDS)
            return true;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "API %s requested, but not available",
             pszAPI);
    return false;
}

/************************************************************************/
/*                               InitFromURL()                          */
/************************************************************************/

bool OGCAPIDataset::InitFromURL(GDALOpenInfo *poOpenInfo)
{
    const char *pszInitialURL =
        STARTS_WITH_CI(poOpenInfo->pszFilename, "OGCAPI:")
            ? poOpenInfo->pszFilename + strlen("OGCAPI:")
            : poOpenInfo->pszFilename;
    CPLJSONDocument oDoc;
    CPLString osURL(pszInitialURL);
    if (!DownloadJSon(osURL, oDoc))
        return false;

    SetRootURLFromURL(osURL);

    auto oCollections = oDoc.GetRoot().GetArray("collections");
    if (!oCollections.IsValid())
    {
        if (!oDoc.GetRoot().GetArray("extent").IsValid())
        {
            // If there is no "colletions" or "extent" member, then it is
            // perhaps a landing page
            const auto oLinks = oDoc.GetRoot().GetArray("links");
            osURL.clear();
            for (const auto &oLink : oLinks)
            {
                if (oLink["rel"].ToString() == "data" &&
                    oLink["type"].ToString() == MEDIA_TYPE_JSON)
                {
                    osURL = BuildURL(oLink["href"].ToString());
                    break;
                }
                else if (oLink["rel"].ToString() == "data" &&
                         !oLink.GetObj("type").IsValid())
                {
                    osURL = BuildURL(oLink["href"].ToString());
                }
            }
            if (!osURL.empty())
            {
                if (!DownloadJSon(osURL, oDoc))
                    return false;
                oCollections = oDoc.GetRoot().GetArray("collections");
            }
        }

        if (!oCollections.IsValid())
        {
            // This is hopefully a /collections/{id} response
            return InitFromCollection(poOpenInfo, oDoc);
        }
    }

    // This is a /collections response
    CPLStringList aosSubdatasets;
    for (const auto &oCollection : oCollections)
    {
        const auto osTitle = oCollection.GetString("title");
        const auto osLayerDataType = oCollection.GetString("layerDataType");
        // CPLDebug("OGCAPI", "%s: %s", osTitle.c_str(),
        // osLayerDataType.c_str());
        if (!osLayerDataType.empty() &&
            (EQUAL(osLayerDataType.c_str(), "Raster") ||
             EQUAL(osLayerDataType.c_str(), "Coverage")) &&
            (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0)
        {
            continue;
        }
        if (!osLayerDataType.empty() &&
            EQUAL(osLayerDataType.c_str(), "Vector") &&
            (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) == 0)
        {
            continue;
        }
        osURL.clear();
        const auto oLinks = oCollection.GetArray("links");
        for (const auto &oLink : oLinks)
        {
            if (oLink["rel"].ToString() == "self" &&
                oLink["type"].ToString() == "application/json")
            {
                osURL = BuildURL(oLink["href"].ToString());
                break;
            }
            else if (oLink["rel"].ToString() == "self" &&
                     oLink.GetString("type").empty())
            {
                osURL = BuildURL(oLink["href"].ToString());
            }
        }
        if (osURL.empty())
        {
            continue;
        }
        const int nIdx = 1 + aosSubdatasets.size() / 2;
        aosSubdatasets.AddNameValue(CPLSPrintf("SUBDATASET_%d_NAME", nIdx),
                                    CPLSPrintf("OGCAPI:%s", osURL.c_str()));
        aosSubdatasets.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_DESC", nIdx),
            CPLSPrintf("Collection %s", osTitle.c_str()));
    }
    SetMetadata(aosSubdatasets.List(), "SUBDATASETS");

    return true;
}

/************************************************************************/
/*                          SelectImageURL()                            */
/************************************************************************/

static const std::pair<std::string, std::string>
SelectImageURL(const char *const *papszOptionOptions,
               std::map<std::string, std::string> &oMapItemUrls)
{
    // Map IMAGE_FORMATS to their content types. Would be nice if this was
    // globally defined someplace
    const std::map<std::string, std::vector<std::string>>
        oFormatContentTypeMap = {
            {"AUTO",
             {"image/png", "image/jpeg", "image/tiff; application=geotiff"}},
            {"PNG_PREFERRED",
             {"image/png", "image/jpeg", "image/tiff; application=geotiff"}},
            {"JPEG_PREFERRED",
             {"image/jpeg", "image/png", "image/tiff; application=geotiff"}},
            {"PNG", {"image/png"}},
            {"JPEG", {"image/jpeg"}},
            {"GEOTIFF", {"image/tiff; application=geotiff"}}};

    // Get the IMAGE_FORMAT
    const std::string osFormat =
        CSLFetchNameValueDef(papszOptionOptions, "IMAGE_FORMAT", "AUTO");

    // Get a list of content types we will search for in priority order based on IMAGE_FORMAT
    auto iterFormat = oFormatContentTypeMap.find(osFormat);
    if (iterFormat == oFormatContentTypeMap.end())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unknown IMAGE_FORMAT specified: %s", osFormat.c_str());
        return std::pair<std::string, CPLString>();
    }
    std::vector<std::string> oContentTypes = iterFormat->second;

    // For "special" IMAGE_FORMATS we will also accept additional content types
    // specified by the server. Note that this will likely result in having
    // some content types duplicated in the vector but that is fine.
    if (osFormat == "AUTO" || osFormat == "PNG_PREFERRED" ||
        osFormat == "JPEG_PREFERRED")
    {
        std::transform(oMapItemUrls.begin(), oMapItemUrls.end(),
                       std::back_inserter(oContentTypes),
                       [](const auto &pair) -> const std::string &
                       { return pair.first; });
    }

    // Loop over each content type - return the first one we find
    for (auto &oContentType : oContentTypes)
    {
        auto iterContentType = oMapItemUrls.find(oContentType);
        if (iterContentType != oMapItemUrls.end())
        {
            return *iterContentType;
        }
    }

    if (osFormat != "AUTO")
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Server does not support specified IMAGE_FORMAT: %s",
                 osFormat.c_str());
    }
    return std::pair<std::string, CPLString>();
}

/************************************************************************/
/*                        SelectVectorFormatURL()                       */
/************************************************************************/

static const CPLString
SelectVectorFormatURL(const char *const *papszOptionOptions,
                      const CPLString &osMVT_URL,
                      const CPLString &osGEOJSON_URL)
{
    const char *pszFormat =
        CSLFetchNameValueDef(papszOptionOptions, "VECTOR_FORMAT", "AUTO");
    if (EQUAL(pszFormat, "AUTO") || EQUAL(pszFormat, "MVT_PREFERRED"))
        return !osMVT_URL.empty() ? osMVT_URL : osGEOJSON_URL;
    else if (EQUAL(pszFormat, "MVT"))
        return osMVT_URL;
    else if (EQUAL(pszFormat, "GEOJSON"))
        return osGEOJSON_URL;
    else if (EQUAL(pszFormat, "GEOJSON_PREFERRED"))
        return !osGEOJSON_URL.empty() ? osGEOJSON_URL : osMVT_URL;
    return CPLString();
}

/************************************************************************/
/*                          InitWithMapAPI()                            */
/************************************************************************/

bool OGCAPIDataset::InitWithMapAPI(GDALOpenInfo *poOpenInfo,
                                   const CPLJSONObject &oRoot, double dfXMin,
                                   double dfYMin, double dfXMax, double dfYMax)
{
    auto oLinks = oRoot["links"].ToArray();

    // Key - mime type, Value url
    std::map<std::string, std::string> oMapItemUrls;

    for (const auto &oLink : oLinks)
    {
        if (oLink["rel"].ToString() ==
                "http://www.opengis.net/def/rel/ogc/1.0/map" &&
            oLink["type"].IsValid())
        {
            oMapItemUrls[oLink["type"].ToString()] =
                BuildURL(oLink["href"].ToString());
        }
        else
        {
            // For lack of additional information assume we are getting some bytes
            oMapItemUrls["application/octet-stream"] =
                BuildURL(oLink["href"].ToString());
        }
    }

    const std::pair<std::string, std::string> oContentUrlPair =
        SelectImageURL(poOpenInfo->papszOpenOptions, oMapItemUrls);
    const std::string osContentType = oContentUrlPair.first;
    const std::string osImageURL = oContentUrlPair.second;

    if (osImageURL.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find link to tileset items");
        return false;
    }

    int l_nBands = FigureBands(osContentType, osImageURL);
    int nOverviewCount = 0;
    int nLargestDim = std::max(nRasterXSize, nRasterYSize);
    while (nLargestDim > 256)
    {
        nOverviewCount++;
        nLargestDim /= 2;
    }

    m_oSRS.importFromEPSG(4326);
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    const bool bCache = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "CACHE", "YES"));
    const int nMaxConnections = atoi(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "MAX_CONNECTIONS",
                             CPLGetConfigOption("GDAL_MAX_CONNECTIONS", "5")));
    CPLString osWMS_XML;
    char *pszEscapedURL = CPLEscapeString(osImageURL.c_str(), -1, CPLES_XML);
    osWMS_XML.Printf("<GDAL_WMS>"
                     "    <Service name=\"OGCAPIMaps\">"
                     "        <ServerUrl>%s</ServerUrl>"
                     "    </Service>"
                     "    <DataWindow>"
                     "        <UpperLeftX>%.17g</UpperLeftX>"
                     "        <UpperLeftY>%.17g</UpperLeftY>"
                     "        <LowerRightX>%.17g</LowerRightX>"
                     "        <LowerRightY>%.17g</LowerRightY>"
                     "        <SizeX>%d</SizeX>"
                     "        <SizeY>%d</SizeY>"
                     "    </DataWindow>"
                     "    <OverviewCount>%d</OverviewCount>"
                     "    <BlockSizeX>256</BlockSizeX>"
                     "    <BlockSizeY>256</BlockSizeY>"
                     "    <BandsCount>%d</BandsCount>"
                     "    <MaxConnections>%d</MaxConnections>"
                     "    %s"
                     "</GDAL_WMS>",
                     pszEscapedURL, dfXMin, dfYMax, dfXMax, dfYMin,
                     nRasterXSize, nRasterYSize, nOverviewCount, l_nBands,
                     nMaxConnections, bCache ? "<Cache />" : "");
    CPLFree(pszEscapedURL);
    CPLDebug("OGCAPI", "%s", osWMS_XML.c_str());
    m_poWMSDS.reset(
        GDALDataset::Open(osWMS_XML, GDAL_OF_RASTER | GDAL_OF_INTERNAL));
    if (m_poWMSDS == nullptr)
        return false;

    for (int i = 1; i <= m_poWMSDS->GetRasterCount(); i++)
    {
        SetBand(i, new OGCAPIMapWrapperBand(this, i));
    }
    SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    return true;
}

/************************************************************************/
/*                        InitWithCoverageAPI()                         */
/************************************************************************/

bool OGCAPIDataset::InitWithCoverageAPI(GDALOpenInfo *poOpenInfo,
                                        const CPLString &osCoverageURL,
                                        double dfXMin, double dfYMin,
                                        double dfXMax, double dfYMax,
                                        const CPLJSONObject &oJsonCollection)
{
    int l_nBands = 1;
    GDALDataType eDT = GDT_Float32;

    auto oRangeType = oJsonCollection["rangeType"];
    if (!oRangeType.IsValid())
        oRangeType = oJsonCollection["rangetype"];

    auto oDomainSet = oJsonCollection["domainset"];
    if (!oDomainSet.IsValid())
        oDomainSet = oJsonCollection["domainSet"];

    if (!oRangeType.IsValid() || !oDomainSet.IsValid())
    {
        auto oLinks = oJsonCollection.GetArray("links");
        for (const auto &oLink : oLinks)
        {
            const auto osRel = oLink.GetString("rel");
            const auto osType = oLink.GetString("type");
            if (osRel == "http://www.opengis.net/def/rel/ogc/1.0/"
                         "coverage-domainset" &&
                (osType == "application/json" || osType.empty()))
            {
                CPLString osURL = BuildURL(oLink["href"].ToString());
                CPLJSONDocument oDoc;
                if (DownloadJSon(osURL.c_str(), oDoc))
                {
                    oDomainSet = oDoc.GetRoot();
                }
            }
            else if (osRel == "http://www.opengis.net/def/rel/ogc/1.0/"
                              "coverage-rangetype" &&
                     (osType == "application/json" || osType.empty()))
            {
                CPLString osURL = BuildURL(oLink["href"].ToString());
                CPLJSONDocument oDoc;
                if (DownloadJSon(osURL.c_str(), oDoc))
                {
                    oRangeType = oDoc.GetRoot();
                }
            }
        }
    }

    if (oRangeType.IsValid())
    {
        auto oField = oRangeType.GetArray("field");
        if (oField.IsValid())
        {
            l_nBands = oField.Size();
            // Such as in https://maps.gnosis.earth/ogcapi/collections/NaturalEarth:raster:HYP_HR_SR_OB_DR/coverage/rangetype?f=json
            // https://github.com/opengeospatial/coverage-implementation-schema/blob/main/standard/schemas/1.1/json/examples/generalGrid/2D_regular.json
            std::string osDataType =
                oField[0].GetString("encodingInfo/dataType");
            if (osDataType.empty())
            {
                // Older way?
                osDataType = oField[0].GetString("definition");
            }
            static const std::map<std::string, GDALDataType> oMapTypes = {
                // https://edc-oapi.dev.hub.eox.at/oapi/collections/S2L2A
                {"UINT8", GDT_Byte},
                {"INT16", GDT_Int16},
                {"UINT16", GDT_UInt16},
                {"INT32", GDT_Int32},
                {"UINT32", GDT_UInt32},
                {"FLOAT32", GDT_Float32},
                {"FLOAT64", GDT_Float64},
                // https://test.cubewerx.com/cubewerx/cubeserv/demo/ogcapi/Daraa/collections/Daraa_DTED/coverage/rangetype?f=json
                {"ogcType:unsignedByte", GDT_Byte},
                {"ogcType:signedShort", GDT_Int16},
                {"ogcType:unsignedShort", GDT_UInt16},
                {"ogcType:signedInt", GDT_Int32},
                {"ogcType:unsignedInt", GDT_UInt32},
                {"ogcType:float32", GDT_Float32},
                {"ogcType:float64", GDT_Float64},
                {"ogcType:double", GDT_Float64},
            };
            // 08-094r1_SWE_Common_Data_Model_2.0_Submission_Package.pdf page
            // 112
            auto oIter = oMapTypes.find(
                CPLString(osDataType)
                    .replaceAll("http://www.opengis.net/def/dataType/OGC/0/",
                                "ogcType:"));
            if (oIter != oMapTypes.end())
            {
                eDT = oIter->second;
            }
            else
            {
                CPLDebug("OGCAPI", "Unhandled data type: %s",
                         osDataType.c_str());
            }
        }
    }

    CPLString osXAxisName;
    CPLString osYAxisName;
    if (oDomainSet.IsValid())
    {
        auto oAxisLabels = oDomainSet["generalGrid"]["axisLabels"].ToArray();
        if (oAxisLabels.IsValid() && oAxisLabels.Size() >= 2)
        {
            osXAxisName = oAxisLabels[0].ToString();
            osYAxisName = oAxisLabels[1].ToString();
        }

        auto oAxis = oDomainSet["generalGrid"]["axis"].ToArray();
        if (oAxis.IsValid() && oAxis.Size() >= 2)
        {
            double dfXRes = std::abs(oAxis[0].GetDouble("resolution"));
            double dfYRes = std::abs(oAxis[1].GetDouble("resolution"));

            dfXMin = oAxis[0].GetDouble("lowerBound");
            dfXMax = oAxis[0].GetDouble("upperBound");
            dfYMin = oAxis[1].GetDouble("lowerBound");
            dfYMax = oAxis[1].GetDouble("upperBound");

            if (osXAxisName == "Lat")
            {
                std::swap(dfXRes, dfYRes);
                std::swap(dfXMin, dfYMin);
                std::swap(dfXMax, dfYMax);
            }

            double dfXSize = (dfXMax - dfXMin) / dfXRes;
            double dfYSize = (dfYMax - dfYMin) / dfYRes;
            while (dfXSize > INT_MAX || dfYSize > INT_MAX)
            {
                dfXSize /= 2;
                dfYSize /= 2;
            }

            nRasterXSize = std::max(1, static_cast<int>(0.5 + dfXSize));
            nRasterYSize = std::max(1, static_cast<int>(0.5 + dfYSize));
            m_gt[0] = dfXMin;
            m_gt[1] = (dfXMax - dfXMin) / nRasterXSize;
            m_gt[3] = dfYMax;
            m_gt[5] = -(dfYMax - dfYMin) / nRasterYSize;
        }

        OGRSpatialReference oSRS;
        std::string srsName(oDomainSet["generalGrid"].GetString("srsName"));
        bool bSwap = false;

        // Strip of time component, as found in
        // OGCAPI:https://maps.ecere.com/ogcapi/collections/blueMarble
        if (STARTS_WITH(srsName.c_str(),
                        "http://www.opengis.net/def/crs-compound?1=") &&
            srsName.find("&2=http://www.opengis.net/def/crs/OGC/0/") !=
                std::string::npos)
        {
            srsName = srsName.substr(
                strlen("http://www.opengis.net/def/crs-compound?1="));
            srsName.resize(srsName.find("&2="));
        }

        if (oSRS.SetFromUserInput(
                srsName.c_str(),
                OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) ==
            OGRERR_NONE)
        {
            if (oSRS.EPSGTreatsAsLatLong() ||
                oSRS.EPSGTreatsAsNorthingEasting())
            {
                bSwap = true;
            }
        }
        else if (srsName ==
                 "https://ows.rasdaman.org/def/crs/EPSG/0/4326")  // HACK
        {
            bSwap = true;
        }
        if (bSwap)
        {
            std::swap(osXAxisName, osYAxisName);
        }
    }

    int nOverviewCount = 0;
    int nLargestDim = std::max(nRasterXSize, nRasterYSize);
    while (nLargestDim > 256)
    {
        nOverviewCount++;
        nLargestDim /= 2;
    }

    m_oSRS.importFromEPSG(4326);
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    CPLString osCoverageURLModified(osCoverageURL);
    if (osCoverageURLModified.find('&') == std::string::npos &&
        osCoverageURLModified.find('?') == std::string::npos)
    {
        osCoverageURLModified += '?';
    }
    else
    {
        osCoverageURLModified += '&';
    }

    if (!osXAxisName.empty() && !osYAxisName.empty())
    {
        osCoverageURLModified +=
            CPLSPrintf("subset=%s(${minx}:${maxx}),%s(${miny}:${maxy})&"
                       "scaleSize=%s(${width}),%s(${height})",
                       osXAxisName.c_str(), osYAxisName.c_str(),
                       osXAxisName.c_str(), osYAxisName.c_str());
    }
    else
    {
        // FIXME
        osCoverageURLModified += "bbox=${minx},${miny},${maxx},${maxy}&"
                                 "scaleSize=Lat(${height}),Long(${width})";
    }

    const bool bCache = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "CACHE", "YES"));
    const int nMaxConnections = atoi(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "MAX_CONNECTIONS",
                             CPLGetConfigOption("GDAL_MAX_CONNECTIONS", "5")));
    CPLString osWMS_XML;
    char *pszEscapedURL = CPLEscapeString(osCoverageURLModified, -1, CPLES_XML);
    std::string osAccept("<Accept>image/tiff;application=geotiff</Accept>");
    osWMS_XML.Printf("<GDAL_WMS>"
                     "    <Service name=\"OGCAPICoverage\">"
                     "        <ServerUrl>%s</ServerUrl>"
                     "    </Service>"
                     "    <DataWindow>"
                     "        <UpperLeftX>%.17g</UpperLeftX>"
                     "        <UpperLeftY>%.17g</UpperLeftY>"
                     "        <LowerRightX>%.17g</LowerRightX>"
                     "        <LowerRightY>%.17g</LowerRightY>"
                     "        <SizeX>%d</SizeX>"
                     "        <SizeY>%d</SizeY>"
                     "    </DataWindow>"
                     "    <OverviewCount>%d</OverviewCount>"
                     "    <BlockSizeX>256</BlockSizeX>"
                     "    <BlockSizeY>256</BlockSizeY>"
                     "    <BandsCount>%d</BandsCount>"
                     "    <DataType>%s</DataType>"
                     "    <MaxConnections>%d</MaxConnections>"
                     "    %s"
                     "    %s"
                     "</GDAL_WMS>",
                     pszEscapedURL, dfXMin, dfYMax, dfXMax, dfYMin,
                     nRasterXSize, nRasterYSize, nOverviewCount, l_nBands,
                     GDALGetDataTypeName(eDT), nMaxConnections,
                     osAccept.c_str(), bCache ? "<Cache />" : "");
    CPLFree(pszEscapedURL);
    CPLDebug("OGCAPI", "%s", osWMS_XML.c_str());
    m_poWMSDS.reset(
        GDALDataset::Open(osWMS_XML, GDAL_OF_RASTER | GDAL_OF_INTERNAL));
    if (m_poWMSDS == nullptr)
        return false;

    for (int i = 1; i <= m_poWMSDS->GetRasterCount(); i++)
    {
        SetBand(i, new OGCAPIMapWrapperBand(this, i));
    }
    SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    return true;
}

/************************************************************************/
/*                      OGCAPIMapWrapperBand()                          */
/************************************************************************/

OGCAPIMapWrapperBand::OGCAPIMapWrapperBand(OGCAPIDataset *poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = poDSIn->m_poWMSDS->GetRasterBand(nBand)->GetRasterDataType();
    poDSIn->m_poWMSDS->GetRasterBand(nBand)->GetBlockSize(&nBlockXSize,
                                                          &nBlockYSize);
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr OGCAPIMapWrapperBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                        void *pImage)
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    return poGDS->m_poWMSDS->GetRasterBand(nBand)->ReadBlock(
        nBlockXOff, nBlockYOff, pImage);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr OGCAPIMapWrapperBand::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    return poGDS->m_poWMSDS->GetRasterBand(nBand)->RasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nPixelSpace, nLineSpace, psExtraArg);
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int OGCAPIMapWrapperBand::GetOverviewCount()
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    return poGDS->m_poWMSDS->GetRasterBand(nBand)->GetOverviewCount();
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand *OGCAPIMapWrapperBand::GetOverview(int nLevel)
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    return poGDS->m_poWMSDS->GetRasterBand(nBand)->GetOverview(nLevel);
}

/************************************************************************/
/*                   GetColorInterpretation()                           */
/************************************************************************/

GDALColorInterp OGCAPIMapWrapperBand::GetColorInterpretation()
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    // The WMS driver returns Grey-Alpha for 2 band, RGB(A) for 3 or 4 bands
    // Restrict that behavior to Byte only data.
    if (eDataType == GDT_Byte)
        return poGDS->m_poWMSDS->GetRasterBand(nBand)->GetColorInterpretation();
    return GCI_Undefined;
}

/************************************************************************/
/*                           ParseXMLSchema()                           */
/************************************************************************/

static bool
ParseXMLSchema(const std::string &osURL,
               std::vector<std::unique_ptr<OGRFieldDefn>> &apoFields,
               OGRwkbGeometryType &eGeomType)
{
    CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);

    std::vector<GMLFeatureClass *> apoClasses;
    bool bFullyUnderstood = false;
    bool bUseSchemaImports = false;
    bool bHaveSchema = GMLParseXSD(osURL.c_str(), bUseSchemaImports, apoClasses,
                                   bFullyUnderstood);
    if (bHaveSchema && apoClasses.size() == 1)
    {
        auto poGMLFeatureClass = apoClasses[0];
        if (poGMLFeatureClass->GetGeometryPropertyCount() == 1 &&
            poGMLFeatureClass->GetGeometryProperty(0)->GetType() != wkbUnknown)
        {
            eGeomType = static_cast<OGRwkbGeometryType>(
                poGMLFeatureClass->GetGeometryProperty(0)->GetType());
        }

        const int nPropertyCount = poGMLFeatureClass->GetPropertyCount();
        for (int iField = 0; iField < nPropertyCount; iField++)
        {
            const auto poProperty = poGMLFeatureClass->GetProperty(iField);
            OGRFieldSubType eSubType = OFSTNone;
            const OGRFieldType eFType =
                GML_GetOGRFieldType(poProperty->GetType(), eSubType);

            const char *pszName = poProperty->GetName();
            auto poField = std::make_unique<OGRFieldDefn>(pszName, eFType);
            poField->SetSubType(eSubType);
            apoFields.emplace_back(std::move(poField));
        }
        delete poGMLFeatureClass;
        return true;
    }

    for (auto poFeatureClass : apoClasses)
        delete poFeatureClass;

    return false;
}

/************************************************************************/
/*                         InitWithTilesAPI()                           */
/************************************************************************/

bool OGCAPIDataset::InitWithTilesAPI(GDALOpenInfo *poOpenInfo,
                                     const CPLString &osTilesURL, bool bIsMap,
                                     double dfXMin, double dfYMin,
                                     double dfXMax, double dfYMax,
                                     bool bBBOXIsInCRS84,
                                     const CPLJSONObject &oJsonCollection)
{
    CPLJSONDocument oDoc;
    if (!DownloadJSon(osTilesURL.c_str(), oDoc))
        return false;

    auto oTilesets = oDoc.GetRoot()["tilesets"].ToArray();
    if (oTilesets.Size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find tilesets");
        return false;
    }
    const char *pszRequiredTileMatrixSet =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEMATRIXSET");
    const char *pszPreferredTileMatrixSet = CSLFetchNameValue(
        poOpenInfo->papszOpenOptions, "PREFERRED_TILEMATRIXSET");
    CPLString osTilesetURL;
    for (const auto &oTileset : oTilesets)
    {
        const auto oTileMatrixSetURI = oTileset.GetString("tileMatrixSetURI");
        const auto oLinks = oTileset.GetArray("links");
        if (bIsMap)
        {
            if (oTileset.GetString("dataType") != "map")
                continue;
        }
        else
        {
            if (oTileset.GetString("dataType") != "vector")
                continue;
        }
        if (!oLinks.IsValid())
        {
            CPLDebug("OGCAPI", "Missing links for a tileset");
            continue;
        }
        if (pszRequiredTileMatrixSet != nullptr &&
            oTileMatrixSetURI.find(pszRequiredTileMatrixSet) ==
                std::string::npos)
        {
            continue;
        }
        CPLString osCandidateTilesetURL;
        for (const auto &oLink : oLinks)
        {
            if (oLink["rel"].ToString() == "self")
            {
                const auto osType = oLink["type"].ToString();
                if (osType == MEDIA_TYPE_JSON)
                {
                    osCandidateTilesetURL = BuildURL(oLink["href"].ToString());
                    break;
                }
                else if (osType.empty())
                {
                    osCandidateTilesetURL = BuildURL(oLink["href"].ToString());
                }
            }
        }
        if (pszRequiredTileMatrixSet != nullptr)
        {
            osTilesetURL = std::move(osCandidateTilesetURL);
        }
        else if (pszPreferredTileMatrixSet != nullptr &&
                 !osCandidateTilesetURL.empty() &&
                 (oTileMatrixSetURI.find(pszPreferredTileMatrixSet) !=
                  std::string::npos))
        {
            osTilesetURL = std::move(osCandidateTilesetURL);
        }
        else if (oTileMatrixSetURI.find("WorldCRS84Quad") != std::string::npos)
        {
            osTilesetURL = std::move(osCandidateTilesetURL);
        }
        else if (osTilesetURL.empty())
        {
            osTilesetURL = std::move(osCandidateTilesetURL);
        }
    }
    if (osTilesetURL.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find tilematrixset");
        return false;
    }

    // Download and parse selected tileset definition
    if (!DownloadJSon(osTilesetURL.c_str(), oDoc))
        return false;

    const auto oLinks = oDoc.GetRoot().GetArray("links");
    if (!oLinks.IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing links for tileset");
        return false;
    }

    // Key - mime type, Value url
    std::map<std::string, std::string> oMapItemUrls;
    CPLString osMVT_URL;
    CPLString osGEOJSON_URL;
    CPLString osTilingSchemeURL;
    bool bTilingSchemeURLJson = false;

    for (const auto &oLink : oLinks)
    {
        const auto osRel = oLink.GetString("rel");
        const auto osType = oLink.GetString("type");

        if (!bTilingSchemeURLJson &&
            osRel == "http://www.opengis.net/def/rel/ogc/1.0/tiling-scheme")
        {
            if (osType == MEDIA_TYPE_JSON)
            {
                bTilingSchemeURLJson = true;
                osTilingSchemeURL = BuildURL(oLink["href"].ToString());
            }
            else if (osType.empty())
            {
                osTilingSchemeURL = BuildURL(oLink["href"].ToString());
            }
        }
        else if (bIsMap)
        {
            if (osRel == "item" && !osType.empty())
            {
                oMapItemUrls[osType] = BuildURL(oLink["href"].ToString());
            }
            else if (osRel == "item")
            {
                // For lack of additional information assume we are getting some bytes
                oMapItemUrls["application/octet-stream"] =
                    BuildURL(oLink["href"].ToString());
            }
        }
        else
        {
            if (osRel == "item" &&
                osType == "application/vnd.mapbox-vector-tile")
            {
                osMVT_URL = BuildURL(oLink["href"].ToString());
            }
            else if (osRel == "item" && osType == "application/geo+json")
            {
                osGEOJSON_URL = BuildURL(oLink["href"].ToString());
            }
        }
    }

    if (osTilingSchemeURL.empty())
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Cannot find http://www.opengis.net/def/rel/ogc/1.0/tiling-scheme");
        return false;
    }

    // Parse tile matrix set limits.
    const auto oTileMatrixSetLimits =
        oDoc.GetRoot().GetArray("tileMatrixSetLimits");

    struct Limits
    {
        int minTileRow;
        int maxTileRow;
        int minTileCol;
        int maxTileCol;
    };

    std::map<CPLString, Limits> oMapTileMatrixSetLimits;
    if (CPLTestBool(
            CPLGetConfigOption("GDAL_OGCAPI_TILEMATRIXSET_LIMITS", "YES")))
    {
        for (const auto &jsonLimit : oTileMatrixSetLimits)
        {
            const auto osTileMatrix = jsonLimit.GetString("tileMatrix");
            if (!osTileMatrix.empty())
            {
                Limits limits;
                limits.minTileRow = jsonLimit.GetInteger("minTileRow");
                limits.maxTileRow = jsonLimit.GetInteger("maxTileRow");
                limits.minTileCol = jsonLimit.GetInteger("minTileCol");
                limits.maxTileCol = jsonLimit.GetInteger("maxTileCol");
                if (limits.minTileRow > limits.maxTileRow)
                    continue;  // shouldn't happen on valid data
                oMapTileMatrixSetLimits[osTileMatrix] = limits;
            }
        }
    }

    const std::pair<std::string, std::string> oContentUrlPair =
        SelectImageURL(poOpenInfo->papszOpenOptions, oMapItemUrls);
    const std::string osContentType = oContentUrlPair.first;
    const std::string osRasterURL = oContentUrlPair.second;

    const CPLString osVectorURL = SelectVectorFormatURL(
        poOpenInfo->papszOpenOptions, osMVT_URL, osGEOJSON_URL);
    if (osRasterURL.empty() && osVectorURL.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find link to PNG, JPEG, MVT or GeoJSON tiles");
        return false;
    }

    for (const char *pszNeedle : {"{tileMatrix}", "{tileRow}", "{tileCol}"})
    {
        if (!osRasterURL.empty() &&
            osRasterURL.find(pszNeedle) == std::string::npos)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s missing in tile URL %s",
                     pszNeedle, osRasterURL.c_str());
            return false;
        }
        if (!osVectorURL.empty() &&
            osVectorURL.find(pszNeedle) == std::string::npos)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s missing in tile URL %s",
                     pszNeedle, osVectorURL.c_str());
            return false;
        }
    }

    // Download and parse tile matrix set definition
    if (!DownloadJSon(osTilingSchemeURL.c_str(), oDoc, nullptr,
                      MEDIA_TYPE_JSON))
        return false;

    auto tms = gdal::TileMatrixSet::parse(oDoc.SaveAsString().c_str());
    if (tms == nullptr)
        return false;

    if (m_oSRS.SetFromUserInput(
            tms->crs().c_str(),
            OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
        OGRERR_NONE)
        return false;
    const bool bInvertAxis = m_oSRS.EPSGTreatsAsLatLong() != FALSE ||
                             m_oSRS.EPSGTreatsAsNorthingEasting() != FALSE;
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    bool bFoundSomething = false;
    if (!osVectorURL.empty() && (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0)
    {
        const auto osVectorType = oJsonCollection.GetString("vectorType");
        OGRwkbGeometryType eGeomType = wkbUnknown;
        if (osVectorType == "Points")
            eGeomType = wkbPoint;
        else if (osVectorType == "Lines")
            eGeomType = wkbMultiLineString;
        else if (osVectorType == "Polygons")
            eGeomType = wkbMultiPolygon;

        CPLString osXMLSchemaURL;
        for (const auto &oLink : oJsonCollection.GetArray("links"))
        {
            if (oLink["rel"].ToString() == "describedBy" &&
                oLink["type"].ToString() == "text/xml")
            {
                osXMLSchemaURL = BuildURL(oLink["href"].ToString());
            }
        }

        std::vector<std::unique_ptr<OGRFieldDefn>> apoFields;
        bool bGotSchema = false;
        if (!osXMLSchemaURL.empty())
        {
            bGotSchema = ParseXMLSchema(osXMLSchemaURL, apoFields, eGeomType);
        }

        for (const auto &tileMatrix : tms->tileMatrixList())
        {
            const double dfOriX =
                bInvertAxis ? tileMatrix.mTopLeftY : tileMatrix.mTopLeftX;
            const double dfOriY =
                bInvertAxis ? tileMatrix.mTopLeftX : tileMatrix.mTopLeftY;

            auto oLimitsIter = oMapTileMatrixSetLimits.find(tileMatrix.mId);
            if (!oMapTileMatrixSetLimits.empty() &&
                oLimitsIter == oMapTileMatrixSetLimits.end())
            {
                // Tile matrix level not in known limits
                continue;
            }
            int minCol = std::max(
                0, static_cast<int>((dfXMin - dfOriX) / tileMatrix.mResX /
                                    tileMatrix.mTileWidth));
            int maxCol =
                std::min(tileMatrix.mMatrixWidth - 1,
                         static_cast<int>((dfXMax - dfOriX) / tileMatrix.mResX /
                                          tileMatrix.mTileWidth));
            int minRow = std::max(
                0, static_cast<int>((dfOriY - dfYMax) / tileMatrix.mResY /
                                    tileMatrix.mTileHeight));
            int maxRow =
                std::min(tileMatrix.mMatrixHeight - 1,
                         static_cast<int>((dfOriY - dfYMin) / tileMatrix.mResY /
                                          tileMatrix.mTileHeight));
            if (oLimitsIter != oMapTileMatrixSetLimits.end())
            {
                // Take into account tileMatrixSetLimits
                minCol = std::max(minCol, oLimitsIter->second.minTileCol);
                minRow = std::max(minRow, oLimitsIter->second.minTileRow);
                maxCol = std::min(maxCol, oLimitsIter->second.maxTileCol);
                maxRow = std::min(maxRow, oLimitsIter->second.maxTileRow);
                if (minCol > maxCol || minRow > maxRow)
                {
                    continue;
                }
            }
            auto poLayer =
                std::unique_ptr<OGCAPITiledLayer>(new OGCAPITiledLayer(
                    this, bInvertAxis, osVectorURL, osVectorURL == osMVT_URL,
                    tileMatrix, eGeomType));
            poLayer->SetMinMaxXY(minCol, minRow, maxCol, maxRow);
            poLayer->SetExtent(dfXMin, dfYMin, dfXMax, dfYMax);
            if (bGotSchema)
                poLayer->SetFields(apoFields);
            m_apoLayers.emplace_back(std::move(poLayer));
        }

        bFoundSomething = true;
    }

    if (!osRasterURL.empty() && (poOpenInfo->nOpenFlags & GDAL_OF_RASTER) != 0)
    {
        if (bBBOXIsInCRS84)
        {
            // Reproject the extent if needed
            OGRSpatialReference oCRS84;
            oCRS84.importFromEPSG(4326);
            oCRS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            auto poCT = std::unique_ptr<OGRCoordinateTransformation>(
                OGRCreateCoordinateTransformation(&oCRS84, &m_oSRS));
            if (poCT)
            {
                poCT->TransformBounds(dfXMin, dfYMin, dfXMax, dfYMax, &dfXMin,
                                      &dfYMin, &dfXMax, &dfYMax, 21);
            }
        }

        const bool bCache = CPLTestBool(
            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "CACHE", "YES"));
        const int nMaxConnections = atoi(CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "MAX_CONNECTIONS",
            CPLGetConfigOption("GDAL_WMS_MAX_CONNECTIONS", "5")));
        const char *pszTileMatrix =
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEMATRIX");

        int l_nBands = FigureBands(osContentType, osRasterURL);

        for (const auto &tileMatrix : tms->tileMatrixList())
        {
            if (pszTileMatrix && !EQUAL(tileMatrix.mId.c_str(), pszTileMatrix))
            {
                continue;
            }
            if (tileMatrix.mTileWidth == 0 ||
                tileMatrix.mMatrixWidth > INT_MAX / tileMatrix.mTileWidth ||
                tileMatrix.mTileHeight == 0 ||
                tileMatrix.mMatrixHeight > INT_MAX / tileMatrix.mTileHeight)
            {
                // Too resoluted for GDAL limits
                break;
            }
            auto oLimitsIter = oMapTileMatrixSetLimits.find(tileMatrix.mId);
            if (!oMapTileMatrixSetLimits.empty() &&
                oLimitsIter == oMapTileMatrixSetLimits.end())
            {
                // Tile matrix level not in known limits
                continue;
            }

            if (dfXMax - dfXMin < tileMatrix.mResX ||
                dfYMax - dfYMin < tileMatrix.mResY)
            {
                // skip levels for which the extent is smaller than the size
                // of one pixel
                continue;
            }

            CPLString osURL(osRasterURL);
            osURL.replaceAll("{tileMatrix}", tileMatrix.mId.c_str());
            osURL.replaceAll("{tileRow}", "${y}");
            osURL.replaceAll("{tileCol}", "${x}");

            const double dfOriX =
                bInvertAxis ? tileMatrix.mTopLeftY : tileMatrix.mTopLeftX;
            const double dfOriY =
                bInvertAxis ? tileMatrix.mTopLeftX : tileMatrix.mTopLeftY;

            const auto CreateWMS_XML =
                [=, &osURL, &tileMatrix](int minRow, int rowCount,
                                         int nCoalesce, double &dfStripMinY,
                                         double &dfStripMaxY)
            {
                int minCol = 0;
                int maxCol = tileMatrix.mMatrixWidth - 1;
                int maxRow = minRow + rowCount - 1;
                double dfStripMinX =
                    dfOriX + minCol * tileMatrix.mTileWidth * tileMatrix.mResX;
                double dfStripMaxX = dfOriX + (maxCol + 1) *
                                                  tileMatrix.mTileWidth *
                                                  tileMatrix.mResX;
                dfStripMaxY =
                    dfOriY - minRow * tileMatrix.mTileHeight * tileMatrix.mResY;
                dfStripMinY = dfOriY - (maxRow + 1) * tileMatrix.mTileHeight *
                                           tileMatrix.mResY;
                CPLString osWMS_XML;
                char *pszEscapedURL = CPLEscapeString(osURL, -1, CPLES_XML);
                osWMS_XML.Printf(
                    "<GDAL_WMS>"
                    "    <Service name=\"TMS\">"
                    "        <ServerUrl>%s</ServerUrl>"
                    "        <TileXMultiplier>%d</TileXMultiplier>"
                    "    </Service>"
                    "    <DataWindow>"
                    "        <UpperLeftX>%.17g</UpperLeftX>"
                    "        <UpperLeftY>%.17g</UpperLeftY>"
                    "        <LowerRightX>%.17g</LowerRightX>"
                    "        <LowerRightY>%.17g</LowerRightY>"
                    "        <TileLevel>0</TileLevel>"
                    "        <TileY>%d</TileY>"
                    "        <SizeX>%d</SizeX>"
                    "        <SizeY>%d</SizeY>"
                    "        <YOrigin>top</YOrigin>"
                    "    </DataWindow>"
                    "    <BlockSizeX>%d</BlockSizeX>"
                    "    <BlockSizeY>%d</BlockSizeY>"
                    "    <BandsCount>%d</BandsCount>"
                    "    <MaxConnections>%d</MaxConnections>"
                    "    %s"
                    "</GDAL_WMS>",
                    pszEscapedURL, nCoalesce, dfStripMinX, dfStripMaxY,
                    dfStripMaxX, dfStripMinY, minRow,
                    (maxCol - minCol + 1) / nCoalesce * tileMatrix.mTileWidth,
                    rowCount * tileMatrix.mTileHeight, tileMatrix.mTileWidth,
                    tileMatrix.mTileHeight, l_nBands, nMaxConnections,
                    bCache ? "<Cache />" : "");
                CPLFree(pszEscapedURL);
                return osWMS_XML;
            };

            auto vmwl = tileMatrix.mVariableMatrixWidthList;
            if (vmwl.empty())
            {
                double dfIgnored1, dfIgnored2;
                CPLString osWMS_XML(CreateWMS_XML(0, tileMatrix.mMatrixHeight,
                                                  1, dfIgnored1, dfIgnored2));
                if (osWMS_XML.empty())
                    continue;
                std::unique_ptr<GDALDataset> poDS(GDALDataset::Open(
                    osWMS_XML, GDAL_OF_RASTER | GDAL_OF_INTERNAL));
                if (!poDS)
                    return false;
                m_apoDatasetsAssembled.emplace_back(std::move(poDS));
            }
            else
            {
                std::sort(vmwl.begin(), vmwl.end(),
                          [](const gdal::TileMatrixSet::TileMatrix::
                                 VariableMatrixWidth &a,
                             const gdal::TileMatrixSet::TileMatrix::
                                 VariableMatrixWidth &b)
                          { return a.mMinTileRow < b.mMinTileRow; });
                std::vector<GDALDatasetH> apoStrippedDS;
                // For each variable matrix width, create a separate WMS dataset
                // with the correspond strip
                for (size_t i = 0; i < vmwl.size(); i++)
                {
                    if (vmwl[i].mCoalesce <= 0 ||
                        (tileMatrix.mMatrixWidth % vmwl[i].mCoalesce) != 0)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid coalesce factor (%d) w.r.t matrix "
                                 "width (%d)",
                                 vmwl[i].mCoalesce, tileMatrix.mMatrixWidth);
                        return false;
                    }
                    {
                        double dfStripMinY = 0;
                        double dfStripMaxY = 0;
                        CPLString osWMS_XML(CreateWMS_XML(
                            vmwl[i].mMinTileRow,
                            vmwl[i].mMaxTileRow - vmwl[i].mMinTileRow + 1,
                            vmwl[i].mCoalesce, dfStripMinY, dfStripMaxY));
                        if (osWMS_XML.empty())
                            continue;
                        if (dfStripMinY < dfYMax && dfStripMaxY > dfYMin)
                        {
                            std::unique_ptr<GDALDataset> poDS(GDALDataset::Open(
                                osWMS_XML, GDAL_OF_RASTER | GDAL_OF_INTERNAL));
                            if (!poDS)
                                return false;
                            m_apoDatasetsElementary.emplace_back(
                                std::move(poDS));
                            apoStrippedDS.emplace_back(GDALDataset::ToHandle(
                                m_apoDatasetsElementary.back().get()));
                        }
                    }

                    // Add a strip for non-coalesced tiles
                    if (i + 1 < vmwl.size() &&
                        vmwl[i].mMaxTileRow + 1 != vmwl[i + 1].mMinTileRow)
                    {
                        double dfStripMinY = 0;
                        double dfStripMaxY = 0;
                        CPLString osWMS_XML(CreateWMS_XML(
                            vmwl[i].mMaxTileRow + 1,
                            vmwl[i + 1].mMinTileRow - vmwl[i].mMaxTileRow - 1,
                            1, dfStripMinY, dfStripMaxY));
                        if (osWMS_XML.empty())
                            continue;
                        if (dfStripMinY < dfYMax && dfStripMaxY > dfYMin)
                        {
                            std::unique_ptr<GDALDataset> poDS(GDALDataset::Open(
                                osWMS_XML, GDAL_OF_RASTER | GDAL_OF_INTERNAL));
                            if (!poDS)
                                return false;
                            m_apoDatasetsElementary.emplace_back(
                                std::move(poDS));
                            apoStrippedDS.emplace_back(GDALDataset::ToHandle(
                                m_apoDatasetsElementary.back().get()));
                        }
                    }
                }

                if (apoStrippedDS.empty())
                    return false;

                // Assemble the strips in a single VRT
                CPLStringList argv;
                argv.AddString("-resolution");
                argv.AddString("highest");
                GDALBuildVRTOptions *psOptions =
                    GDALBuildVRTOptionsNew(argv.List(), nullptr);
                GDALDatasetH hAssembledDS = GDALBuildVRT(
                    "", static_cast<int>(apoStrippedDS.size()),
                    &apoStrippedDS[0], nullptr, psOptions, nullptr);
                GDALBuildVRTOptionsFree(psOptions);
                if (hAssembledDS == nullptr)
                    return false;
                m_apoDatasetsAssembled.emplace_back(
                    GDALDataset::FromHandle(hAssembledDS));
            }

            CPLStringList argv;
            argv.AddString("-of");
            argv.AddString("VRT");
            argv.AddString("-projwin");
            argv.AddString(CPLSPrintf("%.17g", dfXMin));
            argv.AddString(CPLSPrintf("%.17g", dfYMax));
            argv.AddString(CPLSPrintf("%.17g", dfXMax));
            argv.AddString(CPLSPrintf("%.17g", dfYMin));
            GDALTranslateOptions *psOptions =
                GDALTranslateOptionsNew(argv.List(), nullptr);
            GDALDatasetH hCroppedDS = GDALTranslate(
                "", GDALDataset::ToHandle(m_apoDatasetsAssembled.back().get()),
                psOptions, nullptr);
            GDALTranslateOptionsFree(psOptions);
            if (hCroppedDS == nullptr)
                return false;
            m_apoDatasetsCropped.emplace_back(
                GDALDataset::FromHandle(hCroppedDS));

            if (tileMatrix.mResX <= m_gt[1])
                break;
        }
        if (!m_apoDatasetsCropped.empty())
        {
            std::reverse(std::begin(m_apoDatasetsCropped),
                         std::end(m_apoDatasetsCropped));
            nRasterXSize = m_apoDatasetsCropped[0]->GetRasterXSize();
            nRasterYSize = m_apoDatasetsCropped[0]->GetRasterYSize();
            m_apoDatasetsCropped[0]->GetGeoTransform(m_gt);

            for (int i = 1; i <= m_apoDatasetsCropped[0]->GetRasterCount(); i++)
            {
                SetBand(i, new OGCAPITilesWrapperBand(this, i));
            }
            SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

            bFoundSomething = true;
        }
    }

    return bFoundSomething;
}

/************************************************************************/
/*                      OGCAPITilesWrapperBand()                        */
/************************************************************************/

OGCAPITilesWrapperBand::OGCAPITilesWrapperBand(OGCAPIDataset *poDSIn,
                                               int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = poDSIn->m_apoDatasetsCropped[0]
                    ->GetRasterBand(nBand)
                    ->GetRasterDataType();
    poDSIn->m_apoDatasetsCropped[0]->GetRasterBand(nBand)->GetBlockSize(
        &nBlockXSize, &nBlockYSize);
}

/************************************************************************/
/*                            IReadBlock()                              */
/************************************************************************/

CPLErr OGCAPITilesWrapperBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                          void *pImage)
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    return poGDS->m_apoDatasetsCropped[0]->GetRasterBand(nBand)->ReadBlock(
        nBlockXOff, nBlockYOff, pImage);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr OGCAPITilesWrapperBand::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);

    if ((nBufXSize < nXSize || nBufYSize < nYSize) &&
        poGDS->m_apoDatasetsCropped.size() > 1 && eRWFlag == GF_Read)
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    return poGDS->m_apoDatasetsCropped[0]->GetRasterBand(nBand)->RasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nPixelSpace, nLineSpace, psExtraArg);
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

int OGCAPITilesWrapperBand::GetOverviewCount()
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    return static_cast<int>(poGDS->m_apoDatasetsCropped.size() - 1);
}

/************************************************************************/
/*                              GetOverview()                           */
/************************************************************************/

GDALRasterBand *OGCAPITilesWrapperBand::GetOverview(int nLevel)
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    if (nLevel < 0 || nLevel >= GetOverviewCount())
        return nullptr;
    return poGDS->m_apoDatasetsCropped[nLevel + 1]->GetRasterBand(nBand);
}

/************************************************************************/
/*                   GetColorInterpretation()                           */
/************************************************************************/

GDALColorInterp OGCAPITilesWrapperBand::GetColorInterpretation()
{
    OGCAPIDataset *poGDS = cpl::down_cast<OGCAPIDataset *>(poDS);
    return poGDS->m_apoDatasetsCropped[0]
        ->GetRasterBand(nBand)
        ->GetColorInterpretation();
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr OGCAPIDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                int nXSize, int nYSize, void *pData,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eBufType, int nBandCount,
                                BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                                GSpacing nLineSpace, GSpacing nBandSpace,
                                GDALRasterIOExtraArg *psExtraArg)
{
    if (!m_apoDatasetsCropped.empty())
    {
        // Tiles API
        if ((nBufXSize < nXSize || nBufYSize < nYSize) &&
            m_apoDatasetsCropped.size() > 1 && eRWFlag == GF_Read)
        {
            int bTried;
            CPLErr eErr = TryOverviewRasterIO(
                eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                nBufYSize, eBufType, nBandCount, panBandMap, nPixelSpace,
                nLineSpace, nBandSpace, psExtraArg, &bTried);
            if (bTried)
                return eErr;
        }

        return m_apoDatasetsCropped[0]->RasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg);
    }
    else if (m_poWMSDS)
    {
        // Maps API
        return m_poWMSDS->RasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                   nBufXSize, nBufYSize, eBufType, nBandCount,
                                   panBandMap, nPixelSpace, nLineSpace,
                                   nBandSpace, psExtraArg);
    }

    // Should not be hit
    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpace, nLineSpace,
                                  nBandSpace, psExtraArg);
}

/************************************************************************/
/*                         OGCAPITiledLayer()                           */
/************************************************************************/

OGCAPITiledLayer::OGCAPITiledLayer(
    OGCAPIDataset *poDS, bool bInvertAxis, const CPLString &osTileURL,
    bool bIsMVT, const gdal::TileMatrixSet::TileMatrix &tileMatrix,
    OGRwkbGeometryType eGeomType)
    : m_poDS(poDS), m_osTileURL(osTileURL), m_bIsMVT(bIsMVT),
      m_oTileMatrix(tileMatrix), m_bInvertAxis(bInvertAxis)
{
    m_poFeatureDefn = new OGCAPITiledLayerFeatureDefn(
        this, ("Zoom level " + tileMatrix.mId).c_str());
    SetDescription(m_poFeatureDefn->GetName());
    m_poFeatureDefn->SetGeomType(eGeomType);
    if (eGeomType != wkbNone)
    {
        auto poClonedSRS = poDS->m_oSRS.Clone();
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poClonedSRS);
        poClonedSRS->Dereference();
    }
    m_poFeatureDefn->Reference();
    m_osTileURL.replaceAll("{tileMatrix}", tileMatrix.mId.c_str());
}

/************************************************************************/
/*                        ~OGCAPITiledLayer()                           */
/************************************************************************/

OGCAPITiledLayer::~OGCAPITiledLayer()
{
    m_poFeatureDefn->InvalidateLayer();
    m_poFeatureDefn->Release();
}

/************************************************************************/
/*                       GetCoalesceFactorForRow()                      */
/************************************************************************/

int OGCAPITiledLayer::GetCoalesceFactorForRow(int nRow) const
{
    int nCoalesce = 1;
    for (const auto &vmw : m_oTileMatrix.mVariableMatrixWidthList)
    {
        if (nRow >= vmw.mMinTileRow && nRow <= vmw.mMaxTileRow)
        {
            nCoalesce = vmw.mCoalesce;
            break;
        }
    }
    return nCoalesce;
}

/************************************************************************/
/*                         ResetReading()                               */
/************************************************************************/

void OGCAPITiledLayer::ResetReading()
{
    if (m_nCurX == m_nCurMinX && m_nCurY == m_nCurMinY && m_poUnderlyingLayer)
    {
        m_poUnderlyingLayer->ResetReading();
    }
    else
    {
        m_nCurX = m_nCurMinX;
        m_nCurY = m_nCurMinY;
        m_poUnderlyingDS.reset();
        m_poUnderlyingLayer = nullptr;
    }
}

/************************************************************************/
/*                             OpenTile()                               */
/************************************************************************/

GDALDataset *OGCAPITiledLayer::OpenTile(int nX, int nY, bool &bEmptyContent)
{
    int nCoalesce = GetCoalesceFactorForRow(nY);
    if (nCoalesce <= 0)
        return nullptr;
    nX = (nX / nCoalesce) * nCoalesce;

    const char *const *papszOpenOptions = nullptr;
    CPLString poPrefix;
    CPLStringList aosOpenOptions;

    if (m_bIsMVT)
    {
        const double dfOriX =
            m_bInvertAxis ? m_oTileMatrix.mTopLeftY : m_oTileMatrix.mTopLeftX;
        const double dfOriY =
            m_bInvertAxis ? m_oTileMatrix.mTopLeftX : m_oTileMatrix.mTopLeftY;
        aosOpenOptions.SetNameValue(
            "@GEOREF_TOPX",
            CPLSPrintf("%.17g", dfOriX + nX * m_oTileMatrix.mResX *
                                             m_oTileMatrix.mTileWidth));
        aosOpenOptions.SetNameValue(
            "@GEOREF_TOPY",
            CPLSPrintf("%.17g", dfOriY - nY * m_oTileMatrix.mResY *
                                             m_oTileMatrix.mTileHeight));
        aosOpenOptions.SetNameValue(
            "@GEOREF_TILEDIMX",
            CPLSPrintf("%.17g", nCoalesce * m_oTileMatrix.mResX *
                                    m_oTileMatrix.mTileWidth));
        aosOpenOptions.SetNameValue(
            "@GEOREF_TILEDIMY",
            CPLSPrintf("%.17g",
                       m_oTileMatrix.mResY * m_oTileMatrix.mTileWidth));

        papszOpenOptions = aosOpenOptions.List();
        poPrefix = "MVT";
    }

    std::unique_ptr<GDALDataset> dataset = m_poDS->OpenTile(
        m_osTileURL, stoi(m_oTileMatrix.mId), nX, nY, bEmptyContent,
        GDAL_OF_VECTOR, poPrefix, papszOpenOptions);

    return dataset.release();
}

/************************************************************************/
/*                      FinalizeFeatureDefnWithLayer()                  */
/************************************************************************/

void OGCAPITiledLayer::FinalizeFeatureDefnWithLayer(OGRLayer *poUnderlyingLayer)
{
    if (!m_bFeatureDefnEstablished)
    {
        m_bFeatureDefnEstablished = true;
        const auto poSrcFieldDefn = poUnderlyingLayer->GetLayerDefn();
        const int nFieldCount = poSrcFieldDefn->GetFieldCount();
        for (int i = 0; i < nFieldCount; i++)
        {
            m_poFeatureDefn->AddFieldDefn(poSrcFieldDefn->GetFieldDefn(i));
        }
    }
}

/************************************************************************/
/*                            BuildFeature()                            */
/************************************************************************/

OGRFeature *OGCAPITiledLayer::BuildFeature(OGRFeature *poSrcFeature, int nX,
                                           int nY)
{
    int nCoalesce = GetCoalesceFactorForRow(nY);
    if (nCoalesce <= 0)
        return nullptr;
    nX = (nX / nCoalesce) * nCoalesce;

    OGRFeature *poFeature = new OGRFeature(m_poFeatureDefn);
    const GIntBig nFID = nY * m_oTileMatrix.mMatrixWidth + nX +
                         poSrcFeature->GetFID() * m_oTileMatrix.mMatrixWidth *
                             m_oTileMatrix.mMatrixHeight;
    auto poGeom = poSrcFeature->StealGeometry();
    if (poGeom && m_poFeatureDefn->GetGeomType() != wkbUnknown)
    {
        poGeom =
            OGRGeometryFactory::forceTo(poGeom, m_poFeatureDefn->GetGeomType());
    }
    poFeature->SetFrom(poSrcFeature, true);
    poFeature->SetFID(nFID);
    if (poGeom && m_poFeatureDefn->GetGeomFieldCount() > 0)
    {
        poGeom->assignSpatialReference(
            m_poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef());
    }
    poFeature->SetGeometryDirectly(poGeom);
    delete poSrcFeature;
    return poFeature;
}

/************************************************************************/
/*                        IncrementTileIndices()                        */
/************************************************************************/

bool OGCAPITiledLayer::IncrementTileIndices()
{

    const int nCoalesce = GetCoalesceFactorForRow(m_nCurY);
    if (nCoalesce <= 0)
        return false;
    if (m_nCurX / nCoalesce < m_nCurMaxX / nCoalesce)
    {
        m_nCurX += nCoalesce;
    }
    else if (m_nCurY < m_nCurMaxY)
    {
        m_nCurX = m_nCurMinX;
        m_nCurY++;
    }
    else
    {
        m_nCurY = -1;
        return false;
    }
    return true;
}

/************************************************************************/
/*                          GetNextRawFeature()                         */
/************************************************************************/

OGRFeature *OGCAPITiledLayer::GetNextRawFeature()
{
    while (true)
    {
        if (m_poUnderlyingLayer == nullptr)
        {
            if (m_nCurY < 0)
            {
                return nullptr;
            }
            bool bEmptyContent = false;
            m_poUnderlyingDS.reset(OpenTile(m_nCurX, m_nCurY, bEmptyContent));
            if (bEmptyContent)
            {
                if (!IncrementTileIndices())
                    return nullptr;
                continue;
            }
            if (m_poUnderlyingDS == nullptr)
            {
                return nullptr;
            }
            m_poUnderlyingLayer = m_poUnderlyingDS->GetLayer(0);
            if (m_poUnderlyingLayer == nullptr)
            {
                return nullptr;
            }
            FinalizeFeatureDefnWithLayer(m_poUnderlyingLayer);
        }

        auto poSrcFeature = m_poUnderlyingLayer->GetNextFeature();
        if (poSrcFeature != nullptr)
        {
            return BuildFeature(poSrcFeature, m_nCurX, m_nCurY);
        }

        m_poUnderlyingDS.reset();
        m_poUnderlyingLayer = nullptr;

        if (!IncrementTileIndices())
            return nullptr;
    }
}

/************************************************************************/
/*                           GetFeature()                               */
/************************************************************************/

OGRFeature *OGCAPITiledLayer::GetFeature(GIntBig nFID)
{
    if (nFID < 0)
        return nullptr;
    const GIntBig nFIDInTile =
        nFID / (m_oTileMatrix.mMatrixWidth * m_oTileMatrix.mMatrixHeight);
    const GIntBig nTileID =
        nFID % (m_oTileMatrix.mMatrixWidth * m_oTileMatrix.mMatrixHeight);
    const int nY = static_cast<int>(nTileID / m_oTileMatrix.mMatrixWidth);
    const int nX = static_cast<int>(nTileID % m_oTileMatrix.mMatrixWidth);
    bool bEmptyContent = false;
    std::unique_ptr<GDALDataset> poUnderlyingDS(
        OpenTile(nX, nY, bEmptyContent));
    if (poUnderlyingDS == nullptr)
        return nullptr;
    OGRLayer *poUnderlyingLayer = poUnderlyingDS->GetLayer(0);
    if (poUnderlyingLayer == nullptr)
        return nullptr;
    FinalizeFeatureDefnWithLayer(poUnderlyingLayer);
    OGRFeature *poSrcFeature = poUnderlyingLayer->GetFeature(nFIDInTile);
    if (poSrcFeature == nullptr)
        return nullptr;
    return BuildFeature(poSrcFeature, nX, nY);
}

/************************************************************************/
/*                         EstablishFields()                            */
/************************************************************************/

void OGCAPITiledLayer::EstablishFields()
{
    if (!m_bFeatureDefnEstablished && !m_bEstablishFieldsCalled)
    {
        m_bEstablishFieldsCalled = true;

        // Try up to 10 requests in order. We could probably remove that
        // to use just the fallback logic.
        for (int i = 0; i < 10; ++i)
        {
            bool bEmptyContent = false;
            m_poUnderlyingDS.reset(OpenTile(m_nCurX, m_nCurY, bEmptyContent));
            if (bEmptyContent || !m_poUnderlyingDS)
            {
                if (!IncrementTileIndices())
                    break;
                continue;
            }
            m_poUnderlyingLayer = m_poUnderlyingDS->GetLayer(0);
            if (m_poUnderlyingLayer)
            {
                FinalizeFeatureDefnWithLayer(m_poUnderlyingLayer);
                break;
            }
        }

        if (!m_bFeatureDefnEstablished)
        {
            // Try to sample at different locations in the extent
            for (int j = 0; !m_bFeatureDefnEstablished && j < 3; ++j)
            {
                m_nCurY = m_nMinY + (2 * j + 1) * (m_nMaxY - m_nMinY) / 6;
                for (int i = 0; i < 3; ++i)
                {
                    m_nCurX = m_nMinX + (2 * i + 1) * (m_nMaxX - m_nMinX) / 6;
                    bool bEmptyContent = false;
                    m_poUnderlyingDS.reset(
                        OpenTile(m_nCurX, m_nCurY, bEmptyContent));
                    if (bEmptyContent || !m_poUnderlyingDS)
                    {
                        continue;
                    }
                    m_poUnderlyingLayer = m_poUnderlyingDS->GetLayer(0);
                    if (m_poUnderlyingLayer)
                    {
                        FinalizeFeatureDefnWithLayer(m_poUnderlyingLayer);
                        break;
                    }
                }
            }
        }

        if (!m_bFeatureDefnEstablished)
        {
            CPLDebug("OGCAPI", "Could not establish feature definition. No "
                               "valid tile found in sampling done");
        }

        ResetReading();
    }
}

/************************************************************************/
/*                            SetExtent()                               */
/************************************************************************/

void OGCAPITiledLayer::SetExtent(double dfXMin, double dfYMin, double dfXMax,
                                 double dfYMax)
{
    m_sEnvelope.MinX = dfXMin;
    m_sEnvelope.MinY = dfYMin;
    m_sEnvelope.MaxX = dfXMax;
    m_sEnvelope.MaxY = dfYMax;
}

/************************************************************************/
/*                           IGetExtent()                               */
/************************************************************************/

OGRErr OGCAPITiledLayer::IGetExtent(int /* iGeomField */, OGREnvelope *psExtent,
                                    bool /* bForce */)
{
    *psExtent = m_sEnvelope;
    return OGRERR_NONE;
}

/************************************************************************/
/*                         ISetSpatialFilter()                          */
/************************************************************************/

OGRErr OGCAPITiledLayer::ISetSpatialFilter(int iGeomField,
                                           const OGRGeometry *poGeomIn)
{
    const OGRErr eErr = OGRLayer::ISetSpatialFilter(iGeomField, poGeomIn);
    if (eErr == OGRERR_NONE)
    {
        OGREnvelope sEnvelope;
        if (m_poFilterGeom != nullptr)
            sEnvelope = m_sFilterEnvelope;
        else
            sEnvelope = m_sEnvelope;

        const double dfTileDim = m_oTileMatrix.mResX * m_oTileMatrix.mTileWidth;
        const double dfOriX =
            m_bInvertAxis ? m_oTileMatrix.mTopLeftY : m_oTileMatrix.mTopLeftX;
        const double dfOriY =
            m_bInvertAxis ? m_oTileMatrix.mTopLeftX : m_oTileMatrix.mTopLeftY;
        if (sEnvelope.MinX - dfOriX >= -10 * dfTileDim &&
            dfOriY - sEnvelope.MinY >= -10 * dfTileDim &&
            sEnvelope.MaxX - dfOriX <= 10 * dfTileDim &&
            dfOriY - sEnvelope.MaxY <= 10 * dfTileDim)
        {
            m_nCurMinX = std::max(
                m_nMinX,
                static_cast<int>(floor((sEnvelope.MinX - dfOriX) / dfTileDim)));
            m_nCurMinY = std::max(
                m_nMinY,
                static_cast<int>(floor((dfOriY - sEnvelope.MaxY) / dfTileDim)));
            m_nCurMaxX = std::min(
                m_nMaxX,
                static_cast<int>(floor((sEnvelope.MaxX - dfOriX) / dfTileDim)));
            m_nCurMaxY = std::min(
                m_nMaxY,
                static_cast<int>(floor((dfOriY - sEnvelope.MinY) / dfTileDim)));
        }
        else
        {
            m_nCurMinX = m_nMinX;
            m_nCurMinY = m_nMinY;
            m_nCurMaxX = m_nMaxX;
            m_nCurMaxY = m_nMaxY;
        }

        ResetReading();
    }
    return eErr;
}

/************************************************************************/
/*                          TestCapability()                            */
/************************************************************************/

int OGCAPITiledLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCRandomRead))
        return true;
    if (EQUAL(pszCap, OLCFastGetExtent))
        return true;
    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return true;
    if (EQUAL(pszCap, OLCFastSpatialFilter))
        return true;
    return false;
}

/************************************************************************/
/*                            SetMinMaxXY()                             */
/************************************************************************/

void OGCAPITiledLayer::SetMinMaxXY(int minCol, int minRow, int maxCol,
                                   int maxRow)
{
    m_nMinX = minCol;
    m_nMinY = minRow;
    m_nMaxX = maxCol;
    m_nMaxY = maxRow;
    m_nCurMinX = m_nMinX;
    m_nCurMinY = m_nMinY;
    m_nCurMaxX = m_nMaxX;
    m_nCurMaxY = m_nMaxY;
    ResetReading();
}

/************************************************************************/
/*                             SetFields()                              */
/************************************************************************/

void OGCAPITiledLayer::SetFields(
    const std::vector<std::unique_ptr<OGRFieldDefn>> &apoFields)
{
    m_bFeatureDefnEstablished = true;
    for (const auto &poField : apoFields)
    {
        m_poFeatureDefn->AddFieldDefn(poField.get());
    }
}

/************************************************************************/
/*                              Open()                                  */
/************************************************************************/

GDALDataset *OGCAPIDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;
    auto poDS = std::make_unique<OGCAPIDataset>();
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "OGCAPI:") ||
        STARTS_WITH(poOpenInfo->pszFilename, "http://") ||
        STARTS_WITH(poOpenInfo->pszFilename, "https://"))
    {
        if (!poDS->InitFromURL(poOpenInfo))
            return nullptr;
    }
    else
    {
        if (!poDS->InitFromFile(poOpenInfo))
            return nullptr;
    }
    return poDS.release();
}

/************************************************************************/
/*                        GDALRegister_OGCAPI()                         */
/************************************************************************/

void GDALRegister_OGCAPI()

{
    if (GDALGetDriverByName("OGCAPI") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("OGCAPI");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "OGCAPI");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='API' type='string-select' "
        "description='Which API to use to access data' default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>MAP</Value>"
        "       <Value>TILES</Value>"
        "       <Value>COVERAGE</Value>"
        "       <Value>ITEMS</Value>"
        "  </Option>"
        "  <Option name='IMAGE_FORMAT' scope='raster' type='string-select' "
        "description='Which format to use for pixel acquisition' "
        "default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>PNG</Value>"
        "       <Value>PNG_PREFERRED</Value>"
        "       <Value>JPEG</Value>"
        "       <Value>JPEG_PREFERRED</Value>"
        "       <Value>GEOTIFF</Value>"
        "  </Option>"
        "  <Option name='VECTOR_FORMAT' scope='vector' type='string-select' "
        "description='Which format to use for vector data acquisition' "
        "default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>GEOJSON</Value>"
        "       <Value>GEOJSON_PREFERRED</Value>"
        "       <Value>MVT</Value>"
        "       <Value>MVT_PREFERRED</Value>"
        "  </Option>"
        "  <Option name='TILEMATRIXSET' type='string' "
        "description='Identifier of the required tile matrix set'/>"
        "  <Option name='PREFERRED_TILEMATRIXSET' type='string' "
        "description='dentifier of the preferred tile matrix set' "
        "default='WorldCRS84Quad'/>"
        "  <Option name='TILEMATRIX' scope='raster' type='string' "
        "description='Tile matrix identifier.'/>"
        "  <Option name='CACHE' scope='raster' type='boolean' "
        "description='Whether to enable block/tile caching' default='YES'/>"
        "  <Option name='MAX_CONNECTIONS' scope='raster' type='int' "
        "description='Maximum number of connections' default='5'/>"
        "  <Option name='MINX' type='float' "
        "description='Minimum value (in SRS of TileMatrixSet) of X'/>"
        "  <Option name='MINY' type='float' "
        "description='Minimum value (in SRS of TileMatrixSet) of Y'/>"
        "  <Option name='MAXX' type='float' "
        "description='Maximum value (in SRS of TileMatrixSet) of X'/>"
        "  <Option name='MAXY' type='float' "
        "description='Maximum value (in SRS of TileMatrixSet) of Y'/>"
        "</OpenOptionList>");

    poDriver->pfnIdentify = OGCAPIDataset::Identify;
    poDriver->pfnOpen = OGCAPIDataset::Open;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
