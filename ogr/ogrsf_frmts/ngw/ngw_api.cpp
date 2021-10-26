/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018-2020, NextGIS
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/

#include "ogr_ngw.h"

#include "cpl_http.h"

namespace NGWAPI {

std::string GetPermissions(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/permission";
}

std::string GetResource(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId;
}

std::string GetChildren(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl + "/api/resource/?parent=" + osResourceId;
}

std::string GetFeature(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/feature/";
}

std::string GetTMS(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl + "/api/component/render/tile?z=${z}&amp;x=${x}&amp;y=${y}&amp;resource=" +
        osResourceId;
}

std::string GetFeaturePage(const std::string &osUrl, const std::string &osResourceId,
    GIntBig nStart, int nCount, const std::string &osFields,
    const std::string &osWhere, const std::string &osSpatialWhere,
    const std::string &osExtensions, bool IsGeometryIgnored)
{
    std::string osFeatureUrl = GetFeature(osUrl, osResourceId);
    bool bParamAdd = false;
    if(nCount > 0)
    {
        osFeatureUrl += "?offset=" + std::to_string(nStart) + "&limit=" +
            std::to_string(nCount);
        bParamAdd = true;
    }

    if(!osFields.empty())
    {
        if(bParamAdd)
        {
            osFeatureUrl += "&fields=" + osFields;
        }
        else
        {
            osFeatureUrl += "?fields=" + osFields;
            bParamAdd = true;
        }
    }

    if(!osWhere.empty())
    {
        if(bParamAdd)
        {
            osFeatureUrl += "&" + osWhere;
        }
        else
        {
            osFeatureUrl += "?" + osWhere;
            bParamAdd = true;
        }
    }

    if(!osSpatialWhere.empty())
    {
        if(bParamAdd)
        {
            osFeatureUrl += "&intersects=" + osSpatialWhere;
        }
        else
        {
            osFeatureUrl += "?intersects=" + osSpatialWhere;
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
        bParamAdd = true;
    }
    CPL_IGNORE_RET_VAL(bParamAdd);

    if (IsGeometryIgnored)
    {
        osFeatureUrl += "&geom=no";
    }

    return osFeatureUrl;
}

std::string GetRoute(const std::string &osUrl)
{
    return osUrl + "/api/component/pyramid/route";
}

std::string GetUpload(const std::string &osUrl)
{
    return osUrl + "/api/component/file_upload/upload";
}

std::string GetVersion(const std::string &osUrl)
{
    return osUrl + "/api/component/pyramid/pkg_version";
}

bool CheckVersion(const std::string &osVersion, int nMajor, int nMinor, int nPatch)
{
    int nCurrentMajor(0);
    int nCurrentMinor(0);
    int nCurrentPatch(0);

    CPLStringList aosList(CSLTokenizeString2(osVersion.c_str(), ".", 0));
    if(aosList.size() > 2)
    {
        nCurrentMajor = atoi(aosList[0]);
        nCurrentMinor = atoi(aosList[1]);
        nCurrentPatch = atoi(aosList[2]);
    }
    else if(aosList.size() > 1)
    {
        nCurrentMajor = atoi(aosList[0]);
        nCurrentMinor = atoi(aosList[1]);
    }
    else if(aosList.size() > 0)
    {
        nCurrentMajor = atoi(aosList[0]);
    }

    return nCurrentMajor >= nMajor && nCurrentMinor >= nMinor &&
        nCurrentPatch >= nPatch;
}

Uri ParseUri(const std::string &osUrl)
{
    Uri stOut;
    std::size_t nFound = osUrl.find(":");
    if( nFound == std::string::npos )
    {
        return stOut;
    }

    stOut.osPrefix = osUrl.substr(0, nFound);
    std::string osUrlInt = CPLString(osUrl.substr(nFound + 1)).tolower();

    nFound = osUrlInt.find("/resource/");
    if( nFound == std::string::npos )
    {
        return stOut;
    }

    stOut.osAddress = osUrlInt.substr(0, nFound);

    std::string osResourceId = CPLString(osUrlInt.substr(nFound + strlen("/resource/"))).Trim();

    nFound = osResourceId.find('/');
    if( nFound != std::string::npos )
    {
        stOut.osResourceId = osResourceId.substr(0, nFound);
        stOut.osNewResourceName = osResourceId.substr(nFound + 1);
    }
    else
    {
        stOut.osResourceId = osResourceId;
    }

    return stOut;
}

static void ReportError(const GByte *pabyData, int nDataLen)
{
    CPLJSONDocument oResult;
    if( oResult.LoadMemory(pabyData, nDataLen) )
    {
        CPLJSONObject oRoot = oResult.GetRoot();
        if( oRoot.IsValid() )
        {
            std::string osErrorMessage = oRoot.GetString("message");
            if( !osErrorMessage.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMessage.c_str());
                return;
            }
        }
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Unexpected error occurred.");
}

std::string CreateResource(const std::string &osUrl, const std::string &osPayload,
    char **papszHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osPayload;

    papszHTTPOptions = CSLAddString( papszHTTPOptions, "CUSTOMREQUEST=POST" );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, osPayloadInt.c_str() );
    papszHTTPOptions = CSLAddString( papszHTTPOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    CPLDebug("NGW", "CreateResource request payload: %s", osPayload.c_str());

    CPLJSONDocument oCreateReq;
    bool bResult = oCreateReq.LoadUrl( GetResource( osUrl, "" ),
        papszHTTPOptions );
    CSLDestroy( papszHTTPOptions );
    std::string osResourceId("-1");
    CPLJSONObject oRoot = oCreateReq.GetRoot();
    if( oRoot.IsValid() )
    {
        if( bResult )
        {
            osResourceId = oRoot.GetString("id", "-1");
        }
        else
        {
            std::string osErrorMessage = oRoot.GetString("message");
            if( !osErrorMessage.empty() )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMessage.c_str());
            }
        }
    }
    return osResourceId;
}

bool UpdateResource(const std::string &osUrl, const std::string &osResourceId,
    const std::string &osPayload, char **papszHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osPayload;

    papszHTTPOptions = CSLAddString( papszHTTPOptions, "CUSTOMREQUEST=PUT" );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, osPayloadInt.c_str() );
    papszHTTPOptions = CSLAddString( papszHTTPOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    CPLDebug("NGW", "UpdateResource request payload: %s", osPayload.c_str());

    CPLHTTPResult *psResult = CPLHTTPFetch( GetResource(osUrl, osResourceId).c_str(),
        papszHTTPOptions );
    CSLDestroy( papszHTTPOptions );
    bool bResult = false;
    if( psResult )
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;

        // Get error message.
        if( !bResult )
        {
            ReportError(psResult->pabyData, psResult->nDataLen);
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
    char **papszHTTPOptions)
{
    CPLErrorReset();
    papszHTTPOptions = CSLAddString(papszHTTPOptions, "CUSTOMREQUEST=DELETE");
    CPLHTTPResult *psResult = CPLHTTPFetch( GetResource(osUrl, osResourceId).c_str(),
        papszHTTPOptions);
    bool bResult = false;
    if( psResult )
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;
        // Get error message.
        if( !bResult )
        {
            ReportError(psResult->pabyData, psResult->nDataLen);
        }
        CPLHTTPDestroyResult(psResult);
    }
    CSLDestroy( papszHTTPOptions );
    return bResult;
}

bool RenameResource(const std::string &osUrl, const std::string &osResourceId,
    const std::string &osNewName, char **papszHTTPOptions)
{
    CPLJSONObject oPayload;
    CPLJSONObject oResource("resource", oPayload);
    oResource.Add("display_name", osNewName);
    std::string osPayload = oPayload.Format(CPLJSONObject::PrettyFormat::Plain);

    return UpdateResource( osUrl, osResourceId, osPayload, papszHTTPOptions);
}

OGRwkbGeometryType NGWGeomTypeToOGRGeomType(const std::string &osGeomType)
{
    // http://docs.nextgis.com/docs_ngweb_dev/doc/developer/vector_data_types.html#nextgisweb.feature_layer.interface.GEOM_TYPE
    if( osGeomType == "POINT")
        return wkbPoint;
    else if ( osGeomType == "LINESTRING")
        return wkbLineString;
    else if ( osGeomType == "POLYGON")
        return wkbPolygon;
    else if ( osGeomType == "MULTIPOINT")
        return wkbMultiPoint;
    else if ( osGeomType == "MULTILINESTRING")
        return wkbMultiLineString;
    else if ( osGeomType == "MULTIPOLYGON")
        return wkbMultiPolygon;
    else if( osGeomType == "POINTZ")
        return wkbPoint25D;
    else if ( osGeomType == "LINESTRINGZ")
        return wkbLineString25D;
    else if ( osGeomType == "POLYGONZ")
        return wkbPolygon25D;
    else if ( osGeomType == "MULTIPOINTZ")
        return wkbMultiPoint25D;
    else if ( osGeomType == "MULTILINESTRINGZ")
        return wkbMultiLineString25D;
    else if ( osGeomType == "MULTIPOLYGONZ")
        return wkbMultiPolygon25D;
    else
        return wkbUnknown;
}

std::string OGRGeomTypeToNGWGeomType(OGRwkbGeometryType eType)
{
    switch(eType)
    { // Don't flatten
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
    if( osFieldType == "INTEGER")
        return OFTInteger;
    else if ( osFieldType == "BIGINT")
        return OFTInteger64;
    else if ( osFieldType == "REAL")
        return OFTReal;
    else if ( osFieldType == "STRING")
        return OFTString;
    else if ( osFieldType == "DATE")
        return OFTDate;
    else if ( osFieldType == "TIME")
        return OFTTime;
    else if ( osFieldType == "DATETIME")
        return OFTDateTime;
    else
        return OFTString;
}

std::string OGRFieldTypeToNGWFieldType(OGRFieldType eType)
{
    switch(eType) {
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
    const std::string &osResourceId, char **papszHTTPOptions, bool bReadWrite)
{
    Permissions stOut;
    CPLErrorReset();
    CPLJSONDocument oPermissionReq;
    bool bResult = oPermissionReq.LoadUrl( GetPermissions( osUrl, osResourceId ),
        papszHTTPOptions );

    CPLJSONObject oRoot = oPermissionReq.GetRoot();
    if( oRoot.IsValid() )
    {
        if( bResult )
        {
            stOut.bResourceCanRead = oRoot.GetBool( "resource/read", true );
            stOut.bResourceCanCreate = oRoot.GetBool( "resource/create", bReadWrite );
            stOut.bResourceCanUpdate = oRoot.GetBool( "resource/update", bReadWrite );
            stOut.bResourceCanDelete = oRoot.GetBool( "resource/delete", bReadWrite );

            stOut.bDatastructCanRead = oRoot.GetBool( "datastruct/read", true );
            stOut.bDatastructCanWrite = oRoot.GetBool( "datastruct/write", bReadWrite );

            stOut.bDataCanRead = oRoot.GetBool( "data/read", true );
            stOut.bDataCanWrite = oRoot.GetBool( "data/write", bReadWrite );

            stOut.bMetadataCanRead = oRoot.GetBool( "metadata/read", true );
            stOut.bMetadataCanWrite = oRoot.GetBool( "metadata/write", bReadWrite );
        }
        else
        {
            std::string osErrorMessage = oRoot.GetString("message");
            if( osErrorMessage.empty() )
            {
                osErrorMessage = "Get permissions failed";
            }
            CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMessage.c_str());
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Get permissions failed");
    }

    return stOut;
}

std::string GetFeatureCount(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/feature_count";
}

std::string GetLayerExtent(const std::string &osUrl, const std::string &osResourceId)
{
    return osUrl + "/api/resource/" + osResourceId + "/extent";
}

std::string GetResmetaSuffix(CPLJSONObject::Type eType)
{
    switch( eType ) {
        case CPLJSONObject::Type::Integer:
        case CPLJSONObject::Type::Long:
            return ".d";
        case CPLJSONObject::Type::Double:
            return ".f";
        default:
            return "";
    }
}

void FillResmeta(CPLJSONObject &oRoot, char **papszMetadata)
{
    CPLJSONObject oResMeta("resmeta", oRoot);
    CPLJSONObject oResMetaItems("items", oResMeta);
    CPLStringList oaMetadata(papszMetadata, FALSE);
    for( int i = 0; i < oaMetadata.size(); ++i )
    {
        std::string osItem = oaMetadata[i];
        size_t nPos = osItem.find("=");
        if( nPos != std::string::npos )
        {
            std::string osItemName = osItem.substr( 0, nPos );
            CPLString osItemValue = osItem.substr( nPos + 1 );

            if( osItemName.size() > 2 )
            {
                size_t nSuffixPos = osItemName.size() - 2;
                std::string osSuffix = osItemName.substr(nSuffixPos);
                if( osSuffix == ".d")
                {
                    GInt64 nVal = CPLAtoGIntBig( osItemValue.c_str() );
                    oResMetaItems.Add( osItemName.substr(0, nSuffixPos), nVal );
                    continue;
                }

                if( osSuffix == ".f")
                {
                    oResMetaItems.Add( osItemName.substr(0, nSuffixPos),
                        CPLAtofM(osItemValue.c_str()) );
                    continue;
                }
            }

            oResMetaItems.Add(osItemName, osItemValue);
        }
    }
}

bool FlushMetadata(const std::string &osUrl, const std::string &osResourceId,
    char **papszMetadata, char **papszHTTPOptions )
{
    if( nullptr == papszMetadata )
    {
        return true;
    }
    CPLJSONObject oMetadataJson;
    FillResmeta(oMetadataJson, papszMetadata);

    return UpdateResource( osUrl, osResourceId,
        oMetadataJson.Format(CPLJSONObject::PrettyFormat::Plain), papszHTTPOptions);
}

bool DeleteFeature(const std::string &osUrl, const std::string &osResourceId,
    const std::string &osFeatureId, char **papszHTTPOptions)
{
    CPLErrorReset();
    papszHTTPOptions = CSLAddString(papszHTTPOptions, "CUSTOMREQUEST=DELETE");
    std::string osUrlInt = GetFeature(osUrl, osResourceId) + osFeatureId;
    CPLHTTPResult *psResult = CPLHTTPFetch( osUrlInt.c_str(), papszHTTPOptions);
    CSLDestroy( papszHTTPOptions );
    bool bResult = false;
    if( psResult )
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;
        // Get error message.
        if( !bResult )
        {
            ReportError(psResult->pabyData, psResult->nDataLen);
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bResult;
}

GIntBig CreateFeature(const std::string &osUrl, const std::string &osResourceId,
    const std::string &osFeatureJson, char **papszHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osFeatureJson;

    papszHTTPOptions = CSLAddString( papszHTTPOptions, "CUSTOMREQUEST=POST" );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, osPayloadInt.c_str() );
    papszHTTPOptions = CSLAddString( papszHTTPOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    CPLDebug("NGW", "CreateFeature request payload: %s", osFeatureJson.c_str());

    std::string osUrlInt = GetFeature( osUrl, osResourceId );

    CPLJSONDocument oCreateFeatureReq;
    bool bResult = oCreateFeatureReq.LoadUrl( osUrlInt, papszHTTPOptions );
    CSLDestroy( papszHTTPOptions );

    CPLJSONObject oRoot = oCreateFeatureReq.GetRoot();
    GIntBig nOutFID = OGRNullFID;
    if( oRoot.IsValid() )
    {
        if( bResult )
        {
            nOutFID = oRoot.GetLong( "id", OGRNullFID );
        }
        else
        {
            std::string osErrorMessage = oRoot.GetString("message");
            if( osErrorMessage.empty() )
            {
                osErrorMessage = "Create new feature failed";
            }
            CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMessage.c_str());
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Create new feature failed");
    }

    CPLDebug("NGW", "CreateFeature new FID: " CPL_FRMT_GIB, nOutFID);
    return nOutFID;
}

bool UpdateFeature(const std::string &osUrl, const std::string &osResourceId,
    const std::string &osFeatureId, const std::string &osFeatureJson,
    char **papszHTTPOptions)
{
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osFeatureJson;

    papszHTTPOptions = CSLAddString( papszHTTPOptions, "CUSTOMREQUEST=PUT" );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, osPayloadInt.c_str() );
    papszHTTPOptions = CSLAddString( papszHTTPOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    CPLDebug("NGW", "UpdateFeature request payload: %s", osFeatureJson.c_str());

    std::string osUrlInt = GetFeature(osUrl, osResourceId) + osFeatureId;
    CPLHTTPResult *psResult = CPLHTTPFetch( osUrlInt.c_str(), papszHTTPOptions );
    CSLDestroy( papszHTTPOptions );
    bool bResult = false;
    if( psResult )
    {
        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;

        // Get error message.
        if( !bResult )
        {
            ReportError(psResult->pabyData, psResult->nDataLen);
        }
        CPLHTTPDestroyResult(psResult);
    }
    return bResult;
}

std::vector<GIntBig> PatchFeatures(const std::string &osUrl, const std::string &osResourceId,
    const std::string &osFeaturesJson, char **papszHTTPOptions)
{
    std::vector<GIntBig> aoFIDs;
    CPLErrorReset();
    std::string osPayloadInt = "POSTFIELDS=" + osFeaturesJson;

    papszHTTPOptions = CSLAddString( papszHTTPOptions, "CUSTOMREQUEST=PATCH" );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, osPayloadInt.c_str() );
    papszHTTPOptions = CSLAddString( papszHTTPOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    CPLDebug("NGW", "PatchFeatures request payload: %s", osFeaturesJson.c_str());

    std::string osUrlInt = GetFeature(osUrl, osResourceId);
    CPLJSONDocument oPatchFeatureReq;
    bool bResult = oPatchFeatureReq.LoadUrl( osUrlInt, papszHTTPOptions );
    CSLDestroy( papszHTTPOptions );

    CPLJSONObject oRoot = oPatchFeatureReq.GetRoot();
    if( oRoot.IsValid() )
    {
        if( bResult )
        {
            CPLJSONArray aoJSONIDs = oRoot.ToArray();
            for( int i = 0; i < aoJSONIDs.Size(); ++i)
            {
                GIntBig nOutFID = aoJSONIDs[i].GetLong( "id", OGRNullFID );
                aoFIDs.push_back(nOutFID);
            }
        }
        else
        {
            std::string osErrorMessage = oRoot.GetString("message");
            if( osErrorMessage.empty() )
            {
                osErrorMessage = "Patch features failed";
            }
            CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMessage.c_str());
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Patch features failed");
    }
    return aoFIDs;
}

bool GetExtent(const std::string &osUrl, const std::string &osResourceId,
    char **papszHTTPOptions, int nEPSG, OGREnvelope &stExtent)
{
    CPLErrorReset();
    CPLJSONDocument oExtentReq;
    bool bResult = oExtentReq.LoadUrl( GetLayerExtent( osUrl, osResourceId ),
        papszHTTPOptions);

    CPLJSONObject oRoot = oExtentReq.GetRoot();
    if( !bResult)
    {
        std::string osErrorMessage = oRoot.GetString("message");
        if( osErrorMessage.empty() )
        {
            osErrorMessage = "Get extent failed";
        }
        CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMessage.c_str());
        return false;
    }
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
    o4326SRS.SetWellKnownGeogCS( "WGS84" );
    o4326SRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    OGRSpatialReference o3857SRS;
    o3857SRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if( o3857SRS.importFromEPSG(nEPSG) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Project extent SRS to EPSG:3857 failed");
        return false;
    }

    OGRCoordinateTransformation *poTransform =
        OGRCreateCoordinateTransformation( &o4326SRS, &o3857SRS );
    if( poTransform )
    {
        poTransform->Transform( 4, adfCoordinatesX, adfCoordinatesY );
        delete poTransform;

        stExtent.MinX = std::numeric_limits<double>::max();
        stExtent.MaxX = std::numeric_limits<double>::min();
        stExtent.MinY = std::numeric_limits<double>::max();
        stExtent.MaxY = std::numeric_limits<double>::min();

        for(int i = 1; i < 4; ++i)
        {
            if( stExtent.MinX > adfCoordinatesX[i] )
            {
                stExtent.MinX = adfCoordinatesX[i];
            }
            if( stExtent.MaxX < adfCoordinatesX[i] )
            {
                stExtent.MaxX = adfCoordinatesX[i];
            }
            if( stExtent.MinY > adfCoordinatesY[i] )
            {
                stExtent.MinY = adfCoordinatesY[i];
            }
            if( stExtent.MaxY < adfCoordinatesY[i] )
            {
                stExtent.MaxY = adfCoordinatesY[i];
            }
        }
    }
    return true;
}

CPLJSONObject UploadFile(const std::string &osUrl, const std::string &osFilePath,
    char **papszHTTPOptions, GDALProgressFunc pfnProgress, void *pProgressData)
{
    CPLErrorReset();
    papszHTTPOptions = CSLAddString( papszHTTPOptions,
        CPLSPrintf("FORM_FILE_PATH=%s", osFilePath.c_str()) );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, "FORM_FILE_NAME=file" );

    const char* pszFormFileName = CPLGetFilename( osFilePath.c_str() );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, "FORM_KEY_0=name" );
    papszHTTPOptions = CSLAddString( papszHTTPOptions,
        CPLSPrintf("FORM_VALUE_0=%s", pszFormFileName) );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, "FORM_ITEM_COUNT=1" );

    CPLHTTPResult *psResult = CPLHTTPFetchEx( GetUpload(osUrl).c_str(),
        papszHTTPOptions, pfnProgress, pProgressData, nullptr, nullptr );
    CSLDestroy( papszHTTPOptions );
    CPLJSONObject oResult;
    if( psResult )
    {
        const bool bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;

        // Get error message.
        if( !bResult )
        {
            ReportError(psResult->pabyData, psResult->nDataLen);
            CPLHTTPDestroyResult(psResult);
            return oResult;
        }
        CPLJSONDocument oFileJson;
        if( oFileJson.LoadMemory(psResult->pabyData, psResult->nDataLen) )
        {
            oResult = oFileJson.GetRoot();
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

} // namespace NGWAPI
