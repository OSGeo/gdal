/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018-2020, NextGIS <info@nextgis.com>
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
#include "gdal_proxy.h"

class NGWWrapperRasterBand : public GDALProxyRasterBand
{
    GDALRasterBand *poBaseBand;

protected:
    virtual GDALRasterBand *RefUnderlyingRasterBand() const override { return poBaseBand; }

public:
    explicit NGWWrapperRasterBand( GDALRasterBand* poBaseBandIn ) :
        poBaseBand( poBaseBandIn )
    {
        eDataType = poBaseBand->GetRasterDataType();
        poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }
    virtual ~NGWWrapperRasterBand() {}
};

/*
 * OGRNGWDataset()
 */
OGRNGWDataset::OGRNGWDataset() :
    nBatchSize(-1),
    nPageSize(-1),
    bFetchedPermissions(false),
    bHasFeaturePaging(false),
    bExtInNativeData(false),
    bMetadataDerty(false),
    papoLayers(nullptr),
    nLayers(0),
    poRasterDS(nullptr),
    nRasters(0),
    nCacheExpires(604800),  // 7 days
    nCacheMaxSize(67108864),// 64 MB
    osJsonDepth("32")
{}

/*
 * ~OGRNGWDataset()
 */
OGRNGWDataset::~OGRNGWDataset()
{
    // Last sync with server.
    OGRNGWDataset::FlushCache(true);

    if( poRasterDS != nullptr )
    {
        GDALClose( poRasterDS );
        poRasterDS = nullptr;
    }

    for( int i = 0; i < nLayers; ++i )
    {
        delete papoLayers[i];
    }
    CPLFree( papoLayers );
}

/*
 * FetchPermissions()
 */
void OGRNGWDataset::FetchPermissions()
{
    if( bFetchedPermissions )
    {
        return;
    }

    if( IsUpdateMode() )
    {
        // Check connection and is it read only.
        char **papszHTTPOptions = GetHeaders();
        stPermissions = NGWAPI::CheckPermissions( osUrl, osResourceId,
            papszHTTPOptions, IsUpdateMode() );
        CSLDestroy( papszHTTPOptions );
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
int OGRNGWDataset::TestCapability( const char *pszCap )
{
    FetchPermissions();
    if( EQUAL(pszCap, ODsCCreateLayer) )
    {
        return stPermissions.bResourceCanCreate;
    }
    else if( EQUAL(pszCap, ODsCDeleteLayer) )
    {
        return stPermissions.bResourceCanDelete;
    }
    else if( EQUAL(pszCap, "RenameLayer") )
    {
        return stPermissions.bResourceCanUpdate;
    }
    else if( EQUAL(pszCap, ODsCRandomLayerWrite) )
    {
        return stPermissions.bDataCanWrite; // FIXME: Check on resource level is this permission set?
    }
    else if( EQUAL(pszCap, ODsCRandomLayerRead) )
    {
        return stPermissions.bDataCanRead;
    }
    else
    {
        return FALSE;
    }
}

/*
 * GetLayer()
 */
OGRLayer *OGRNGWDataset::GetLayer( int iLayer )
{
    if( iLayer < 0 || iLayer >= nLayers )
    {
        return nullptr;
    }
    else
    {
        return papoLayers[iLayer];
    }
}

/*
 * Open()
 */
bool OGRNGWDataset::Open( const std::string &osUrlIn,
    const std::string &osResourceIdIn, char **papszOpenOptionsIn,
    bool bUpdateIn, int nOpenFlagsIn )
{
    osUrl = osUrlIn;
    osResourceId = osResourceIdIn;

    eAccess = bUpdateIn ? GA_Update : GA_ReadOnly;

    osUserPwd = CSLFetchNameValueDef( papszOpenOptionsIn, "USERPWD",
        CPLGetConfigOption("NGW_USERPWD", ""));

    nBatchSize = atoi( CSLFetchNameValueDef( papszOpenOptionsIn,
        "BATCH_SIZE", CPLGetConfigOption("NGW_BATCH_SIZE", "-1") ) );

    nPageSize = atoi( CSLFetchNameValueDef(papszOpenOptionsIn, "PAGE_SIZE",
        CPLGetConfigOption("NGW_PAGE_SIZE", "-1") ) );
    if( nPageSize == 0 )
    {
        nPageSize = -1;
    }

    nCacheExpires = atoi( CSLFetchNameValueDef(papszOpenOptionsIn, "CACHE_EXPIRES",
        CPLGetConfigOption("NGW_CACHE_EXPIRES", "604800") ) );

    nCacheMaxSize = atoi( CSLFetchNameValueDef(papszOpenOptionsIn, "CACHE_MAX_SIZE",
        CPLGetConfigOption("NGW_CACHE_MAX_SIZE", "67108864") ) );

    bExtInNativeData = CPLFetchBool( papszOpenOptionsIn, "NATIVE_DATA",
        CPLTestBool( CPLGetConfigOption("NGW_NATIVE_DATA", "NO") ) );

    osJsonDepth = CSLFetchNameValueDef( papszOpenOptionsIn, "JSON_DEPTH",
        CPLGetConfigOption("NGW_JSON_DEPTH", "32"));

    osExtensions = CSLFetchNameValueDef(papszOpenOptionsIn, "EXTENSIONS",
        CPLGetConfigOption("NGW_EXTENSIONS", ""));

    if (osExtensions.empty())
    {
        bExtInNativeData = false;
    }

    return Init( nOpenFlagsIn );
}

/*
 * Open()
 *
 * The pszFilename templates:
 *      - NGW:http://some.nextgis.com/resource/0
 *      - NGW:http://some.nextgis.com:8000/test/resource/0
 */
bool OGRNGWDataset::Open( const char *pszFilename, char **papszOpenOptionsIn,
    bool bUpdateIn, int nOpenFlagsIn )
{
    NGWAPI::Uri stUri = NGWAPI::ParseUri(pszFilename);

    if( stUri.osPrefix != "NGW" )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "Unsupported name %s", pszFilename);
        return false;
    }

    osUrl = stUri.osAddress;
    osResourceId = stUri.osResourceId;

    return Open( stUri.osAddress, stUri.osResourceId, papszOpenOptionsIn,
        bUpdateIn, nOpenFlagsIn );
}

/*
 * Init()
 */
bool OGRNGWDataset::Init(int nOpenFlagsIn)
{
    // NOTE: Skip check API version at that moment. We expected API v3.

    // Get resource details.
    CPLJSONDocument oResourceDetailsReq;
    char **papszHTTPOptions = GetHeaders();
    bool bResult = oResourceDetailsReq.LoadUrl( NGWAPI::GetResource( osUrl,
        osResourceId ), papszHTTPOptions );

    CPLDebug("NGW", "Get resource %s details %s", osResourceId.c_str(),
        bResult ? "success" : "failed");

    if( bResult )
    {
        CPLJSONObject oRoot = oResourceDetailsReq.GetRoot();

        if( oRoot.IsValid() )
        {
            std::string osResourceType = oRoot.GetString("resource/cls");
            FillMetadata( oRoot );

            if( osResourceType == "resource_group" )
            {
                // Check feature paging.
                FillCapabilities( papszHTTPOptions );
                if( oRoot.GetBool( "resource/children", false ) ) {
                    // Get child resources.
                    bResult = FillResources( papszHTTPOptions, nOpenFlagsIn );
                }
            }
            else if( (osResourceType == "vector_layer" ||
                osResourceType == "postgis_layer") )
            {
                // Check feature paging.
                FillCapabilities( papszHTTPOptions );
                // Add vector layer.
                AddLayer( oRoot, papszHTTPOptions, nOpenFlagsIn );
            }
            else if( osResourceType == "mapserver_style" ||
                osResourceType == "qgis_vector_style" ||
                osResourceType == "raster_style" ||
                osResourceType == "qgis_raster_style" ||
                osResourceType == "wmsclient_layer" )
            {
                // GetExtent from parent.
                OGREnvelope stExtent;
                std::string osParentId = oRoot.GetString("resource/parent/id");
                bool bExtentResult = NGWAPI::GetExtent(osUrl, osParentId,
                    papszHTTPOptions, 3857, stExtent);

                if( !bExtentResult )
                {
                    // Set full extent for EPSG:3857.
                    stExtent.MinX = -20037508.34;
                    stExtent.MaxX = 20037508.34;
                    stExtent.MinY = -20037508.34;
                    stExtent.MaxY = 20037508.34;
                }

                CPLDebug("NGW", "Raster extent is: %f, %f, %f, %f",
                    stExtent.MinX, stExtent.MinY,
                    stExtent.MaxX, stExtent.MaxY);

                int nEPSG = 3857;
                // Get parent details. We can skip this as default SRS in NGW is 3857.
                if( osResourceType == "wmsclient_layer" )
                {
                    nEPSG = oRoot.GetInteger("wmsclient_layer/srs/id", nEPSG);
                }
                else
                {
                    CPLJSONDocument oResourceReq;
                    bResult = oResourceReq.LoadUrl( NGWAPI::GetResource( osUrl,
                        osResourceId ), papszHTTPOptions );

                    if( bResult )
                    {
                        CPLJSONObject oParentRoot = oResourceReq.GetRoot();
                        if( osResourceType == "mapserver_style" ||
                            osResourceType == "qgis_vector_style" )
                        {
                            nEPSG = oParentRoot.GetInteger("vector_layer/srs/id", nEPSG);
                        }
                        else if( osResourceType == "raster_style" ||
                                 osResourceType == "qgis_raster_style")
                        {
                            nEPSG = oParentRoot.GetInteger("raster_layer/srs/id", nEPSG);
                        }
                    }
                }

                // Create raster dataset.
                std::string osRasterUrl = NGWAPI::GetTMS(osUrl, osResourceId);
                char* pszRasterUrl = CPLEscapeString(osRasterUrl.c_str(), -1, CPLES_XML);
                const char *pszConnStr = CPLSPrintf("<GDAL_WMS><Service name=\"TMS\">"
            "<ServerUrl>%s</ServerUrl></Service><DataWindow>"
            "<UpperLeftX>-20037508.34</UpperLeftX><UpperLeftY>20037508.34</UpperLeftY>"
            "<LowerRightX>20037508.34</LowerRightX><LowerRightY>-20037508.34</LowerRightY>"
            "<TileLevel>%d</TileLevel><TileCountX>1</TileCountX>"
            "<TileCountY>1</TileCountY><YOrigin>top</YOrigin></DataWindow>"
            "<Projection>EPSG:%d</Projection><BlockSizeX>256</BlockSizeX>"
            "<BlockSizeY>256</BlockSizeY><BandsCount>%d</BandsCount>"
            "<Cache><Type>file</Type><Expires>%d</Expires><MaxSize>%d</MaxSize>"
            "</Cache><ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes></GDAL_WMS>",
                pszRasterUrl,
                22,      // NOTE: We have no limit in zoom levels.
                nEPSG,   // NOTE: Default SRS is EPSG:3857.
                4,
                nCacheExpires,
                nCacheMaxSize);

                CPLFree( pszRasterUrl );

                poRasterDS = reinterpret_cast<GDALDataset*>(GDALOpenEx(pszConnStr,
                    GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_INTERNAL, nullptr,
                    nullptr, nullptr));

                if( poRasterDS )
                {
                    bResult = true;
                    nRasterXSize = poRasterDS->GetRasterXSize();
                    nRasterYSize = poRasterDS->GetRasterYSize();

                    for( int iBand = 1; iBand <= poRasterDS->GetRasterCount();
                            iBand++ )
                    {
                        SetBand( iBand, new NGWWrapperRasterBand(
                            poRasterDS->GetRasterBand( iBand )) );
                    }

                    // Set pixel limits.
                    bool bHasTransform = false;
                    double geoTransform[6] = { 0.0 };
                    double invGeoTransform[6] = { 0.0 };
                    if(poRasterDS->GetGeoTransform(geoTransform) == CE_None)
                    {
                        bHasTransform = GDALInvGeoTransform(geoTransform,
                            invGeoTransform) == TRUE;
                    }

                    if(bHasTransform)
                    {
                        GDALApplyGeoTransform(invGeoTransform, stExtent.MinX,
                            stExtent.MinY, &stPixelExtent.MinX, &stPixelExtent.MaxY);

                        GDALApplyGeoTransform(invGeoTransform, stExtent.MaxX,
                            stExtent.MaxY, &stPixelExtent.MaxX, &stPixelExtent.MinY);

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
                else
                {
                    bResult = false;
                }
            }
            else if( osResourceType == "raster_layer" ) //FIXME: Do we need this check? && nOpenFlagsIn & GDAL_OF_RASTER )
            {
                AddRaster( oRoot, papszHTTPOptions );
            }
            else
            {
                bResult = false;
            }
            // TODO: Add support for baselayers, webmap, wfsserver_service, wmsserver_service.
        }
    }

    CSLDestroy( papszHTTPOptions );
    return bResult;
}

/*
 * FillResources()
 */
bool OGRNGWDataset::FillResources( char **papszOptions, int nOpenFlagsIn )
{
    CPLJSONDocument oResourceDetailsReq;
    bool bResult = oResourceDetailsReq.LoadUrl( NGWAPI::GetChildren( osUrl,
        osResourceId ), papszOptions );

    if( bResult )
    {
        CPLJSONArray oChildren(oResourceDetailsReq.GetRoot());
        for( int i = 0; i < oChildren.Size(); ++i )
        {
            CPLJSONObject oChild = oChildren[i];
            std::string osResourceType = oChild.GetString("resource/cls");
            if( (osResourceType == "vector_layer" ||
                osResourceType == "postgis_layer") )
            {
                // Add vector layer. If failed, try next layer.
                AddLayer( oChild, papszOptions, nOpenFlagsIn );
            }
            else if( (osResourceType == "raster_layer" ||
                osResourceType == "wmsclient_layer") && nOpenFlagsIn & GDAL_OF_RASTER )
            {
                AddRaster( oChild, papszOptions );
            }
            // TODO: Add support for baselayers, webmap, wfsserver_service, wmsserver_service.
        }
    }
    return bResult;
}

/*
 * AddLayer()
 */
void OGRNGWDataset::AddLayer( const CPLJSONObject &oResourceJsonObject,
    char **papszOptions, int nOpenFlagsIn )
{
    std::string osLayerResourceId;
    if( nOpenFlagsIn & GDAL_OF_VECTOR )
    {
        OGRNGWLayer *poLayer = new OGRNGWLayer( this, oResourceJsonObject );
        papoLayers = (OGRNGWLayer**) CPLRealloc(papoLayers, (nLayers + 1) *
            sizeof(OGRNGWLayer*));
        papoLayers[nLayers++] = poLayer;
        osLayerResourceId = poLayer->GetResourceId();
    }
    else
    {
        osLayerResourceId = oResourceJsonObject.GetString("resource/id");
    }

    // Check styles exist and add them as rasters.
    if( nOpenFlagsIn & GDAL_OF_RASTER &&
        oResourceJsonObject.GetBool( "resource/children", false ) )
    {
        CPLJSONDocument oResourceChildReq;
        bool bResult = oResourceChildReq.LoadUrl( NGWAPI::GetChildren( osUrl,
            osLayerResourceId ), papszOptions );

        if( bResult )
        {
            CPLJSONArray oChildren( oResourceChildReq.GetRoot() );
            for( int i = 0; i < oChildren.Size(); ++i )
            {
                AddRaster( oChildren[i], papszOptions );
            }
        }
    }
}

/*
 * AddRaster()
 */
void OGRNGWDataset::AddRaster( const CPLJSONObject &oRasterJsonObj,
    char **papszOptions )
{
    std::string osOutResourceId;
    std::string osOutResourceName;
    std::string osResourceType = oRasterJsonObj.GetString( "resource/cls" );
    if( osResourceType == "mapserver_style" ||
        osResourceType == "qgis_vector_style" ||
        osResourceType == "raster_style" ||
        osResourceType == "qgis_raster_style" ||
        osResourceType == "wmsclient_layer" )
    {
        osOutResourceId = oRasterJsonObj.GetString( "resource/id" );
        osOutResourceName = oRasterJsonObj.GetString( "resource/display_name" );
    }
    else if( osResourceType == "raster_layer" )
    {
        std::string osRasterResourceId = oRasterJsonObj.GetString( "resource/id" );
        CPLJSONDocument oResourceRequest;
        bool bResult = oResourceRequest.LoadUrl( NGWAPI::GetChildren( osUrl,
            osRasterResourceId ), papszOptions );

        if( bResult )
        {
            CPLJSONArray oChildren(oResourceRequest.GetRoot());
            for( int i = 0; i < oChildren.Size(); ++i )
            {
                CPLJSONObject oChild = oChildren[i];
                osResourceType = oChild.GetString("resource/cls");
                if( osResourceType == "raster_style" ||
                    osResourceType == "qgis_raster_style" )
                {
                    AddRaster( oChild, papszOptions );
                }
            }
        }
    }

    if( !osOutResourceId.empty() )
    {
        if( osOutResourceName.empty() )
        {
            osOutResourceName = "raster_" + osOutResourceId;
        }

        CPLDebug("NGW", "Add raster %s: %s", osOutResourceId.c_str(),
            osOutResourceName.c_str());

        GDALDataset::SetMetadataItem( CPLSPrintf("SUBDATASET_%d_NAME", nRasters),
            CPLSPrintf("NGW:%s/resource/%s", osUrl.c_str(),
            osOutResourceId.c_str()), "SUBDATASETS" );
        GDALDataset::SetMetadataItem( CPLSPrintf("SUBDATASET_%d_DESC", nRasters),
            osOutResourceName.c_str(), "SUBDATASETS" );
        nRasters++;
    }
}

/*
 * ICreateLayer
 */
OGRLayer *OGRNGWDataset::ICreateLayer( const char *pszNameIn,
                                           OGRSpatialReference *poSpatialRef,
                                           OGRwkbGeometryType eGType,
                                           char **papszOptions )
{
    if( !IsUpdateMode() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Operation not available in read-only mode");
        return nullptr;
    }

    // Check permissions as we create new layer in memory and will create in during SyncToDisk.
    FetchPermissions();

    if( !stPermissions.bResourceCanCreate )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
        return nullptr;
    }

    // Check input parameters.
    if( (eGType < wkbPoint || eGType > wkbMultiPolygon) &&
        (eGType < wkbPoint25D || eGType > wkbMultiPolygon25D) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Unsupported geometry type: %s", OGRGeometryTypeToName(eGType));
        return nullptr;
    }

    if( !poSpatialRef )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Undefined spatial reference");
        return nullptr;
    }

    poSpatialRef->AutoIdentifyEPSG();
    const char *pszEPSG = poSpatialRef->GetAuthorityCode( nullptr );
    int nEPSG = -1;
    if( pszEPSG != nullptr )
    {
        nEPSG = atoi( pszEPSG );
    }

    if( nEPSG != 3857 ) // TODO: Check NextGIS Web supported SRS.
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Unsupported spatial reference EPSG code: %d", nEPSG);
        return nullptr;
    }

    // Do we already have this layer?  If so, should we blow it away?
    bool bOverwrite = CPLFetchBool(papszOptions, "OVERWRITE", false);
    for( int iLayer = 0; iLayer < nLayers; ++iLayer )
    {
        if( EQUAL(pszNameIn, papoLayers[iLayer]->GetName()) )
        {
            if( bOverwrite )
            {
                DeleteLayer( iLayer );
                break;
            }
            else
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Layer %s already exists, CreateLayer failed.\n"
                          "Use the layer creation option OVERWRITE=YES to "
                          "replace it.",
                          pszNameIn );
                return nullptr;
            }
        }
    }

    // Create layer.
    std::string osKey = CSLFetchNameValueDef( papszOptions, "KEY", "");
    std::string osDesc = CSLFetchNameValueDef( papszOptions, "DESCRIPTION", "");
    OGRSpatialReference* poSRSClone = poSpatialRef->Clone();
    poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    OGRNGWLayer *poLayer = new OGRNGWLayer( this, pszNameIn, poSRSClone, eGType,
        osKey, osDesc );
    poSRSClone->Release();
    papoLayers = (OGRNGWLayer**) CPLRealloc(papoLayers, (nLayers + 1) *
        sizeof(OGRNGWLayer*));
    papoLayers[nLayers++] = poLayer;
    return poLayer;
}

/*
 * DeleteLayer()
 */
OGRErr OGRNGWDataset::DeleteLayer( int iLayer )
{
    if( !IsUpdateMode() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Operation not available in read-only mode.");
        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Layer %d not in legal range of 0 to %d.", iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    OGRNGWLayer *poLayer = static_cast<OGRNGWLayer*>(papoLayers[iLayer]);

    if( poLayer->GetResourceId() != "-1" )
    {
        // For layers from server we can check permissions.

        // We can skip check permissions here as papoLayers[iLayer]->Delete() will
        // return false if no delete permission available.
        FetchPermissions();

        if( !stPermissions.bResourceCanDelete )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
            return OGRERR_FAILURE;
        }
    }

    if( poLayer->Delete() )
    {
        delete poLayer;
        memmove( papoLayers + iLayer, papoLayers + iLayer + 1,
                 sizeof(void *) * (nLayers - iLayer - 1) );
        nLayers--;
    }

    return OGRERR_NONE;
}

/*
 * FillMetadata()
 */
void OGRNGWDataset::FillMetadata( const CPLJSONObject &oRootObject )
{
    std::string osCreateDate = oRootObject.GetString("resource/creation_date");
    if( !osCreateDate.empty() )
    {
        GDALDataset::SetMetadataItem( "creation_date", osCreateDate.c_str() );
    }
    osName = oRootObject.GetString("resource/display_name");
    SetDescription( osName.c_str() );
    GDALDataset::SetMetadataItem( "display_name", osName.c_str() );
    std::string osDescription = oRootObject.GetString("resource/description");
    if( !osDescription.empty() )
    {
        GDALDataset::SetMetadataItem( "description", osDescription.c_str() );
    }
    std::string osResourceType = oRootObject.GetString("resource/cls");
    if( !osResourceType.empty() )
    {
        GDALDataset::SetMetadataItem( "resource_type", osResourceType.c_str() );
    }
    std::string osResourceParentId = oRootObject.GetString("resource/parent/id");
    if( !osResourceParentId.empty() )
    {
        GDALDataset::SetMetadataItem( "parent_id", osResourceParentId.c_str() );
    }
    GDALDataset::SetMetadataItem( "id", osResourceId.c_str() );

    std::vector<CPLJSONObject> items =
        oRootObject.GetObj("resmeta/items").GetChildren();

    for( const CPLJSONObject &item : items )
    {
        std::string osSuffix = NGWAPI::GetResmetaSuffix( item.GetType() );
        GDALDataset::SetMetadataItem( (item.GetName() + osSuffix).c_str(),
            item.ToString().c_str(), "NGW" );
    }
}

/*
 * FlushMetadata()
 */
bool OGRNGWDataset::FlushMetadata( char **papszMetadata )
{
    if( !bMetadataDerty )
    {
        return true;
    }

    bool bResult = NGWAPI::FlushMetadata(osUrl, osResourceId, papszMetadata,
        GetHeaders());
    if( bResult )
    {
        bMetadataDerty = false;
    }

    return bResult;
}

/*
 * SetMetadata()
 */
CPLErr OGRNGWDataset::SetMetadata( char **papszMetadata, const char *pszDomain)
{
    FetchPermissions();
    if( !stPermissions.bMetadataCanWrite )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
        return CE_Failure;
    }

    CPLErr eResult = GDALDataset::SetMetadata(papszMetadata, pszDomain);
    if( eResult == CE_None && pszDomain != nullptr && EQUAL(pszDomain, "NGW") )
    {
        eResult = FlushMetadata( papszMetadata ) ? CE_None : CE_Failure;
    }
    return eResult;
}

/*
 * SetMetadataItem()
 */
CPLErr OGRNGWDataset::SetMetadataItem( const char *pszName,
    const char *pszValue, const char *pszDomain)
{
    FetchPermissions();
    if( !stPermissions.bMetadataCanWrite )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
        return CE_Failure;
    }
    if( pszDomain != nullptr && EQUAL(pszDomain, "NGW") )
    {
        bMetadataDerty = true;
    }
    return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/*
 * FlushCache()
 */
void OGRNGWDataset::FlushCache(bool bAtClosing)
{
    GDALDataset::FlushCache(bAtClosing);
    FlushMetadata( GetMetadata("NGW") );
}

/*
 * GetHeaders()
 */
char **OGRNGWDataset::GetHeaders() const
{
    char **papszOptions = nullptr;
    papszOptions = CSLAddString(papszOptions, "HEADERS=Accept: */*");
    papszOptions = CSLAddNameValue(papszOptions, "JSON_DEPTH", osJsonDepth.c_str());
    if( !osUserPwd.empty() )
    {
        papszOptions = CSLAddString(papszOptions, "HTTPAUTH=BASIC");
        std::string osUserPwdOption("USERPWD=");
        osUserPwdOption += osUserPwd;
        papszOptions = CSLAddString(papszOptions, osUserPwdOption.c_str());
    }
    return papszOptions;
}

/*
 * SQLUnescape()
 * Get from gdal/ogr/ogrsf_frmts/sqlite/ogrsqliteutility.cpp as we don't want
 * dependency on sqlite
 */
static CPLString SQLUnescape( const char *pszVal )
{
    char chQuoteChar = pszVal[0];
    if( chQuoteChar != '\'' && chQuoteChar != '"' )
        return pszVal;

    CPLString osRet;
    pszVal ++;
    while( *pszVal != '\0' )
    {
        if( *pszVal == chQuoteChar )
        {
            if( pszVal[1] == chQuoteChar )
                pszVal ++;
            else
                break;
        }
        osRet += *pszVal;
        pszVal ++;
    }
    return osRet;
}

/*
 * SQLTokenize()
 * Get from gdal/ogr/ogrsf_frmts/sqlite/ogrsqliteutility.cpp as we don't want
 * dependency on sqlite
 */
static char **SQLTokenize( const char *pszStr )
{
    char** papszTokens = nullptr;
    bool bInQuote = false;
    char chQuoteChar = '\0';
    bool bInSpace = true;
    CPLString osCurrentToken;
    while( *pszStr != '\0' )
    {
        if( *pszStr == ' ' && !bInQuote )
        {
            if( !bInSpace )
            {
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
            }
            bInSpace = true;
        }
        else if( (*pszStr == '(' || *pszStr == ')' || *pszStr == ',')  && !bInQuote )
        {
            if( !bInSpace )
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
        else if( *pszStr == '"' || *pszStr == '\'' )
        {
            if( bInQuote && *pszStr == chQuoteChar && pszStr[1] == chQuoteChar )
            {
                osCurrentToken += *pszStr;
                osCurrentToken += *pszStr;
                pszStr += 2;
                continue;
            }
            else if( bInQuote && *pszStr == chQuoteChar )
            {
                osCurrentToken += *pszStr;
                papszTokens = CSLAddString(papszTokens, osCurrentToken);
                osCurrentToken.clear();
                bInSpace = true;
                bInQuote = false;
                chQuoteChar = '\0';
            }
            else if( bInQuote )
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
        pszStr ++;
    }

    if( !osCurrentToken.empty() )
        papszTokens = CSLAddString(papszTokens, osCurrentToken);

    return papszTokens;
}


/*
 * ExecuteSQL()
 */
OGRLayer *OGRNGWDataset::ExecuteSQL( const char *pszStatement,
    OGRGeometry *poSpatialFilter, const char *pszDialect )
{
    // Clean statement string.
    CPLString osStatement(pszStatement);
    osStatement = osStatement.Trim().replaceAll("  ", " ");

    if( STARTS_WITH_CI(osStatement, "DELLAYER:") )
    {
        CPLString osLayerName = osStatement.substr(strlen("DELLAYER:"));
        if( osLayerName.endsWith(";") )
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 1);
            osLayerName.Trim();
        }

        CPLDebug("NGW", "Delete layer with name %s.", osLayerName.c_str());

        for( int iLayer = 0; iLayer < nLayers; ++iLayer )
        {
            if( EQUAL(papoLayers[iLayer]->GetName(), osLayerName ) )
            {
                DeleteLayer( iLayer );
                return nullptr;
            }
        }
        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer : %s",
            osLayerName.c_str());

        return nullptr;
    }

    if( STARTS_WITH_CI(osStatement, "DELETE FROM") )
    {
        // Get layer name from pszStatement DELETE FROM layer;.
        CPLString osLayerName = osStatement.substr(strlen("DELETE FROM "));
        if( osLayerName.endsWith(";") )
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 1);
            osLayerName.Trim();
        }

        CPLDebug("NGW", "Delete features from layer with name %s.", osLayerName.c_str());

        OGRNGWLayer *poLayer = static_cast<OGRNGWLayer*>(GetLayerByName(osLayerName));
        if( poLayer )
        {
            poLayer->DeleteAllFeatures();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer : %s",
                osLayerName.c_str());
        }
        return nullptr;
    }

    if( STARTS_WITH_CI(osStatement, "DROP TABLE") )
    {
        // Get layer name from pszStatement DELETE FROM layer;.
        CPLString osLayerName = osStatement.substr(strlen("DROP TABLE "));
        if( osLayerName.endsWith(";") )
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 1);
            osLayerName.Trim();
        }

        CPLDebug("NGW", "Delete layer with name %s.", osLayerName.c_str());

        for( int iLayer = 0; iLayer < nLayers; ++iLayer )
        {
            if( EQUAL(papoLayers[iLayer]->GetName(), osLayerName ) )
            {
                DeleteLayer( iLayer );
                return nullptr;
            }
        }

        CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer : %s",
            osLayerName.c_str());

        return nullptr;
    }

    if( STARTS_WITH_CI(osStatement, "ALTER TABLE ") )
    {
        if( osStatement.endsWith(";") )
        {
            osStatement = osStatement.substr(0, osStatement.size() - 1);
            osStatement.Trim();
        }

        CPLStringList aosTokens( SQLTokenize(osStatement) );
        /* ALTER TABLE src_table RENAME TO dst_table */
        if( aosTokens.size() == 6 && EQUAL(aosTokens[3], "RENAME") &&
            EQUAL(aosTokens[4], "TO") )
        {
            const char* pszSrcTableName = aosTokens[2];
            const char* pszDstTableName = aosTokens[5];

            OGRNGWLayer *poLayer = static_cast<OGRNGWLayer*>(GetLayerByName(
                SQLUnescape(pszSrcTableName) ));
            if( poLayer )
            {
                poLayer->Rename( SQLUnescape(pszDstTableName) );
                return nullptr;
            }

            CPLError(CE_Failure, CPLE_AppDefined, "Unknown layer : %s",
                pszSrcTableName);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Unsupported alter table operation. Only rename table to ... support.");
        }
        return nullptr;
    }

    // SELECT xxxxx FROM yyyy WHERE zzzzzz;
    if( STARTS_WITH_CI(osStatement, "SELECT ") )
    {
        swq_select oSelect;
        CPLDebug("NGW", "Select statement: %s", osStatement.c_str());
        if( oSelect.preparse( osStatement ) != CE_None )
        {
            return nullptr;
        }

        if( oSelect.join_count == 0 && oSelect.poOtherSelect == nullptr &&
            oSelect.table_count == 1 && oSelect.order_specs == 0 )
        {
            OGRNGWLayer *poLayer = reinterpret_cast<OGRNGWLayer*>(
                GetLayerByName( oSelect.table_defs[0].table_name ) );
            if( nullptr == poLayer )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Layer %s not found in dataset.", oSelect.table_defs[0].table_name);
                return nullptr;
            }

            std::set<std::string> aosFields;
            bool bSkip = false;
            for( int i = 0; i < oSelect.result_columns; ++i )
            {
                swq_col_func col_func = oSelect.column_defs[i].col_func;
                if( col_func != SWQCF_NONE )
                {
                    bSkip = true;
                    break;
                }

                if( oSelect.column_defs[i].distinct_flag )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                        "Distinct not supported.");
                    bSkip = true;
                    break;
                }

                if( oSelect.column_defs[i].field_name != nullptr )
                {
                    if( EQUAL(oSelect.column_defs[i].field_name, "*") )
                    {
                        aosFields.clear();
                        aosFields.emplace( oSelect.column_defs[i].field_name );
                        break;
                    }
                    else
                    {
                        aosFields.emplace( oSelect.column_defs[i].field_name );
                    }
                }
            }

            std::string osNgwSelect;
            for( int iKey = 0; iKey < oSelect.order_specs; iKey++ )
            {
                swq_order_def *psKeyDef = oSelect.order_defs + iKey;
                if(iKey > 0 )
                {
                    osNgwSelect += ",";
                }

                if( psKeyDef->ascending_flag == TRUE )
                {
                    osNgwSelect += psKeyDef->field_name;
                }
                else
                {
                    osNgwSelect += "-" + std::string(psKeyDef->field_name);
                }
            }

            if( oSelect.where_expr != nullptr )
            {
                if( !osNgwSelect.empty() )
                {
                    osNgwSelect += "&";
                }
                osNgwSelect += OGRNGWLayer::TranslateSQLToFilter(
                    oSelect.where_expr );

            }

            if( osNgwSelect.empty() )
            {
                bSkip = true;
            }

            if( !bSkip )
            {
                if( aosFields.empty() )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "SELECT statement is invalid: field list is empty.");
                    return nullptr;
                }

                if( poLayer->SyncToDisk() != OGRERR_NONE )
                {
                    return nullptr;
                }

                OGRNGWLayer *poOutLayer = poLayer->Clone();
                if( aosFields.size() == 1 && *(aosFields.begin()) == "*" )
                {
                    poOutLayer->SetIgnoredFields(nullptr);
                }
                else
                {
                    poOutLayer->SetSelectedFields(aosFields);
                }
                poOutLayer->SetSpatialFilter(poSpatialFilter);

                if( osNgwSelect.empty() ) // If we here oSelect.where_expr is empty
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
    if( poRasterDS != nullptr )
    {
        return poRasterDS->GetSpatialRef();
    }
    return GDALDataset::GetSpatialRef();
}

/*
 * GetGeoTransform()
 */
CPLErr OGRNGWDataset::GetGeoTransform( double *padfTransform )
{
    if( poRasterDS != nullptr )
    {
        return poRasterDS->GetGeoTransform( padfTransform );
    }
    return GDALDataset::GetGeoTransform( padfTransform );
}

/*
 * IRasterIO()
 */
CPLErr OGRNGWDataset::IRasterIO( GDALRWFlag eRWFlag, int nXOff, int nYOff,
    int nXSize, int nYSize, void *pData, int nBufXSize, int nBufYSize,
    GDALDataType eBufType, int nBandCount, int *panBandMap,
    GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace,
    GDALRasterIOExtraArg* psExtraArg )
{
    if( poRasterDS != nullptr )
    {
        if( stPixelExtent.IsInit() )
        {
            OGREnvelope stTestExtent;
            stTestExtent.MinX = static_cast<double>(nXOff);
            stTestExtent.MinY = static_cast<double>(nYOff);
            stTestExtent.MaxX = static_cast<double>(nXOff + nXSize);
            stTestExtent.MaxY = static_cast<double>(nYOff + nYSize);

            if( !stPixelExtent.Intersects(stTestExtent) )
            {
                CPLDebug("NGW", "Raster extent in px is: %f, %f, %f, %f",
                    stPixelExtent.MinX, stPixelExtent.MinY,
                    stPixelExtent.MaxX, stPixelExtent.MaxY);
                CPLDebug("NGW", "RasterIO extent is: %f, %f, %f, %f",
                    stTestExtent.MinX, stTestExtent.MinY,
                    stTestExtent.MaxX, stTestExtent.MaxY);

                // Fill buffer transparent color.
                memset( pData, 0, nBufXSize * nBufYSize * nBandCount *
                    GDALGetDataTypeSizeBytes(eBufType) );
                return CE_None;
            }
        }
    }
    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
        nBufXSize, nBufYSize, eBufType, nBandCount, panBandMap, nPixelSpace,
        nLineSpace, nBandSpace, psExtraArg);
}

/*
 * FillCapabilities()
 */
void OGRNGWDataset::FillCapabilities( char **papszOptions )
{
    // Check NGW version. Paging available from 3.1
    CPLJSONDocument oRouteReq;
    if( oRouteReq.LoadUrl( NGWAPI::GetVersion(osUrl), papszOptions ) )
    {
        CPLJSONObject oRoot = oRouteReq.GetRoot();

        if( oRoot.IsValid() )
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
