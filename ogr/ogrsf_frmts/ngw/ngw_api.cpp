/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018-2025, NextGIS
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/

#include "ogr_ngw.h"

#include "cpl_http.h"

#include <limits>

namespace NGWAPI
{

static std::string GetErrorMessage(const CPLJSONObject &oRoot,
                                   const std::string &osErrorMessage)
{
    if (oRoot.IsValid())
    {
        std::string osErrorMessageInt =
            oRoot.GetString("message", osErrorMessage);
        if (!osErrorMessageInt.empty())
        {
            return osErrorMessageInt;
        }
    }
    return osErrorMessage;
}

bool CheckRequestResult(bool bResult, const CPLJSONObject &oRoot,
                        const std::string &osErrorMessage)
{
    if (!bResult)
    {
        auto osMsg = GetErrorMessage(oRoot, osErrorMessage);

        CPLError(CE_Failure, CPLE_AppDefined,
                 "NGW driver failed to fetch data with error: %s",
                 osMsg.c_str());
        return false;
    }

    return true;
}

static void ReportError(const GByte *pabyData, int nDataLen,
                        const std::string &osErrorMessage)
{
    CPLJSONDocument oResult;
    if (oResult.LoadMemory(pabyData, nDataLen))
    {
        CPLJSONObject oRoot = oResult.GetRoot();
        auto osMsg = GetErrorMessage(oRoot, osErrorMessage);
        CPLError(CE_Failure, CPLE_AppDefined, "%s", osMsg.c_str());
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMessage.c_str());
    }
}

bool CheckSupportedType(bool bIsRaster, const std::string &osType)
{
    //TODO: Add "raster_mosaic", "tileset", "wfsserver_service" and "wmsserver_service"
    if (bIsRaster)
    {
        if (osType == "mapserver_style" || osType == "qgis_vector_style" ||
            osType == "raster_style" || osType == "qgis_raster_style" ||
            osType == "basemap_layer" || osType == "webmap" ||
            osType == "wmsclient_layer" || osType == "raster_layer")
        {
            return true;
        }
    }
    else
    {
        if (osType == "vector_layer" || osType == "postgis_layer")
        {
            return true;
        }
    }
    return false;
}

std::string GetPermissionsURL(const std::string &osUrl,
                              const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/permission";
}

std::string GetResourceURL(const std::string &osUrl,
                           const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId;
}

std::string GetChildrenURL(const std::string &osUrl,
                           const std::string &osResourceId)
{
    return osUrl + "/api/resource/?parent=" + osResourceId;
}

std::string GetFeatureURL(const std::string &osUrl,
                          const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/feature/";
}

std::string GetTMSURL(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl +
           "/api/component/render/"
           "tile?z=${z}&amp;x=${x}&amp;y=${y}&amp;resource=" +
           osResourceId;
}

std::string GetSearchURL(const std::string &osUrl, const std::string &osKey,
                         const std::string &osValue)
{
    return osUrl + "/api/resource/search/?" + osKey + "=" + osValue;
}

std::string
GetFeaturePageURL(const std::string &osUrl, const std::string &osResourceId,
                  GIntBig nStart, int nCount, const std::string &osFields,
                  const std::string &osWhere, const std::string &osSpatialWhere,
                  const std::string &osExtensions, bool IsGeometryIgnored)
{
    std::string osFeatureUrl = GetFeatureURL(osUrl, osResourceId);
    bool bParamAdd = false;
    if (nCount > 0)
    {
        osFeatureUrl += "?offset=" + std::to_string(nStart) +
                        "&limit=" + std::to_string(nCount);
        bParamAdd = true;
    }

    if (!osFields.empty())
    {
        if (bParamAdd)
        {
            osFeatureUrl += "&fields=" + osFields;
        }
        else
        {
            osFeatureUrl += "?fields=" + osFields;
            bParamAdd = true;
        }
    }

    if (!osWhere.empty())
    {
        if (bParamAdd)
        {
            osFeatureUrl += "&" + osWhere;
        }
        else
        {
            osFeatureUrl += "?" + osWhere;
            bParamAdd = true;
        }
    }

    if (!osSpatialWhere.empty())
    {
        if (bParamAdd)
        {
            osFeatureUrl += "&intersects=" + osSpatialWhere;
        }
        else
        {
            osFeatureUrl += "?intersects=" + osSpatialWhere;
            bParamAdd = true;
        }
    }

    if (IsGeometryIgnored)
    {
        if (bParamAdd)
        {
            osFeatureUrl += "&geom=no";
        }
        else
        {
            osFeatureUrl += "?geom=no";
            bParamAdd = true;
        }
    }

    if (bParamAdd)
    {
        osFeatureUrl += "&extensions=" + osExtensions;
    }
    else
    {
        osFeatureUrl += "?extensions=" + osExtensions;
    }

    return osFeatureUrl;
}

std::string GetRouteURL(const std::string &osUrl)
{
    return osUrl + "/api/component/pyramid/route";
}

std::string GetUploadURL(const std::string &osUrl)
{
    return osUrl + "/api/component/file_upload/upload";
}

std::string GetVersionURL(const std::string &osUrl)
{
    return osUrl + "/api/component/pyramid/pkg_version";
}

std::string GetCOGURL(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/cog";
}

bool CheckVersion(const std::string &osVersion, int nMajor, int nMinor,
                  int nPatch)
{
    int nCurrentMajor(0);
    int nCurrentMinor(0);
    int nCurrentPatch(0);

    CPLStringList aosList(CSLTokenizeString2(osVersion.c_str(), ".", 0));
    if (aosList.size() > 2)
    {
        nCurrentMajor = atoi(aosList[0]);
        nCurrentMinor = atoi(aosList[1]);
        nCurrentPatch = atoi(aosList[2]);
    }
    else if (aosList.size() > 1)
    {
        nCurrentMajor = atoi(aosList[0]);
        nCurrentMinor = atoi(aosList[1]);
    }
    else if (aosList.size() > 0)
    {
        nCurrentMajor = atoi(aosList[0]);
    }

    int nCheckVersion = nMajor * 1000 + nMinor * 100 + nPatch;
    int nCurrentVersion =
        nCurrentMajor * 1000 + nCurrentMinor * 100 + nCurrentPatch;
    return nCurrentVersion >= nCheckVersion;
}

Uri ParseUri(const std::string &osUrl)
{
    Uri stOut;
    std::size_t nFound = osUrl.find(":");
    if (nFound == std::string::npos)
    {
        return stOut;
    }

    stOut.osPrefix = osUrl.substr(0, nFound);
    std::string osUrlInt = CPLString(osUrl.substr(nFound + 1)).tolower();

    nFound = osUrlInt.find("/resource/");
    if (nFound == std::string::npos)
    {
        return stOut;
    }

    stOut.osAddress = osUrlInt.substr(0, nFound);

    std::string osResourceId =
        CPLString(osUrlInt.substr(nFound + strlen("/resource/"))).Trim();

    nFound = osResourceId.find('/');
    if (nFound != std::string::npos)
    {
        stOut.osResourceId = osResourceId.substr(0, nFound);
        stOut.osNewResourceName = osResourceId.substr(nFound + 1);
    }
    else
    {
        stOut.osResourceId = std::move(osResourceId);
    }

    return stOut;
}

std::string CreateResource(const std::string &osUrl,
                           const std::string &osPayload,
                           const CPLStringList &aosHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osPayload;

    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);

    aosHTTPOptionsInt.AddString("CUSTOMREQUEST=POST");
    aosHTTPOptionsInt.AddString(osPayloadInt.c_str());
    aosHTTPOptionsInt.AddString(
        "HEADERS=Content-Type: application/json\r\nAccept: */*");

    CPLDebug("NGW", "CreateResource request payload: %s", osPayload.c_str());

    CPLJSONDocument oCreateReq;
    bool bResult =
        oCreateReq.LoadUrl(GetResourceURL(osUrl, ""), aosHTTPOptionsInt);
    std::string osResourceId("-1");
    CPLJSONObject oRoot = oCreateReq.GetRoot();
    if (CheckRequestResult(bResult, oRoot, "CreateResource request failed"))
    {
        osResourceId = oRoot.GetString("id", "-1");
    }
    return osResourceId;
}

bool UpdateResource(const std::string &osUrl, const std::string &osResourceId,
                    const std::string &osPayload,
                    const CPLStringList &aosHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osPayload;

    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);
    aosHTTPOptionsInt.AddString("CUSTOMREQUEST=PUT");
    aosHTTPOptionsInt.AddString(osPayloadInt.c_str());
    aosHTTPOptionsInt.AddString(
        "HEADERS=Content-Type: application/json\r\nAccept: */*");

    CPLDebug("NGW", "UpdateResource request payload: %s", osPayload.c_str());

    CPLHTTPResult *psResult = CPLHTTPFetch(
        GetResourceURL(osUrl, osResourceId).c_str(), aosHTTPOptionsInt);
    bool bResult = false;
    if (psResult)
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;

        // Get error message.
        if (!bResult)
        {
            ReportError(psResult->pabyData, psResult->nDataLen,
                        "UpdateResource request failed");
        }
        CPLHTTPDestroyResult(psResult);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Update resource %s failed",
                 osResourceId.c_str());
    }
    return bResult;
}

bool DeleteResource(const std::string &osUrl, const std::string &osResourceId,
                    const CPLStringList &aosHTTPOptions)
{
    CPLErrorReset();
    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);

    aosHTTPOptionsInt.AddString("CUSTOMREQUEST=DELETE");
    auto osUrlNew = GetResourceURL(osUrl, osResourceId);
    CPLHTTPResult *psResult = CPLHTTPFetch(osUrlNew.c_str(), aosHTTPOptionsInt);
    bool bResult = false;
    if (psResult)
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;
        // Get error message.
        if (!bResult)
        {
            ReportError(psResult->pabyData, psResult->nDataLen,
                        "DeleteResource request failed");
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bResult;
}

bool RenameResource(const std::string &osUrl, const std::string &osResourceId,
                    const std::string &osNewName,
                    const CPLStringList &aosHTTPOptions)
{
    CPLJSONObject oPayload;
    CPLJSONObject oResource("resource", oPayload);
    oResource.Add("display_name", osNewName);
    std::string osPayload = oPayload.Format(CPLJSONObject::PrettyFormat::Plain);

    return UpdateResource(osUrl, osResourceId, osPayload, aosHTTPOptions);
}

OGRwkbGeometryType NGWGeomTypeToOGRGeomType(const std::string &osGeomType)
{
    // http://docs.nextgis.com/docs_ngweb_dev/doc/developer/vector_data_types.html#nextgisweb.feature_layer.interface.GEOM_TYPE
    if (osGeomType == "POINT")
        return wkbPoint;
    else if (osGeomType == "LINESTRING")
        return wkbLineString;
    else if (osGeomType == "POLYGON")
        return wkbPolygon;
    else if (osGeomType == "MULTIPOINT")
        return wkbMultiPoint;
    else if (osGeomType == "MULTILINESTRING")
        return wkbMultiLineString;
    else if (osGeomType == "MULTIPOLYGON")
        return wkbMultiPolygon;
    else if (osGeomType == "POINTZ")
        return wkbPoint25D;
    else if (osGeomType == "LINESTRINGZ")
        return wkbLineString25D;
    else if (osGeomType == "POLYGONZ")
        return wkbPolygon25D;
    else if (osGeomType == "MULTIPOINTZ")
        return wkbMultiPoint25D;
    else if (osGeomType == "MULTILINESTRINGZ")
        return wkbMultiLineString25D;
    else if (osGeomType == "MULTIPOLYGONZ")
        return wkbMultiPolygon25D;
    else
        return wkbUnknown;
}

std::string OGRGeomTypeToNGWGeomType(OGRwkbGeometryType eType)
{
    switch (eType)
    {  // Don't flatten
        case wkbPoint:
            return "POINT";
        case wkbLineString:
            return "LINESTRING";
        case wkbPolygon:
            return "POLYGON";
        case wkbMultiPoint:
            return "MULTIPOINT";
        case wkbMultiLineString:
            return "MULTILINESTRING";
        case wkbMultiPolygon:
            return "MULTIPOLYGON";
        case wkbPoint25D:
            return "POINTZ";
        case wkbLineString25D:
            return "LINESTRINGZ";
        case wkbPolygon25D:
            return "POLYGONZ";
        case wkbMultiPoint25D:
            return "MULTIPOINTZ";
        case wkbMultiLineString25D:
            return "MULTILINESTRINGZ";
        case wkbMultiPolygon25D:
            return "MULTIPOLYGONZ";
        default:
            return "";
    }
}

OGRFieldType NGWFieldTypeToOGRFieldType(const std::string &osFieldType)
{
    // http://docs.nextgis.com/docs_ngweb_dev/doc/developer/vector_data_types.html#nextgisweb.feature_layer.interface.FIELD_TYPE
    if (osFieldType == "INTEGER")
        return OFTInteger;
    else if (osFieldType == "BIGINT")
        return OFTInteger64;
    else if (osFieldType == "REAL")
        return OFTReal;
    else if (osFieldType == "STRING")
        return OFTString;
    else if (osFieldType == "DATE")
        return OFTDate;
    else if (osFieldType == "TIME")
        return OFTTime;
    else if (osFieldType == "DATETIME")
        return OFTDateTime;
    else
        return OFTString;
}

std::string OGRFieldTypeToNGWFieldType(OGRFieldType eType)
{
    switch (eType)
    {
        case OFTInteger:
            return "INTEGER";
        case OFTInteger64:
            return "BIGINT";
        case OFTReal:
            return "REAL";
        case OFTString:
            return "STRING";
        case OFTDate:
            return "DATE";
        case OFTTime:
            return "TIME";
        case OFTDateTime:
            return "DATETIME";
        default:
            return "STRING";
    }
}

Permissions CheckPermissions(const std::string &osUrl,
                             const std::string &osResourceId,
                             const CPLStringList &aosHTTPOptions,
                             bool bReadWrite)
{
    Permissions stOut;
    CPLErrorReset();
    CPLJSONDocument oPermissionReq;

    auto osUrlNew = GetPermissionsURL(osUrl, osResourceId);
    bool bResult = oPermissionReq.LoadUrl(osUrlNew, aosHTTPOptions);

    CPLJSONObject oRoot = oPermissionReq.GetRoot();
    if (CheckRequestResult(bResult, oRoot, "Get permissions failed"))
    {
        stOut.bResourceCanRead = oRoot.GetBool("resource/read", true);
        stOut.bResourceCanCreate = oRoot.GetBool("resource/create", bReadWrite);
        stOut.bResourceCanUpdate = oRoot.GetBool("resource/update", bReadWrite);
        stOut.bResourceCanDelete = oRoot.GetBool("resource/delete", bReadWrite);

        stOut.bDatastructCanRead = oRoot.GetBool("datastruct/read", true);
        stOut.bDatastructCanWrite =
            oRoot.GetBool("datastruct/write", bReadWrite);

        stOut.bDataCanRead = oRoot.GetBool("data/read", true);
        stOut.bDataCanWrite = oRoot.GetBool("data/write", bReadWrite);

        stOut.bMetadataCanRead = oRoot.GetBool("metadata/read", true);
        stOut.bMetadataCanWrite = oRoot.GetBool("metadata/write", bReadWrite);

        CPLErrorReset();  // If we are here no error occurred
        return stOut;
    }

    return stOut;
}

std::string GetFeatureCount(const std::string &osUrl,
                            const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/feature_count";
}

std::string GetLayerExtent(const std::string &osUrl,
                           const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/extent";
}

std::string GetResmetaSuffix(CPLJSONObject::Type eType)
{
    switch (eType)
    {
        case CPLJSONObject::Type::Integer:
        case CPLJSONObject::Type::Long:
            return ".d";
        case CPLJSONObject::Type::Double:
            return ".f";
        default:
            return "";
    }
}

void FillResmeta(const CPLJSONObject &oRoot, char **papszMetadata)
{
    CPLJSONObject oResMeta("resmeta", oRoot);
    CPLJSONObject oResMetaItems("items", oResMeta);
    CPLStringList oaMetadata(papszMetadata, FALSE);
    for (int i = 0; i < oaMetadata.size(); ++i)
    {
        std::string osItem = oaMetadata[i];
        size_t nPos = osItem.find("=");
        if (nPos != std::string::npos)
        {
            std::string osItemName = osItem.substr(0, nPos);
            CPLString osItemValue = osItem.substr(nPos + 1);

            if (osItemName.size() > 2)
            {
                size_t nSuffixPos = osItemName.size() - 2;
                std::string osSuffix = osItemName.substr(nSuffixPos);
                if (osSuffix == ".d")
                {
                    GInt64 nVal = CPLAtoGIntBig(osItemValue.c_str());
                    oResMetaItems.Add(osItemName.substr(0, nSuffixPos), nVal);
                    continue;
                }

                if (osSuffix == ".f")
                {
                    oResMetaItems.Add(osItemName.substr(0, nSuffixPos),
                                      CPLAtofM(osItemValue.c_str()));
                    continue;
                }
            }

            oResMetaItems.Add(osItemName, osItemValue);
        }
    }
}

bool FlushMetadata(const std::string &osUrl, const std::string &osResourceId,
                   char **papszMetadata, const CPLStringList &aosHTTPOptions)
{
    if (nullptr == papszMetadata)
    {
        return true;
    }
    CPLJSONObject oMetadataJson;
    FillResmeta(oMetadataJson, papszMetadata);

    return UpdateResource(
        osUrl, osResourceId,
        oMetadataJson.Format(CPLJSONObject::PrettyFormat::Plain),
        aosHTTPOptions);
}

bool DeleteFeature(const std::string &osUrl, const std::string &osResourceId,
                   const std::string &osFeatureId,
                   const CPLStringList &aosHTTPOptions)
{
    CPLErrorReset();
    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);
    aosHTTPOptionsInt.AddString("CUSTOMREQUEST=DELETE");
    std::string osUrlInt = GetFeatureURL(osUrl, osResourceId) + osFeatureId;
    CPLHTTPResult *psResult = CPLHTTPFetch(osUrlInt.c_str(), aosHTTPOptionsInt);
    bool bResult = false;
    if (psResult)
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;
        // Get error message.
        if (!bResult)
        {
            ReportError(psResult->pabyData, psResult->nDataLen,
                        "DeleteFeature request failed");
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bResult;
}

bool DeleteFeatures(const std::string &osUrl, const std::string &osResourceId,
                    const std::string &osFeaturesIDJson,
                    const CPLStringList &aosHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osFeaturesIDJson;

    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);
    aosHTTPOptionsInt.AddString("CUSTOMREQUEST=DELETE");
    aosHTTPOptionsInt.AddString(osPayloadInt.c_str());
    aosHTTPOptionsInt.AddString(
        "HEADERS=Content-Type: application/json\r\nAccept: */*");

    std::string osUrlInt = GetFeatureURL(osUrl, osResourceId);
    CPLHTTPResult *psResult = CPLHTTPFetch(osUrlInt.c_str(), aosHTTPOptionsInt);
    bool bResult = false;
    if (psResult)
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;
        // Get error message.
        if (!bResult)
        {
            ReportError(psResult->pabyData, psResult->nDataLen,
                        "DeleteFeatures request failed");
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bResult;
}

GIntBig CreateFeature(const std::string &osUrl, const std::string &osResourceId,
                      const std::string &osFeatureJson,
                      const CPLStringList &aosHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osFeatureJson;

    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);
    aosHTTPOptionsInt.AddString("CUSTOMREQUEST=POST");
    aosHTTPOptionsInt.AddString(osPayloadInt.c_str());
    aosHTTPOptionsInt.AddString(
        "HEADERS=Content-Type: application/json\r\nAccept: */*");

    CPLDebug("NGW", "CreateFeature request payload: %s", osFeatureJson.c_str());

    std::string osUrlInt = GetFeatureURL(osUrl, osResourceId);

    CPLJSONDocument oCreateFeatureReq;
    bool bResult = oCreateFeatureReq.LoadUrl(osUrlInt, aosHTTPOptionsInt);

    CPLJSONObject oRoot = oCreateFeatureReq.GetRoot();
    GIntBig nOutFID = OGRNullFID;
    if (CheckRequestResult(bResult, oRoot, "Create new feature failed"))
    {
        nOutFID = oRoot.GetLong("id", OGRNullFID);
    }

    CPLDebug("NGW", "CreateFeature new FID: " CPL_FRMT_GIB, nOutFID);
    return nOutFID;
}

bool UpdateFeature(const std::string &osUrl, const std::string &osResourceId,
                   const std::string &osFeatureId,
                   const std::string &osFeatureJson,
                   const CPLStringList &aosHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osFeatureJson;

    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);
    aosHTTPOptionsInt.AddString("CUSTOMREQUEST=PUT");
    aosHTTPOptionsInt.AddString(osPayloadInt.c_str());
    aosHTTPOptionsInt.AddString(
        "HEADERS=Content-Type: application/json\r\nAccept: */*");

    CPLDebug("NGW", "UpdateFeature request payload: %s", osFeatureJson.c_str());

    std::string osUrlInt = GetFeatureURL(osUrl, osResourceId) + osFeatureId;
    CPLHTTPResult *psResult = CPLHTTPFetch(osUrlInt.c_str(), aosHTTPOptionsInt);
    bool bResult = false;
    if (psResult)
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;

        // Get error message.
        if (!bResult)
        {
            ReportError(psResult->pabyData, psResult->nDataLen,
                        "UpdateFeature request failed");
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bResult;
}

std::vector<GIntBig> PatchFeatures(const std::string &osUrl,
                                   const std::string &osResourceId,
                                   const std::string &osFeaturesJson,
                                   const CPLStringList &aosHTTPOptions)
{
    std::vector<GIntBig> aoFIDs;
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osFeaturesJson;

    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);
    aosHTTPOptionsInt.AddString("CUSTOMREQUEST=PATCH");
    aosHTTPOptionsInt.AddString(osPayloadInt.c_str());
    aosHTTPOptionsInt.AddString(
        "HEADERS=Content-Type: application/json\r\nAccept: */*");

    CPLDebug("NGW", "PatchFeatures request payload: %s",
             osFeaturesJson.c_str());

    std::string osUrlInt = GetFeatureURL(osUrl, osResourceId);
    CPLJSONDocument oPatchFeatureReq;
    bool bResult = oPatchFeatureReq.LoadUrl(osUrlInt, aosHTTPOptionsInt);

    CPLJSONObject oRoot = oPatchFeatureReq.GetRoot();
    if (CheckRequestResult(bResult, oRoot, "Patch features failed"))
    {
        CPLJSONArray aoJSONIDs = oRoot.ToArray();
        for (int i = 0; i < aoJSONIDs.Size(); ++i)
        {
            GIntBig nOutFID = aoJSONIDs[i].GetLong("id", OGRNullFID);
            aoFIDs.push_back(nOutFID);
        }
    }
    return aoFIDs;
}

bool GetExtent(const std::string &osUrl, const std::string &osResourceId,
               const CPLStringList &aosHTTPOptions, int nEPSG,
               OGREnvelope &stExtent)
{
    CPLErrorReset();
    CPLJSONDocument oExtentReq;
    double dfRetryDelaySecs =
        CPLAtof(aosHTTPOptions.FetchNameValueDef("RETRY_DELAY", "2.5"));
    int nMaxRetries = atoi(aosHTTPOptions.FetchNameValueDef("MAX_RETRY", "0"));
    int nRetryCount = 0;
    while (true)
    {
        auto osUrlNew = GetLayerExtent(osUrl, osResourceId);
        bool bResult = oExtentReq.LoadUrl(osUrlNew, aosHTTPOptions);

        CPLJSONObject oRoot = oExtentReq.GetRoot();
        if (CheckRequestResult(bResult, oRoot, "Get extent failed"))
        {
            // Response extent spatial reference is EPSG:4326.

            double dfMinX = oRoot.GetDouble("extent/minLon");
            double dfMinY = oRoot.GetDouble("extent/minLat");
            double dfMaxX = oRoot.GetDouble("extent/maxLon");
            double dfMaxY = oRoot.GetDouble("extent/maxLat");

            double adfCoordinatesX[4];
            double adfCoordinatesY[4];
            adfCoordinatesX[0] = dfMinX;
            adfCoordinatesY[0] = dfMinY;
            adfCoordinatesX[1] = dfMinX;
            adfCoordinatesY[1] = dfMaxY;
            adfCoordinatesX[2] = dfMaxX;
            adfCoordinatesY[2] = dfMaxY;
            adfCoordinatesX[3] = dfMaxX;
            adfCoordinatesY[3] = dfMinY;

            OGRSpatialReference o4326SRS;
            o4326SRS.SetWellKnownGeogCS("WGS84");
            o4326SRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            OGRSpatialReference o3857SRS;
            o3857SRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (o3857SRS.importFromEPSG(nEPSG) != OGRERR_NONE)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Project extent SRS to EPSG:3857 failed");
                return false;
            }

            OGRCoordinateTransformation *poTransform =
                OGRCreateCoordinateTransformation(&o4326SRS, &o3857SRS);
            if (poTransform)
            {
                poTransform->Transform(4, adfCoordinatesX, adfCoordinatesY);
                delete poTransform;

                stExtent.MinX = std::numeric_limits<double>::max();
                stExtent.MaxX = std::numeric_limits<double>::min();
                stExtent.MinY = std::numeric_limits<double>::max();
                stExtent.MaxY = std::numeric_limits<double>::min();

                for (int i = 1; i < 4; ++i)
                {
                    if (stExtent.MinX > adfCoordinatesX[i])
                    {
                        stExtent.MinX = adfCoordinatesX[i];
                    }
                    if (stExtent.MaxX < adfCoordinatesX[i])
                    {
                        stExtent.MaxX = adfCoordinatesX[i];
                    }
                    if (stExtent.MinY > adfCoordinatesY[i])
                    {
                        stExtent.MinY = adfCoordinatesY[i];
                    }
                    if (stExtent.MaxY < adfCoordinatesY[i])
                    {
                        stExtent.MaxY = adfCoordinatesY[i];
                    }
                }
            }
            CPLErrorReset();  // If we are here no error occurred
            return true;
        }

        if (nRetryCount >= nMaxRetries)
        {
            return false;
        }

        CPLSleep(dfRetryDelaySecs);
        nRetryCount++;
    }
    return false;
}

CPLJSONObject UploadFile(const std::string &osUrl,
                         const std::string &osFilePath,
                         const CPLStringList &aosHTTPOptions,
                         GDALProgressFunc pfnProgress, void *pProgressData)
{
    CPLErrorReset();
    CPLStringList aosHTTPOptionsInt(aosHTTPOptions);
    aosHTTPOptionsInt.AddString(
        CPLSPrintf("FORM_FILE_PATH=%s", osFilePath.c_str()));
    aosHTTPOptionsInt.AddString("FORM_FILE_NAME=file");

    const char *pszFormFileName = CPLGetFilename(osFilePath.c_str());
    aosHTTPOptionsInt.AddString("FORM_KEY_0=name");
    aosHTTPOptionsInt.AddString(CPLSPrintf("FORM_VALUE_0=%s", pszFormFileName));
    aosHTTPOptionsInt.AddString("FORM_ITEM_COUNT=1");

    CPLHTTPResult *psResult =
        CPLHTTPFetchEx(GetUploadURL(osUrl).c_str(), aosHTTPOptionsInt,
                       pfnProgress, pProgressData, nullptr, nullptr);
    CPLJSONObject oResult;
    if (psResult)
    {
        const bool bResult =
            psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;

        // Get error message.
        if (!bResult)
        {
            ReportError(psResult->pabyData, psResult->nDataLen,
                        "Upload file request failed");
        }
        else
        {
            CPLJSONDocument oFileJson;
            if (oFileJson.LoadMemory(psResult->pabyData, psResult->nDataLen))
            {
                oResult = oFileJson.GetRoot();
            }
        }
        CPLHTTPDestroyResult(psResult);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Upload file %s failed",
                 osFilePath.c_str());
    }
    return oResult;
}

}  // namespace NGWAPI
