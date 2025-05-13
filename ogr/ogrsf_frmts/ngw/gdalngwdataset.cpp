/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018-2025, NextGIS <info@nextgis.com>
 *
 * SPDX-License-Identifier: MIT
 *******************************************************************************/

#include "ogr_ngw.h"

#include "cpl_http.h"
#include "gdal_proxy.h"

#include <array>
#include <limits>

class NGWWrapperRasterBand : public GDALProxyRasterBand
{
    GDALRasterBand *poBaseBand;

  protected:
    virtual GDALRasterBand *
    RefUnderlyingRasterBand(bool /*bForceOpen*/) const override
    {
        return poBaseBand;
    }

  public:
    explicit NGWWrapperRasterBand(GDALRasterBand *poBaseBandIn)
        : poBaseBand(poBaseBandIn)
    {
        eDataType = poBaseBand->GetRasterDataType();
        poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

    virtual ~NGWWrapperRasterBand()
    {
    }
};

static const char *FormGDALTMSConnectionString(const std::string &osUrl,
                                               const std::string &osResourceId,
                                               int nEPSG, int nCacheExpires,
                                               int nCacheMaxSize)
{
    std::string osRasterUrl = NGWAPI::GetTMSURL(osUrl, osResourceId);
    char *pszRasterUrl = CPLEscapeString(osRasterUrl.c_str(), -1, CPLES_XML);
    const char *pszConnStr =
        CPLSPrintf("<GDAL_WMS><Service name=\"TMS\">"
                   "<ServerUrl>%s</ServerUrl></Service><DataWindow>"
                   "<UpperLeftX>-20037508.34</"
                   "UpperLeftX><UpperLeftY>20037508.34</UpperLeftY>"
                   "<LowerRightX>20037508.34</"
                   "LowerRightX><LowerRightY>-20037508.34</LowerRightY>"
                   "<TileLevel>%d</TileLevel><TileCountX>1</TileCountX>"
                   "<TileCountY>1</TileCountY><YOrigin>top</YOrigin></"
                   "DataWindow>"
                   "<Projection>EPSG:%d</Projection><BlockSizeX>256</"
                   "BlockSizeX>"
                   "<BlockSizeY>256</BlockSizeY><BandsCount>%d</BandsCount>"
                   "<Cache><Type>file</Type><Expires>%d</Expires><MaxSize>%d</"
                   "MaxSize>"
                   "</Cache><ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes></"
                   "GDAL_WMS>",
                   pszRasterUrl,
                   22,     // NOTE: We have no limit in zoom levels.
                   nEPSG,  // NOTE: Default SRS is EPSG:3857.
                   4, nCacheExpires, nCacheMaxSize);

    CPLFree(pszRasterUrl);
    return pszConnStr;
}

static std::string GetStylesIdentifiers(const CPLJSONArray &aoStyles, int nDeep)
{
    std::string sOut;
    if (nDeep > 255)
    {
        return sOut;
    }

    for (const auto &subobj : aoStyles)
    {
        auto sType = subobj.GetString("item_type");
        if (sType == "layer")
        {
            auto sId = subobj.GetString("layer_style_id");
            if (!sId.empty())
            {
                if (sOut.empty())
                {
                    sOut = std::move(sId);
                }
                else
                {
                    sOut += "," + sId;
                }
            }
        }
        else
        {
            auto aoChildren = subobj.GetArray("children");
            auto sId = GetStylesIdentifiers(aoChildren, nDeep + 1);
            if (!sId.empty())
            {
                if (sOut.empty())
                {
                    sOut = std::move(sId);
                }
                else
                {
                    sOut += "," + sId;
                }
            }
        }
    }
    return sOut;
}

/*
 * OGRNGWDataset()
 */
OGRNGWDataset::OGRNGWDataset()
    : nBatchSize(-1), nPageSize(-1), bFetchedPermissions(false),
      bHasFeaturePaging(false), bExtInNativeData(false), bMetadataDerty(false),
      poRasterDS(nullptr), nRasters(0), nCacheExpires(604800),  // 7 days
      nCacheMaxSize(67108864),                                  // 64 MB
      osJsonDepth("32")
{
}

/*
 * ~OGRNGWDataset()
 */
OGRNGWDataset::~OGRNGWDataset()
{
    // Last sync with server.
    OGRNGWDataset::FlushCache(true);

    if (poRasterDS != nullptr)
    {
        GDALClose(poRasterDS);
        poRasterDS = nullptr;
    }
}

/*
 * FetchPermissions()
 */
void OGRNGWDataset::FetchPermissions()
{
    if (bFetchedPermissions)
    {
        return;
    }

    if (IsUpdateMode())
    {
        // Check connection and is it read only.
        stPermissions = NGWAPI::CheckPermissions(
            osUrl, osResourceId, GetHeaders(false), IsUpdateMode());
    }
    else
    {
        stPermissions.bDataCanRead = true;
        stPermissions.bResourceCanRead = true;
        stPermissions.bDatastructCanRead = true;
        stPermissions.bMetadataCanRead = true;
    }
    bFetchedPermissions = true;
}

/*
 * TestCapability()
 */
int OGRNGWDataset::TestCapability(const char *pszCap)
{
    FetchPermissions();
    if (EQUAL(pszCap, ODsCCreateLayer))
    {
        return stPermissions.bResourceCanCreate;
    }
    else if (EQUAL(pszCap, ODsCDeleteLayer))
    {
        return stPermissions.bResourceCanDelete;
    }
    else if (EQUAL(pszCap, "RenameLayer"))
    {
        return stPermissions.bResourceCanUpdate;
    }
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
    {
        return stPermissions.bDataCanWrite;  // FIXME: Check on resource level
                                             // is this permission set?
    }
    else if (EQUAL(pszCap, ODsCRandomLayerRead))
    {
        return stPermissions.bDataCanRead;
    }
    else if (EQUAL(pszCap, ODsCZGeometries))
    {
        return TRUE;
    }
    else if (EQUAL(pszCap, ODsCAddFieldDomain))
    {
        return stPermissions.bResourceCanCreate;
    }
    else if (EQUAL(pszCap, ODsCDeleteFieldDomain))
    {
        return stPermissions.bResourceCanDelete;
    }
    else if (EQUAL(pszCap, ODsCUpdateFieldDomain))
    {
        return stPermissions.bResourceCanUpdate;
    }
    else
    {
        return FALSE;
    }
}

/*
 * GetLayer()
 */
OGRLayer *OGRNGWDataset::GetLayer(int iLayer)
{
    if (iLayer < 0 || iLayer >= GetLayerCount())
    {
        return nullptr;
    }
    else
    {
        return aoLayers[iLayer].get();
    }
}

/*
 * Open()
 */
bool OGRNGWDataset::Open(const std::string &osUrlIn,
                         const std::string &osResourceIdIn,
                         char **papszOpenOptionsIn, bool bUpdateIn,
                         int nOpenFlagsIn)
{
    osUrl = osUrlIn;
    osResourceId = osResourceIdIn;

    eAccess = bUpdateIn ? GA_Update : GA_ReadOnly;

    osUserPwd = CSLFetchNameValueDef(papszOpenOptionsIn, "USERPWD",
                                     CPLGetConfigOption("NGW_USERPWD", ""));

    nBatchSize =
        atoi(CSLFetchNameValueDef(papszOpenOptionsIn, "BATCH_SIZE",
                                  CPLGetConfigOption("NGW_BATCH_SIZE", "-1")));

    nPageSize =
        atoi(CSLFetchNameValueDef(papszOpenOptionsIn, "PAGE_SIZE",
                                  CPLGetConfigOption("NGW_PAGE_SIZE", "-1")));
    if (nPageSize == 0)
    {
        nPageSize = -1;
    }

    nCacheExpires = atoi(CSLFetchNameValueDef(
        papszOpenOptionsIn, "CACHE_EXPIRES",
        CPLGetConfigOption("NGW_CACHE_EXPIRES", "604800")));

    nCacheMaxSize = atoi(CSLFetchNameValueDef(
        papszOpenOptionsIn, "CACHE_MAX_SIZE",
        CPLGetConfigOption("NGW_CACHE_MAX_SIZE", "67108864")));

    bExtInNativeData =
        CPLFetchBool(papszOpenOptionsIn, "NATIVE_DATA",
                     CPLTestBool(CPLGetConfigOption("NGW_NATIVE_DATA", "NO")));

    osJsonDepth =
        CSLFetchNameValueDef(papszOpenOptionsIn, "JSON_DEPTH",
                             CPLGetConfigOption("NGW_JSON_DEPTH", "32"));

    osExtensions =
        CSLFetchNameValueDef(papszOpenOptionsIn, "EXTENSIONS",
                             CPLGetConfigOption("NGW_EXTENSIONS", ""));

    osConnectTimeout =
        CSLFetchNameValueDef(papszOpenOptionsIn, "CONNECTTIMEOUT",
                             CPLGetConfigOption("NGW_CONNECTTIMEOUT", ""));
    osTimeout = CSLFetchNameValueDef(papszOpenOptionsIn, "TIMEOUT",
                                     CPLGetConfigOption("NGW_TIMEOUT", ""));
    osRetryCount =
        CSLFetchNameValueDef(papszOpenOptionsIn, "MAX_RETRY",
                             CPLGetConfigOption("NGW_MAX_RETRY", ""));
    osRetryDelay =
        CSLFetchNameValueDef(papszOpenOptionsIn, "RETRY_DELAY",
                             CPLGetConfigOption("NGW_RETRY_DELAY", ""));

    if (osExtensions.empty())
    {
        bExtInNativeData = false;
    }

    CPLDebug("NGW",
             "Open options:\n"
             "  BATCH_SIZE %d\n"
             "  PAGE_SIZE %d\n"
             "  CACHE_EXPIRES %d\n"
             "  CACHE_MAX_SIZE %d\n"
             "  JSON_DEPTH %s\n"
             "  EXTENSIONS %s\n"
             "  CONNECTTIMEOUT %s\n"
             "  TIMEOUT %s\n"
             "  MAX_RETRY %s\n"
             "  RETRY_DELAY %s",
             nBatchSize, nPageSize, nCacheExpires, nCacheMaxSize,
             osJsonDepth.c_str(), osExtensions.c_str(),
             osConnectTimeout.c_str(), osTimeout.c_str(), osRetryCount.c_str(),
             osRetryDelay.c_str());

    return Init(nOpenFlagsIn);
}

/*
 * Open()
 *
 * The pszFilename templates:
 *      - NGW:http://some.nextgis.com/resource/0
 *      - NGW:http://some.nextgis.com:8000/test/resource/0
 */
bool OGRNGWDataset::Open(const char *pszFilename, char **papszOpenOptionsIn,
                         bool bUpdateIn, int nOpenFlagsIn)
{
    NGWAPI::Uri stUri = NGWAPI::ParseUri(pszFilename);

    if (stUri.osPrefix != "NGW")
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported name %s",
                 pszFilename);
        return false;
    }

    osUrl = stUri.osAddress;
    osResourceId = stUri.osResourceId;

    return Open(stUri.osAddress, stUri.osResourceId, papszOpenOptionsIn,
                bUpdateIn, nOpenFlagsIn);
}

/*
 * SetupRasterDSWrapper()
 */
void OGRNGWDataset::SetupRasterDSWrapper(const OGREnvelope &stExtent)
{
    if (poRasterDS)
    {
        nRasterXSize = poRasterDS->GetRasterXSize();
        nRasterYSize = poRasterDS->GetRasterYSize();

        for (int iBand = 1; iBand <= poRasterDS->GetRasterCount(); iBand++)
        {
            SetBand(iBand,
                    new NGWWrapperRasterBand(poRasterDS->GetRasterBand(iBand)));
        }

        if (stExtent.IsInit())
        {
            // Set pixel limits.
            bool bHasTransform = false;
            double geoTransform[6] = {0.0};
            double invGeoTransform[6] = {0.0};
            if (poRasterDS->GetGeoTransform(geoTransform) == CE_None)
            {
                bHasTransform =
                    GDALInvGeoTransform(geoTransform, invGeoTransform) == TRUE;
            }

            if (bHasTransform)
            {
                GDALApplyGeoTransform(invGeoTransform, stExtent.MinX,
                                      stExtent.MinY, &stPixelExtent.MinX,
                                      &stPixelExtent.MaxY);

                GDALApplyGeoTransform(invGeoTransform, stExtent.MaxX,
                                      stExtent.MaxY, &stPixelExtent.MaxX,
                                      &stPixelExtent.MinY);

                CPLDebug("NGW", "Raster extent in px is: %f, %f, %f, %f",
                         stPixelExtent.MinX, stPixelExtent.MinY,
                         stPixelExtent.MaxX, stPixelExtent.MaxY);
            }
            else
            {
                stPixelExtent.MinX = 0.0;
                stPixelExtent.MinY = 0.0;
                stPixelExtent.MaxX = std::numeric_limits<double>::max();
                stPixelExtent.MaxY = std::numeric_limits<double>::max();
            }
        }
    }
}

/*
 * Init()
 */
bool OGRNGWDataset::Init(int nOpenFlagsIn)
{
    // NOTE: Skip check API version at that moment. We expected API v3 or higher.

    // Get resource details.
    CPLJSONDocument oResourceDetailsReq;
    auto aosHTTPOptions = GetHeaders(false);
    bool bResult = oResourceDetailsReq.LoadUrl(
        NGWAPI::GetResourceURL(osUrl, osResourceId), aosHTTPOptions);

    CPLDebug("NGW", "Get resource %s details %s", osResourceId.c_str(),
             bResult ? "success" : "failed");

    if (bResult)
    {
        CPLJSONObject oRoot = oResourceDetailsReq.GetRoot();

        if (oRoot.IsValid())
        {
            auto osResourceType = oRoot.GetString("resource/cls");
            FillMetadata(oRoot);

            if (osResourceType == "resource_group")
            {
                // Check feature paging.
                FillCapabilities(aosHTTPOptions);
                if (oRoot.GetBool("resource/children", false))
                {
                    // Get child resources.
                    bResult = FillResources(aosHTTPOptions, nOpenFlagsIn);
                }
            }
            else if (NGWAPI::CheckSupportedType(false, osResourceType))
            {
                // Check feature paging.
                FillCapabilities(aosHTTPOptions);
                // Add vector layer.
                AddLayer(oRoot, aosHTTPOptions, nOpenFlagsIn);
            }
            else if (osResourceType == "mapserver_style" ||
                     osResourceType == "qgis_vector_style" ||
                     osResourceType == "raster_style" ||
                     osResourceType == "qgis_raster_style")
            {
                // GetExtent from parent.
                OGREnvelope stExtent;
                std::string osParentId = oRoot.GetString("resource/parent/id");
                bool bExtentResult = NGWAPI::GetExtent(
                    osUrl, osParentId, aosHTTPOptions, 3857, stExtent);

                if (!bExtentResult)
                {
                    // Set full extent for EPSG:3857.
                    stExtent.MinX = -20037508.34;
                    stExtent.MaxX = 20037508.34;
                    stExtent.MinY = -20037508.34;
                    stExtent.MaxY = 20037508.34;
                }

                CPLDebug("NGW", "Raster extent is: %f, %f, %f, %f",
                         stExtent.MinX, stExtent.MinY, stExtent.MaxX,
                         stExtent.MaxY);

                int nEPSG = 3857;
                // NOTE: Get parent details. We can skip this as default SRS in
                // NGW is 3857.
                CPLJSONDocument oResourceReq;
                bResult = oResourceReq.LoadUrl(
                    NGWAPI::GetResourceURL(osUrl, osResourceId),
                    aosHTTPOptions);

                if (bResult)
                {
                    CPLJSONObject oParentRoot = oResourceReq.GetRoot();
                    if (osResourceType == "mapserver_style" ||
                        osResourceType == "qgis_vector_style")
                    {
                        nEPSG = oParentRoot.GetInteger("vector_layer/srs/id",
                                                       nEPSG);
                    }
                    else if (osResourceType == "raster_style" ||
                             osResourceType == "qgis_raster_style")
                    {
                        nEPSG = oParentRoot.GetInteger("raster_layer/srs/id",
                                                       nEPSG);
                    }
                }

                const char *pszConnStr = FormGDALTMSConnectionString(
                    osUrl, osResourceId, nEPSG, nCacheExpires, nCacheMaxSize);
                CPLDebug("NGW", "Open %s as '%s'", osResourceType.c_str(),
                         pszConnStr);
                poRasterDS = GDALDataset::FromHandle(GDALOpenEx(
                    pszConnStr,
                    GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    nullptr, nullptr, nullptr));
                SetupRasterDSWrapper(stExtent);
            }
            else if (osResourceType == "wmsclient_layer")
            {
                OGREnvelope stExtent;
                // Set full extent for EPSG:3857.
                stExtent.MinX = -20037508.34;
                stExtent.MaxX = 20037508.34;
                stExtent.MinY = -20037508.34;
                stExtent.MaxY = 20037508.34;

                CPLDebug("NGW", "Raster extent is: %f, %f, %f, %f",
                         stExtent.MinX, stExtent.MinY, stExtent.MaxX,
                         stExtent.MaxY);

                int nEPSG = oRoot.GetInteger("wmsclient_layer/srs/id", 3857);

                const char *pszConnStr = FormGDALTMSConnectionString(
                    osUrl, osResourceId, nEPSG, nCacheExpires, nCacheMaxSize);
                poRasterDS = GDALDataset::FromHandle(GDALOpenEx(
                    pszConnStr,
                    GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    nullptr, nullptr, nullptr));
                SetupRasterDSWrapper(stExtent);
            }
            else if (osResourceType == "basemap_layer")
            {
                auto osTMSURL = oRoot.GetString("basemap_layer/url");
                int nEPSG = 3857;
                auto osQMS = oRoot.GetString("basemap_layer/qms");
                if (!osQMS.empty())
                {
                    CPLJSONDocument oDoc;
                    if (oDoc.LoadMemory(osQMS))
                    {
                        auto oQMLRoot = oDoc.GetRoot();
                        nEPSG = oQMLRoot.GetInteger("epsg");
                    }
                }

                // TODO: for EPSG != 3857 need to calc full extent
                if (nEPSG != 3857)
                {
                    bResult = false;
                }
                else
                {
                    OGREnvelope stExtent;
                    // Set full extent for EPSG:3857.
                    stExtent.MinX = -20037508.34;
                    stExtent.MaxX = 20037508.34;
                    stExtent.MinY = -20037508.34;
                    stExtent.MaxY = 20037508.34;

                    const char *pszConnStr = FormGDALTMSConnectionString(
                        osTMSURL, osResourceId, nEPSG, nCacheExpires,
                        nCacheMaxSize);
                    poRasterDS = GDALDataset::FromHandle(GDALOpenEx(
                        pszConnStr,
                        GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                        nullptr, nullptr, nullptr));
                    SetupRasterDSWrapper(stExtent);
                }
            }
            else if (osResourceType == "webmap")
            {
                OGREnvelope stExtent;
                // Set full extent for EPSG:3857.
                stExtent.MinX = -20037508.34;
                stExtent.MaxX = 20037508.34;
                stExtent.MinY = -20037508.34;
                stExtent.MaxY = 20037508.34;

                // Get all styles
                auto aoChildren = oRoot.GetArray("webmap/children");
                auto sIdentifiers = GetStylesIdentifiers(aoChildren, 0);

                const char *pszConnStr = FormGDALTMSConnectionString(
                    osUrl, sIdentifiers, 3857, nCacheExpires, nCacheMaxSize);
                poRasterDS = GDALDataset::FromHandle(GDALOpenEx(
                    pszConnStr,
                    GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    nullptr, nullptr, nullptr));
                SetupRasterDSWrapper(stExtent);
            }
            else if (osResourceType == "raster_layer")
            {
                auto osCogURL = NGWAPI::GetCOGURL(osUrl, osResourceId);
                auto osConnStr = std::string("/vsicurl/") + osCogURL;

                CPLDebug("NGW", "Raster url is: %s", osConnStr.c_str());

                poRasterDS = GDALDataset::FromHandle(GDALOpenEx(
                    osConnStr.c_str(),
                    GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                    nullptr, nullptr, nullptr));

                // Add styles if exists
                auto osRasterResourceId = oRoot.GetString("resource/id");
                CPLJSONDocument oResourceRequest;
                bool bLoadResult = oResourceRequest.LoadUrl(
                    NGWAPI::GetChildrenURL(osUrl, osRasterResourceId),
                    aosHTTPOptions);
                if (bLoadResult)
                {
                    CPLJSONArray oChildren(oResourceRequest.GetRoot());
                    for (const auto &oChild : oChildren)
                    {
                        AddRaster(oChild);
                    }
                }

                SetupRasterDSWrapper(OGREnvelope());
            }
            else
            {
                bResult = false;
            }

            // TODO: Add support for wfsserver_service, wmsserver_service,
            // raster_mosaic, tileset.
        }
    }

    return bResult;
}

/*
 * FillResources()
 */
bool OGRNGWDataset::FillResources(const CPLStringList &aosHTTPOptions,
                                  int nOpenFlagsIn)
{
    CPLJSONDocument oResourceDetailsReq;
    // Fill domains
    bool bResult = oResourceDetailsReq.LoadUrl(
        NGWAPI::GetSearchURL(osUrl, "cls", "lookup_table"), aosHTTPOptions);
    if (bResult)
    {
        CPLJSONArray oChildren(oResourceDetailsReq.GetRoot());
        for (const auto &oChild : oChildren)
        {
            OGRNGWCodedFieldDomain oDomain(oChild);
            if (oDomain.GetID() > 0)
            {
                moDomains[oDomain.GetID()] = oDomain;
            }
        }
    }

    // Fill child resources
    bResult = oResourceDetailsReq.LoadUrl(
        NGWAPI::GetChildrenURL(osUrl, osResourceId), aosHTTPOptions);

    if (bResult)
    {
        CPLJSONArray oChildren(oResourceDetailsReq.GetRoot());
        for (const auto &oChild : oChildren)
        {
            if (nOpenFlagsIn & GDAL_OF_VECTOR)
            {
                // Add vector layer. If failed, try next layer.
                AddLayer(oChild, aosHTTPOptions, nOpenFlagsIn);
            }

            if (nOpenFlagsIn & GDAL_OF_RASTER)
            {
                AddRaster(oChild);
            }
        }
    }
    return bResult;
}

/*
 * AddLayer()
 */
void OGRNGWDataset::AddLayer(const CPLJSONObject &oResourceJsonObject,
                             const CPLStringList &aosHTTPOptions,
                             int nOpenFlagsIn)
{
    auto osResourceType = oResourceJsonObject.GetString("resource/cls");
    if (!NGWAPI::CheckSupportedType(false, osResourceType))
    {
        // NOTE: Only vector_layer and postgis_layer types now supported
        return;
    }

    auto osLayerResourceId = oResourceJsonObject.GetString("resource/id");
    if (nOpenFlagsIn & GDAL_OF_VECTOR)
    {
        OGRNGWLayerPtr poLayer(new OGRNGWLayer(this, oResourceJsonObject));
        aoLayers.emplace_back(poLayer);
        osLayerResourceId = poLayer->GetResourceId();
    }

    // Check styles exist and add them as rasters.
    if (nOpenFlagsIn & GDAL_OF_RASTER &&
        oResourceJsonObject.GetBool("resource/children", false))
    {
        CPLJSONDocument oResourceChildReq;
        bool bResult = oResourceChildReq.LoadUrl(
            NGWAPI::GetChildrenURL(osUrl, osLayerResourceId), aosHTTPOptions);

        if (bResult)
        {
            CPLJSONArray oChildren(oResourceChildReq.GetRoot());
            for (const auto &oChild : oChildren)
            {
                AddRaster(oChild);
            }
        }
    }
}

/*
 * AddRaster()
 */
void OGRNGWDataset::AddRaster(const CPLJSONObject &oRasterJsonObj)
{
    auto osResourceType = oRasterJsonObj.GetString("resource/cls");
    if (!NGWAPI::CheckSupportedType(true, osResourceType))
    {
        return;
    }

    auto osOutResourceId = oRasterJsonObj.GetString("resource/id");
    auto osOutResourceName = oRasterJsonObj.GetString("resource/display_name");

    if (osOutResourceName.empty())
    {
        osOutResourceName = "raster_" + osOutResourceId;
    }

    CPLDebug("NGW", "Add raster %s: %s", osOutResourceId.c_str(),
             osOutResourceName.c_str());

    GDALDataset::SetMetadataItem(CPLSPrintf("SUBDATASET_%d_NAME", nRasters + 1),
                                 CPLSPrintf("NGW:%s/resource/%s", osUrl.c_str(),
                                            osOutResourceId.c_str()),
                                 "SUBDATASETS");
    GDALDataset::SetMetadataItem(CPLSPrintf("SUBDATASET_%d_DESC", nRasters + 1),
                                 CPLSPrintf("%s (%s)",
                                            osOutResourceName.c_str(),
                                            osResourceType.c_str()),
                                 "SUBDATASETS");
    nRasters++;
}

/*
 * ICreateLayer
 */
OGRLayer *OGRNGWDataset::ICreateLayer(const char *pszNameIn,
                                      const OGRGeomFieldDefn *poGeomFieldDefn,
                                      CSLConstList papszOptions)
{
    if (!IsUpdateMode())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return nullptr;
    }

    const auto eGType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSpatialRef =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    // Check permissions as we create new layer in memory and will create in
    // during SyncToDisk.
    FetchPermissions();

    if (!stPermissions.bResourceCanCreate)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
        return nullptr;
    }

    // Check input parameters.
    if ((eGType < wkbPoint || eGType > wkbMultiPolygon) &&
        (eGType < wkbPoint25D || eGType > wkbMultiPolygon25D))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unsupported geometry type: %s",
                 OGRGeometryTypeToName(eGType));
        return nullptr;
    }

    if (!poSpatialRef)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Undefined spatial reference");
        return nullptr;
    }

    OGRSpatialReference *poSRSClone = poSpatialRef->Clone();
    poSRSClone->AutoIdentifyEPSG();
    const char *pszEPSG = poSRSClone->GetAuthorityCode(nullptr);
    int nEPSG = -1;
    if (pszEPSG != nullptr)
    {
        nEPSG = atoi(pszEPSG);
    }

    if (nEPSG != 3857)  // TODO: Check NextGIS Web supported SRS.
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported spatial reference EPSG code: %d", nEPSG);
        poSRSClone->Release();
        return nullptr;
    }

    // Do we already have this layer?  If so, should we blow it away?
    bool bOverwrite = CPLFetchBool(papszOptions, "OVERWRITE", false);
    for (int iLayer = 0; iLayer < GetLayerCount(); ++iLayer)
    {
        if (EQUAL(pszNameIn, aoLayers[iLayer]->GetName()))
        {
            if (bOverwrite)
            {
                DeleteLayer(iLayer);
                break;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer %s already exists, CreateLayer failed.\n"
                         "Use the layer creation option OVERWRITE=YES to "
                         "replace it.",
                         pszNameIn);
                poSRSClone->Release();
                return nullptr;
            }
        }
    }

    // Create layer.
    std::string osKey = CSLFetchNameValueDef(papszOptions, "KEY", "");
    std::string osDesc = CSLFetchNameValueDef(papszOptions, "DESCRIPTION", "");
    poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    OGRNGWLayerPtr poLayer(
        new OGRNGWLayer(this, pszNameIn, poSRSClone, eGType, osKey, osDesc));
    poSRSClone->Release();
    aoLayers.emplace_back(poLayer);
    return poLayer.get();
}

/*
 * DeleteLayer()
 */
OGRErr OGRNGWDataset::DeleteLayer(int iLayer)
{
    if (!IsUpdateMode())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode.");
        return OGRERR_FAILURE;
    }

    if (iLayer < 0 || iLayer >= GetLayerCount())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Layer %d not in legal range of 0 to %d.", iLayer,
                 GetLayerCount() - 1);
        return OGRERR_FAILURE;
    }

    auto poLayer = aoLayers[iLayer];
    if (poLayer->GetResourceId() != "-1")
    {
        // For layers from server we can check permissions.

        // We can skip check permissions here as papoLayers[iLayer]->Delete()
        // will return false if no delete permission available.
        FetchPermissions();

        if (!stPermissions.bResourceCanDelete)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
            return OGRERR_FAILURE;
        }
    }

    if (poLayer->Delete())
    {
        aoLayers.erase(aoLayers.begin() + iLayer);
    }

    return OGRERR_NONE;
}

/*
 * FillMetadata()
 */
void OGRNGWDataset::FillMetadata(const CPLJSONObject &oRootObject)
{
    std::string osCreateDate = oRootObject.GetString("resource/creation_date");
    if (!osCreateDate.empty())
    {
        GDALDataset::SetMetadataItem("creation_date", osCreateDate.c_str());
    }
    osName = oRootObject.GetString("resource/display_name");
    SetDescription(osName.c_str());
    GDALDataset::SetMetadataItem("display_name", osName.c_str());
    std::string osDescription = oRootObject.GetString("resource/description");
    if (!osDescription.empty())
    {
        GDALDataset::SetMetadataItem("description", osDescription.c_str());
    }
    std::string osResourceType = oRootObject.GetString("resource/cls");
    if (!osResourceType.empty())
    {
        GDALDataset::SetMetadataItem("resource_type", osResourceType.c_str());
    }
    std::string osResourceParentId =
        oRootObject.GetString("resource/parent/id");
    if (!osResourceParentId.empty())
    {
        GDALDataset::SetMetadataItem("parent_id", osResourceParentId.c_str());
    }
    GDALDataset::SetMetadataItem("id", osResourceId.c_str());

    std::vector<CPLJSONObject> items =
        oRootObject.GetObj("resmeta/items").GetChildren();

    for (const CPLJSONObject &item : items)
    {
        std::string osSuffix = NGWAPI::GetResmetaSuffix(item.GetType());
        GDALDataset::SetMetadataItem((item.GetName() + osSuffix).c_str(),
                                     item.ToString().c_str(), "NGW");
    }
}

/*
 * FlushMetadata()
 */
bool OGRNGWDataset::FlushMetadata(char **papszMetadata)
{
    if (!bMetadataDerty)
    {
        return true;
    }

    bool bResult = NGWAPI::FlushMetadata(osUrl, osResourceId, papszMetadata,
                                         GetHeaders(false));
    if (bResult)
    {
        bMetadataDerty = false;
    }

    return bResult;
}

/*
 * SetMetadata()
 */
CPLErr OGRNGWDataset::SetMetadata(char **papszMetadata, const char *pszDomain)
{
    FetchPermissions();
    if (!stPermissions.bMetadataCanWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
        return CE_Failure;
    }

    CPLErr eResult = GDALDataset::SetMetadata(papszMetadata, pszDomain);
    if (eResult == CE_None && pszDomain != nullptr && EQUAL(pszDomain, "NGW"))
    {
        eResult = FlushMetadata(papszMetadata) ? CE_None : CE_Failure;
    }
    return eResult;
}

/*
 * SetMetadataItem()
 */
CPLErr OGRNGWDataset::SetMetadataItem(const char *pszName, const char *pszValue,
                                      const char *pszDomain)
{
    FetchPermissions();
    if (!stPermissions.bMetadataCanWrite)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
        return CE_Failure;
    }
    if (pszDomain != nullptr && EQUAL(pszDomain, "NGW"))
    {
        bMetadataDerty = true;
    }
    return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/*
 * FlushCache()
 */
CPLErr OGRNGWDataset::FlushCache(bool bAtClosing)
{
    CPLErr eErr = GDALDataset::FlushCache(bAtClosing);
    if (!FlushMetadata(GetMetadata("NGW")))
        eErr = CE_Failure;
    return eErr;
}

/*
 * GetHeaders()
 */
CPLStringList OGRNGWDataset::GetHeaders(bool bSkipRetry) const
{
    CPLStringList aosOptions;
    aosOptions.AddNameValue("HEADERS", "Accept: */*");
    aosOptions.AddNameValue("JSON_DEPTH", osJsonDepth.c_str());
    if (!osUserPwd.empty())
    {
        aosOptions.AddNameValue("HTTPAUTH", "BASIC");
        aosOptions.AddNameValue("USERPWD", osUserPwd.c_str());
    }

    if (!osConnectTimeout.empty())
    {
        aosOptions.AddNameValue("CONNECTTIMEOUT", osConnectTimeout.c_str());
    }

    if (!osTimeout.empty())
    {
        aosOptions.AddNameValue("TIMEOUT", osTimeout.c_str());
    }

    if (!bSkipRetry)
    {
        if (!osRetryCount.empty())
        {
            aosOptions.AddNameValue("MAX_RETRY", osRetryCount.c_str());
        }
        if (!osRetryDelay.empty())
        {
            aosOptions.AddNameValue("RETRY_DELAY", osRetryDelay.c_str());
        }
    }
    return aosOptions;
}

/*
 * SQLUnescape()
 * Get from gdal/ogr/ogrsf_frmts/sqlite/ogrsqliteutility.cpp as we don't want
 * dependency on sqlite
 */
static CPLString SQLUnescape(const char *pszVal)
{
    char chQuoteChar = pszVal[0];
    if (chQuoteChar != '\'' && chQuoteChar != '"')
        return pszVal;

    CPLString osRet;
    pszVal++;
    while (*pszVal != '\0')
    {
        if (*pszVal == chQuoteChar)
        {
            if (pszVal[1] == chQuoteChar)
                pszVal++;
            else
                break;
        }
        osRet += *pszVal;
        pszVal++;
    }
    return osRet;
}

/*
 * SQLTokenize()
 * Get from gdal/ogr/ogrsf_frmts/sqlite/ogrsqliteutility.cpp as we don't want
 * dependency on sqlite
 */
static char **SQLTokenize(const char *pszStr)
{
    char **papszTokens = nullptr;
    bool bInQuote = false;
    char chQuoteChar = '\0';
    bool bInSpace = true;
    CPLString osCurrentToken;
    while (*pszStr != '\0')
    {
        if (*pszStr == ' ' && !bInQuote)
        {
            if (!bInSpace)
            {
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
            }
            bInSpace = true;
        }
        else if ((*pszStr == '(' || *pszStr == ')' || *pszStr == ',') &&
                 !bInQuote)
        {
            if (!bInSpace)
            {
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
            }
            osCurrentToken.clear();
            osCurrentToken += *pszStr;
            papszTokens = CSLAddString(papszTokens, osCurrentToken);
            osCurrentToken.clear();
            bInSpace = true;
        }
        else if (*pszStr == '"' || *pszStr == '\'')
        {
            if (bInQuote && *pszStr == chQuoteChar && pszStr[1] == chQuoteChar)
            {
                osCurrentToken += *pszStr;
                osCurrentToken += *pszStr;
                pszStr += 2;
                continue;
            }
            else if (bInQuote && *pszStr == chQuoteChar)
            {
                osCurrentToken += *pszStr;
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
                bInSpace = true;
                bInQuote = false;
                chQuoteChar = '\0';
            }
            else if (bInQuote)
            {
                osCurrentToken += *pszStr;
            }
            else
            {
                chQuoteChar = *pszStr;
                osCurrentToken.clear();
                osCurrentToken += chQuoteChar;
                bInQuote = true;
                bInSpace = false;
            }
        }
        else
        {
            osCurrentToken += *pszStr;
            bInSpace = false;
        }
        pszStr++;
    }

    if (!osCurrentToken.empty())
        papszTokens = CSLAddString(papszTokens, osCurrentToken);

    return papszTokens;
}

/*
 * ExecuteSQL()
 */
OGRLayer *OGRNGWDataset::ExecuteSQL(const char *pszStatement,
                                    OGRGeometry *poSpatialFilter,
                                    const char *pszDialect)
{
    // Clean statement string.
    CPLString osStatement(pszStatement);
    osStatement = osStatement.Trim().replaceAll("  ", " ");

    if (STARTS_WITH_CI(osStatement, "DELLAYER:"))
    {
        CPLString osLayerName = osStatement.substr(strlen("DELLAYER:"));
        if (osLayerName.endsWith(";"))
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 1);
            osLayerName.Trim();
        }

        CPLDebug("NGW", "Delete layer with name %s.", osLayerName.c_str());

        for (int iLayer = 0; iLayer < GetLayerCount(); ++iLayer)
        {
            if (EQUAL(aoLayers[iLayer]->GetName(), osLayerName))
            {
                DeleteLayer(iLayer);
                return nullptr;
            }
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer : %s",
                 osLayerName.c_str());

        return nullptr;
    }

    if (STARTS_WITH_CI(osStatement, "DELETE FROM"))
    {
        osStatement = osStatement.substr(strlen("DELETE FROM "));
        if (osStatement.endsWith(";"))
        {
            osStatement = osStatement.substr(0, osStatement.size() - 1);
            osStatement.Trim();
        }

        std::size_t found = osStatement.find("WHERE");
        CPLString osLayerName;
        if (found == std::string::npos)
        {  // No where clause
            osLayerName = osStatement;
            osStatement.clear();
        }
        else
        {
            osLayerName = osStatement.substr(0, found);
            osLayerName.Trim();
            osStatement = osStatement.substr(found + strlen("WHERE "));
        }

        OGRNGWLayer *poLayer =
            reinterpret_cast<OGRNGWLayer *>(GetLayerByName(osLayerName));
        if (nullptr == poLayer)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Layer %s not found in dataset.", osName.c_str());
            return nullptr;
        }

        if (osStatement.empty())
        {
            poLayer->DeleteAllFeatures();
        }
        else
        {
            CPLDebug("NGW", "Delete features with statement %s",
                     osStatement.c_str());
            OGRFeatureQuery oQuery;
            OGRErr eErr = oQuery.Compile(poLayer->GetLayerDefn(), osStatement);
            if (eErr != OGRERR_NONE)
            {
                return nullptr;
            }

            // Ignore all fields except first and ignore geometry
            auto poLayerDefn = poLayer->GetLayerDefn();
            poLayerDefn->SetGeometryIgnored(TRUE);
            if (poLayerDefn->GetFieldCount() > 0)
            {
                std::set<std::string> osFields;
                OGRFieldDefn *poFieldDefn = poLayerDefn->GetFieldDefn(0);
                osFields.insert(poFieldDefn->GetNameRef());
                poLayer->SetSelectedFields(osFields);
            }
            CPLString osNgwDelete =
                "NGW:" +
                OGRNGWLayer::TranslateSQLToFilter(
                    reinterpret_cast<swq_expr_node *>(oQuery.GetSWQExpr()));

            poLayer->SetAttributeFilter(osNgwDelete);

            std::vector<GIntBig> aiFeaturesIDs;
            OGRFeature *poFeat;
            while ((poFeat = poLayer->GetNextFeature()) != nullptr)
            {
                aiFeaturesIDs.push_back(poFeat->GetFID());
                OGRFeature::DestroyFeature(poFeat);
            }

            poLayer->DeleteFeatures(aiFeaturesIDs);

            // Reset all filters and ignores
            poLayerDefn->SetGeometryIgnored(FALSE);
            poLayer->SetAttributeFilter(nullptr);
            poLayer->SetIgnoredFields(nullptr);
        }
        return nullptr;
    }

    if (STARTS_WITH_CI(osStatement, "DROP TABLE"))
    {
        // Get layer name from pszStatement DELETE FROM layer;.
        CPLString osLayerName = osStatement.substr(strlen("DROP TABLE "));
        if (osLayerName.endsWith(";"))
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 1);
            osLayerName.Trim();
        }

        CPLDebug("NGW", "Delete layer with name %s.", osLayerName.c_str());

        for (int iLayer = 0; iLayer < GetLayerCount(); ++iLayer)
        {
            if (EQUAL(aoLayers[iLayer]->GetName(), osLayerName))
            {
                DeleteLayer(iLayer);
                return nullptr;
            }
        }

        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer : %s",
                 osLayerName.c_str());

        return nullptr;
    }

    if (STARTS_WITH_CI(osStatement, "ALTER TABLE "))
    {
        if (osStatement.endsWith(";"))
        {
            osStatement = osStatement.substr(0, osStatement.size() - 1);
            osStatement.Trim();
        }

        CPLStringList aosTokens(SQLTokenize(osStatement));
        /* ALTER TABLE src_table RENAME TO dst_table */
        if (aosTokens.size() == 6 && EQUAL(aosTokens[3], "RENAME") &&
            EQUAL(aosTokens[4], "TO"))
        {
            const char *pszSrcTableName = aosTokens[2];
            const char *pszDstTableName = aosTokens[5];

            OGRNGWLayer *poLayer = static_cast<OGRNGWLayer *>(
                GetLayerByName(SQLUnescape(pszSrcTableName)));
            if (poLayer)
            {
                poLayer->Rename(SQLUnescape(pszDstTableName));
                return nullptr;
            }

            CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer : %s",
                     pszSrcTableName);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unsupported alter table operation. Only rename table to "
                     "... support.");
        }
        return nullptr;
    }

    // SELECT xxxxx FROM yyyy WHERE zzzzzz;
    if (STARTS_WITH_CI(osStatement, "SELECT "))
    {
        swq_select oSelect;
        CPLDebug("NGW", "Select statement: %s", osStatement.c_str());
        if (oSelect.preparse(osStatement) != CE_None)
        {
            return nullptr;
        }

        if (oSelect.join_count == 0 && oSelect.poOtherSelect == nullptr &&
            oSelect.table_count == 1 && oSelect.order_specs == 0)
        {
            OGRNGWLayer *poLayer = reinterpret_cast<OGRNGWLayer *>(
                GetLayerByName(oSelect.table_defs[0].table_name));
            if (nullptr == poLayer)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Layer %s not found in dataset.",
                         oSelect.table_defs[0].table_name);
                return nullptr;
            }

            std::set<std::string> aosFields;
            bool bSkip = false;
            for (int i = 0; i < oSelect.result_columns(); ++i)
            {
                swq_col_func col_func = oSelect.column_defs[i].col_func;
                if (col_func != SWQCF_NONE)
                {
                    bSkip = true;
                    break;
                }

                if (oSelect.column_defs[i].distinct_flag)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Distinct not supported.");
                    bSkip = true;
                    break;
                }

                if (oSelect.column_defs[i].field_name != nullptr)
                {
                    if (EQUAL(oSelect.column_defs[i].field_name, "*"))
                    {
                        aosFields.clear();
                        aosFields.emplace(oSelect.column_defs[i].field_name);
                        break;
                    }
                    else
                    {
                        aosFields.emplace(oSelect.column_defs[i].field_name);
                    }
                }
            }

            std::string osNgwSelect;
            for (int iKey = 0; iKey < oSelect.order_specs; iKey++)
            {
                swq_order_def *psKeyDef = oSelect.order_defs + iKey;
                if (iKey > 0)
                {
                    osNgwSelect += ",";
                }

                if (psKeyDef->ascending_flag == TRUE)
                {
                    osNgwSelect += psKeyDef->field_name;
                }
                else
                {
                    osNgwSelect += "-" + std::string(psKeyDef->field_name);
                }
            }

            if (oSelect.where_expr != nullptr)
            {
                if (!osNgwSelect.empty())
                {
                    osNgwSelect += "&";
                }
                osNgwSelect +=
                    OGRNGWLayer::TranslateSQLToFilter(oSelect.where_expr);
            }

            if (osNgwSelect.empty())
            {
                bSkip = true;
            }

            if (!bSkip)
            {
                if (aosFields.empty())
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "SELECT statement is invalid: field list is empty.");
                    return nullptr;
                }

                if (poLayer->SyncToDisk() != OGRERR_NONE)
                {
                    return nullptr;
                }

                OGRNGWLayer *poOutLayer = poLayer->Clone();
                if (aosFields.size() == 1 && *(aosFields.begin()) == "*")
                {
                    poOutLayer->SetIgnoredFields(nullptr);
                }
                else
                {
                    poOutLayer->SetSelectedFields(aosFields);
                }
                poOutLayer->SetSpatialFilter(poSpatialFilter);

                if (osNgwSelect
                        .empty())  // If we here oSelect.where_expr is empty
                {
                    poOutLayer->SetAttributeFilter(nullptr);
                }
                else
                {
                    std::string osAttributeFilte = "NGW:" + osNgwSelect;
                    poOutLayer->SetAttributeFilter(osAttributeFilte.c_str());
                }
                return poOutLayer;
            }
        }
    }

    return GDALDataset::ExecuteSQL(pszStatement, poSpatialFilter, pszDialect);
}

/*
 * GetProjectionRef()
 */
const OGRSpatialReference *OGRNGWDataset::GetSpatialRef() const
{
    if (poRasterDS != nullptr)
    {
        return poRasterDS->GetSpatialRef();
    }
    return GDALDataset::GetSpatialRef();
}

/*
 * GetGeoTransform()
 */
CPLErr OGRNGWDataset::GetGeoTransform(double *padfTransform)
{
    if (poRasterDS != nullptr)
    {
        return poRasterDS->GetGeoTransform(padfTransform);
    }
    return GDALDataset::GetGeoTransform(padfTransform);
}

/*
 * IRasterIO()
 */
CPLErr OGRNGWDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                int nXSize, int nYSize, void *pData,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eBufType, int nBandCount,
                                BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                                GSpacing nLineSpace, GSpacing nBandSpace,
                                GDALRasterIOExtraArg *psExtraArg)
{
    if (poRasterDS != nullptr)
    {
        if (stPixelExtent.IsInit())
        {
            OGREnvelope stTestExtent;
            stTestExtent.MinX = static_cast<double>(nXOff);
            stTestExtent.MinY = static_cast<double>(nYOff);
            stTestExtent.MaxX = static_cast<double>(nXOff + nXSize);
            stTestExtent.MaxY = static_cast<double>(nYOff + nYSize);

            if (!stPixelExtent.Intersects(stTestExtent))
            {
                CPLDebug("NGW", "Raster extent in px is: %f, %f, %f, %f",
                         stPixelExtent.MinX, stPixelExtent.MinY,
                         stPixelExtent.MaxX, stPixelExtent.MaxY);
                CPLDebug("NGW", "RasterIO extent is: %f, %f, %f, %f",
                         stTestExtent.MinX, stTestExtent.MinY,
                         stTestExtent.MaxX, stTestExtent.MaxY);

                // Fill buffer transparent color.
                memset(pData, 0,
                       static_cast<size_t>(nBufXSize) * nBufYSize * nBandCount *
                           GDALGetDataTypeSizeBytes(eBufType));
                return CE_None;
            }
        }
    }
    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpace, nLineSpace,
                                  nBandSpace, psExtraArg);
}

/*
 * FillCapabilities()
 */
void OGRNGWDataset::FillCapabilities(const CPLStringList &aosHTTPOptions)
{
    // Check NGW version. Paging available from 3.1
    CPLJSONDocument oRouteReq;
    if (oRouteReq.LoadUrl(NGWAPI::GetVersionURL(osUrl), aosHTTPOptions))
    {
        CPLJSONObject oRoot = oRouteReq.GetRoot();

        if (oRoot.IsValid())
        {
            std::string osVersion = oRoot.GetString("nextgisweb", "0.0");
            bHasFeaturePaging = NGWAPI::CheckVersion(osVersion, 3, 1);

            CPLDebug("NGW", "Is feature paging supported: %s",
                     bHasFeaturePaging ? "yes" : "no");
        }
    }
}

/*
 * Extensions()
 */
std::string OGRNGWDataset::Extensions() const
{
    return osExtensions;
}

/*
 * GetFieldDomainNames()
 */
std::vector<std::string> OGRNGWDataset::GetFieldDomainNames(CSLConstList) const
{
    std::vector<std::string> oDomainNamesList;
    std::array<OGRFieldType, 3> aeFieldTypes{OFTString, OFTInteger,
                                             OFTInteger64};
    for (auto const &oDom : moDomains)
    {
        for (auto eFieldType : aeFieldTypes)
        {
            auto pOgrDom = oDom.second.ToFieldDomain(eFieldType);
            if (pOgrDom != nullptr)
            {
                oDomainNamesList.emplace_back(pOgrDom->GetName());
            }
        }
    }
    return oDomainNamesList;
}

/*
 * GetFieldDomain()
 */
const OGRFieldDomain *
OGRNGWDataset::GetFieldDomain(const std::string &name) const
{
    std::array<OGRFieldType, 3> aeFieldTypes{OFTString, OFTInteger,
                                             OFTInteger64};
    for (auto const &oDom : moDomains)
    {
        for (auto eFieldType : aeFieldTypes)
        {
            auto pOgrDom = oDom.second.ToFieldDomain(eFieldType);
            if (pOgrDom != nullptr)
            {
                if (pOgrDom->GetName() == name)
                {
                    return pOgrDom;
                }
            }
        }
    }
    return nullptr;
}

/*
 * DeleteFieldDomain()
 */
bool OGRNGWDataset::DeleteFieldDomain(const std::string &name,
                                      std::string &failureReason)
{
    if (eAccess != GA_Update)
    {
        failureReason =
            "DeleteFieldDomain() not supported on read-only dataset";
        return false;
    }

    std::array<OGRFieldType, 3> aeFieldTypes{OFTString, OFTInteger,
                                             OFTInteger64};
    for (auto const &oDom : moDomains)
    {
        for (auto eFieldType : aeFieldTypes)
        {
            auto pOgrDom = oDom.second.ToFieldDomain(eFieldType);
            if (pOgrDom != nullptr)
            {
                if (pOgrDom->GetName() == name)
                {
                    auto nResourceID = oDom.second.GetID();

                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Delete following domains with common "
                             "identifier " CPL_FRMT_GIB ": %s.",
                             nResourceID,
                             oDom.second.GetDomainsNames().c_str());

                    auto result = NGWAPI::DeleteResource(
                        GetUrl(), std::to_string(nResourceID),
                        GetHeaders(false));
                    if (!result)
                    {
                        failureReason = CPLGetLastErrorMsg();
                        return result;
                    }

                    moDomains.erase(nResourceID);

                    // Remove domain from fields definitions
                    for (const auto &oLayer : aoLayers)
                    {
                        for (int i = 0;
                             i < oLayer->GetLayerDefn()->GetFieldCount(); ++i)
                        {
                            OGRFieldDefn *poFieldDefn =
                                oLayer->GetLayerDefn()->GetFieldDefn(i);
                            if (oDom.second.HasDomainName(
                                    poFieldDefn->GetDomainName()))
                            {
                                auto oTemporaryUnsealer(
                                    poFieldDefn->GetTemporaryUnsealer());
                                poFieldDefn->SetDomainName(std::string());
                            }
                        }
                    }
                    return true;
                }
            }
        }
    }
    failureReason = "Domain does not exist";
    return false;
}

/*
 * CreateNGWLookupTableJson()
 */
static std::string CreateNGWLookupTableJson(const OGRCodedFieldDomain *pDomain,
                                            GIntBig nResourceId)
{
    CPLJSONObject oResourceJson;
    // Add resource json item.
    CPLJSONObject oResource("resource", oResourceJson);
    oResource.Add("cls", "lookup_table");
    CPLJSONObject oResourceParent("parent", oResource);
    oResourceParent.Add("id", nResourceId);
    oResource.Add("display_name", pDomain->GetName());
    oResource.Add("description", pDomain->GetDescription());

    // Add vector_layer json item.
    CPLJSONObject oLookupTable("lookup_table", oResourceJson);
    CPLJSONObject oLookupTableItems("items", oLookupTable);
    const auto enumeration = pDomain->GetEnumeration();
    for (int i = 0; enumeration[i].pszCode != nullptr; ++i)
    {
        const char *pszValCurrent = "";
        // NGW not supported null as coded value, so set it as ""
        if (enumeration[i].pszValue != nullptr)
        {
            pszValCurrent = enumeration[i].pszValue;
        }
        oLookupTableItems.Add(enumeration[i].pszCode, pszValCurrent);
    }

    return oResourceJson.Format(CPLJSONObject::PrettyFormat::Plain);
}

/*
 * AddFieldDomain()
 */
bool OGRNGWDataset::AddFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                                   std::string &failureReason)
{
    const std::string domainName(domain->GetName());
    if (eAccess != GA_Update)
    {
        failureReason = "Add field domain not supported on read-only dataset";
        return false;
    }

    if (GetFieldDomain(domainName) != nullptr)
    {
        failureReason = "A domain of identical name already exists";
        return false;
    }

    if (domain->GetDomainType() != OFDT_CODED)
    {
        failureReason = "Unsupported domain type";
        return false;
    }

    auto osPalyload = CreateNGWLookupTableJson(
        static_cast<OGRCodedFieldDomain *>(domain.get()),
        static_cast<GIntBig>(std::stol(osResourceId)));

    std::string osResourceIdInt =
        NGWAPI::CreateResource(osUrl, osPalyload, GetHeaders());
    if (osResourceIdInt == "-1")
    {
        failureReason = CPLGetLastErrorMsg();
        return false;
    }
    auto osNewResourceUrl = NGWAPI::GetResourceURL(osUrl, osResourceIdInt);
    CPLJSONDocument oResourceDetailsReq;
    bool bResult =
        oResourceDetailsReq.LoadUrl(osNewResourceUrl, GetHeaders(false));
    if (!bResult)
    {
        failureReason = CPLGetLastErrorMsg();
        return false;
    }

    OGRNGWCodedFieldDomain oDomain(oResourceDetailsReq.GetRoot());
    if (oDomain.GetID() == 0)
    {
        failureReason = "Failed to parse domain detailes from NGW";
        return false;
    }
    moDomains[oDomain.GetID()] = oDomain;
    return true;
}

/*
 * UpdateFieldDomain()
 */
bool OGRNGWDataset::UpdateFieldDomain(std::unique_ptr<OGRFieldDomain> &&domain,
                                      std::string &failureReason)
{
    const std::string domainName(domain->GetName());
    if (eAccess != GA_Update)
    {
        failureReason = "Add field domain not supported on read-only dataset";
        return false;
    }

    if (GetFieldDomain(domainName) == nullptr)
    {
        failureReason = "The domain should already exist to be updated";
        return false;
    }

    if (domain->GetDomainType() != OFDT_CODED)
    {
        failureReason = "Unsupported domain type";
        return false;
    }

    auto nResourceId = GetDomainIdByName(domainName);
    if (nResourceId == 0)
    {
        failureReason = "Failed get NGW domain identifier";
        return false;
    }

    auto osPayload = CreateNGWLookupTableJson(
        static_cast<const OGRCodedFieldDomain *>(domain.get()),
        static_cast<GIntBig>(std::stol(osResourceId)));

    if (!NGWAPI::UpdateResource(osUrl, osResourceId, osPayload, GetHeaders()))
    {
        failureReason = CPLGetLastErrorMsg();
        return false;
    }

    auto osNewResourceUrl = NGWAPI::GetResourceURL(osUrl, osResourceId);
    CPLJSONDocument oResourceDetailsReq;
    bool bResult =
        oResourceDetailsReq.LoadUrl(osNewResourceUrl, GetHeaders(false));
    if (!bResult)
    {
        failureReason = CPLGetLastErrorMsg();
        return false;
    }

    OGRNGWCodedFieldDomain oDomain(oResourceDetailsReq.GetRoot());
    if (oDomain.GetID() == 0)
    {
        failureReason = "Failed to parse domain detailes from NGW";
        return false;
    }
    moDomains[oDomain.GetID()] = oDomain;
    return true;
}

/*
 * GetDomainByID()
 */
OGRNGWCodedFieldDomain OGRNGWDataset::GetDomainByID(GIntBig id) const
{
    auto pos = moDomains.find(id);
    if (pos == moDomains.end())
    {
        return OGRNGWCodedFieldDomain();
    }
    else
    {
        return pos->second;
    }
}

/*
 *  GetDomainIdByName()
 */
GIntBig OGRNGWDataset::GetDomainIdByName(const std::string &osDomainName) const
{
    for (auto const &oDom : moDomains)
    {
        if (oDom.second.HasDomainName(osDomainName))
        {
            return oDom.first;
        }
    }
    return 0L;
}
