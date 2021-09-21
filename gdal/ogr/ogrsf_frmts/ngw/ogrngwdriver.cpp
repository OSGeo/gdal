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

/*
 * GetHeaders()
 */
static char **GetHeaders(const std::string &osUserPwdIn = "")
{
    char **papszOptions = nullptr;
    papszOptions = CSLAddString(papszOptions, "HEADERS=Accept: */*");
    std::string osUserPwd;
    if( osUserPwdIn.empty() )
    {
        osUserPwd = CPLGetConfigOption("NGW_USERPWD", "");
    }
    else
    {
        osUserPwd = osUserPwdIn;
    }

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
 * OGRNGWDriverIdentify()
 */

static int OGRNGWDriverIdentify( GDALOpenInfo *poOpenInfo )
{
    return STARTS_WITH_CI( poOpenInfo->pszFilename, "NGW:" );
}

/*
 * OGRNGWDriverOpen()
 */

static GDALDataset *OGRNGWDriverOpen( GDALOpenInfo *poOpenInfo )
{
    if( OGRNGWDriverIdentify( poOpenInfo ) == 0 )
    {
        return nullptr;
    }

    OGRNGWDataset *poDS = new OGRNGWDataset();
    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                     poOpenInfo->eAccess == GA_Update, poOpenInfo->nOpenFlags ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/*
 * OGRNGWDriverCreate()
 *
 * Add new datasource name at the end of URL:
 * NGW:http://some.nextgis.com/resource/0/new_name
 * NGW:http://some.nextgis.com:8000/test/resource/0/new_name
 */

static GDALDataset *OGRNGWDriverCreate( const char *pszName,
                                            CPL_UNUSED int nBands,
                                            CPL_UNUSED int nXSize,
                                            CPL_UNUSED int nYSize,
                                            CPL_UNUSED GDALDataType eDT,
                                            char **papszOptions )

{
    NGWAPI::Uri stUri = NGWAPI::ParseUri(pszName);
    CPLErrorReset();
    if( stUri.osPrefix != "NGW" )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported name %s", pszName);
        return nullptr;
    }

    CPLDebug("NGW", "Parse uri result. URL: %s, ID: %s, New name: %s",
        stUri.osAddress.c_str(), stUri.osResourceId.c_str(),
        stUri.osNewResourceName.c_str());

    std::string osKey = CSLFetchNameValueDef( papszOptions, "KEY", "");
    std::string osDesc = CSLFetchNameValueDef( papszOptions, "DESCRIPTION", "");
    std::string osUserPwd = CSLFetchNameValueDef( papszOptions, "USERPWD",
        CPLGetConfigOption("NGW_USERPWD", "") );

    CPLJSONObject oPayload;
    CPLJSONObject oResource( "resource", oPayload );
    oResource.Add( "cls", "resource_group" );
    oResource.Add( "display_name", stUri.osNewResourceName );
    if( !osKey.empty() )
    {
        oResource.Add( "keyname", osKey );
    }

    if( !osDesc.empty() )
    {
        oResource.Add( "description", osDesc );
    }

    CPLJSONObject oParent( "parent", oResource );
    oParent.Add( "id", atoi(stUri.osResourceId.c_str()) );

    std::string osNewResourceId = NGWAPI::CreateResource( stUri.osAddress,
        oPayload.Format(CPLJSONObject::PrettyFormat::Plain), GetHeaders(osUserPwd) );
    if( osNewResourceId == "-1" )
    {
        return nullptr;
    }

    OGRNGWDataset *poDS = new OGRNGWDataset();

    if( !poDS->Open( stUri.osAddress, osNewResourceId, papszOptions, true, GDAL_OF_RASTER | GDAL_OF_VECTOR  ) ) // TODO: GDAL_OF_GNM
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/*
 * OGRNGWDriverDelete()
 */
static CPLErr OGRNGWDriverDelete( const char *pszName )
{
    NGWAPI::Uri stUri = NGWAPI::ParseUri(pszName);
    CPLErrorReset();
    if( !stUri.osNewResourceName.empty() )
    {
        CPLError(CE_Warning, CPLE_NotSupported, "Cannot delete new resource with name %s", pszName);
        return CE_Failure;
    }

    if( stUri.osPrefix != "NGW" )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported name %s", pszName);
        return CE_Failure;
    }

    if( stUri.osResourceId == "0" )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Cannot delete resource 0");
        return CE_Failure;
    }

    char **papszOptions = GetHeaders();
    // NGWAPI::Permissions stPermissions = NGWAPI::CheckPermissions(stUri.osAddress,
    //     stUri.osResourceId, papszOptions, true);
    // if( stPermissions.bResourceCanDelete )
    // {
        return NGWAPI::DeleteResource(stUri.osAddress, stUri.osResourceId,
            papszOptions) ? CE_None : CE_Failure;
    // }
    // CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
    // return CE_Failure;
}

/*
 * OGRNGWDriverRename()
 */
static CPLErr OGRNGWDriverRename( const char *pszNewName, const char *pszOldName )
{
    NGWAPI::Uri stUri = NGWAPI::ParseUri(pszOldName);
    CPLErrorReset();
    if( stUri.osPrefix != "NGW" )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported name %s", pszOldName);
        return CE_Failure;
    }
    CPLDebug("NGW", "Parse uri result. URL: %s, ID: %s, New name: %s",
        stUri.osAddress.c_str(), stUri.osResourceId.c_str(), pszNewName);
    char **papszOptions = GetHeaders();
    // NGWAPI::Permissions stPermissions = NGWAPI::CheckPermissions(stUri.osAddress,
    //     stUri.osResourceId, papszOptions, true);
    // if( stPermissions.bResourceCanUpdate )
    // {
        return NGWAPI::RenameResource(stUri.osAddress, stUri.osResourceId,
            pszNewName, papszOptions) ? CE_None : CE_Failure;
    // }
    // CPLError(CE_Failure, CPLE_AppDefined, "Operation not permitted.");
    // return CE_Failure;
}

/*
 * OGRNGWDriverCreateCopy()
 */
static GDALDataset *OGRNGWDriverCreateCopy( const char *pszFilename,
    GDALDataset *poSrcDS, int bStrict, char **papszOptions,
    GDALProgressFunc pfnProgress, void *pProgressData )
{
    // Check destination dataset,
    NGWAPI::Uri stUri = NGWAPI::ParseUri(pszFilename);
    CPLErrorReset();
    if( stUri.osPrefix != "NGW" )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported name %s", pszFilename);
        return nullptr;
    }

    // NGW v3.1 supported different raster types: 1 band and 16/32 bit, RGB/RGBA
    // rasters and etc.
    // For RGB/RGBA rasters we can create default raster_style.
    // For other types - qml style file path is mandatory.
    std::string osQMLPath = CSLFetchNameValueDef( papszOptions, "RASTER_QML_PATH", "");

    // Check bands count.
    const int nBands = poSrcDS->GetRasterCount();
    if( nBands < 3 || nBands > 4 )
    {
        if( osQMLPath.empty() ) {
            CPLError( CE_Failure, CPLE_NotSupported,
                "Default NGW raster style supports only 3 (RGB) or 4 (RGBA). "
                "Raster has %d bands. You must provide QML file with raster style.",
                nBands );
            return nullptr;
        }
    }

    // Check band data type.
    if( poSrcDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte )
    {
        if( osQMLPath.empty() ) {
            CPLError( CE_Failure, CPLE_NotSupported,
                "Default NGW raster style supports only 8 bit byte bands. "
                "Raster has data type %s. You must provide QML file with raster style.",
                GDALGetDataTypeName( poSrcDS->GetRasterBand(1)->GetRasterDataType()) );
            return nullptr;
        }
    }

    bool bCloseDS = false;
    std::string osFilename;

    // Check if source GDALDataset is tiff.
    if( EQUAL(poSrcDS->GetDriverName(), "GTiff") == FALSE )
    {
        GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
        // Compress to minimize network transfer.
        const char* apszOptions[] = { "COMPRESS=LZW", "NUM_THREADS=ALL_CPUS", nullptr };
        std::string osTempFilename = CPLGenerateTempFilename("ngw_tmp");
        osTempFilename += ".tif";
        GDALDataset *poTmpDS = poDriver->CreateCopy( osTempFilename.c_str(),
            poSrcDS, bStrict, const_cast<char**>(apszOptions), pfnProgress,
            pProgressData);

        if( poTmpDS != nullptr )
        {
            bCloseDS = true;
            osFilename = osTempFilename;
            poSrcDS = poTmpDS;
        }
        else
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                "NGW driver doesn't support %s source raster.",
                poSrcDS->GetDriverName() );
            return nullptr;
        }
    }

    if( osFilename.empty() )
    {
        // Check if source tiff is local file.
        CPLStringList oaFiles( poSrcDS->GetFileList() );
        for( int i = 0; i < oaFiles.size(); ++i )
        {
            // Check extension tif
            const char *pszExt = CPLGetExtension(oaFiles[i]);
            if( pszExt && EQUALN(pszExt, "tif", 3) )
            {
                osFilename = oaFiles[i];
                break;
            }
        }
    }

    if( bCloseDS )
    {
        GDALClose( (GDALDatasetH) poSrcDS );
    }

    std::string osKey = CSLFetchNameValueDef( papszOptions, "KEY", "");
    std::string osDesc = CSLFetchNameValueDef( papszOptions, "DESCRIPTION", "");
    std::string osUserPwd = CSLFetchNameValueDef( papszOptions, "USERPWD",
        CPLGetConfigOption("NGW_USERPWD", ""));
    std::string osStyleName = CSLFetchNameValueDef( papszOptions, "RASTER_STYLE_NAME", "");

    // Send file
    char **papszHTTPOptions = GetHeaders(osUserPwd);
    CPLJSONObject oFileJson = NGWAPI::UploadFile(stUri.osAddress, osFilename,
        papszHTTPOptions, pfnProgress, pProgressData);

    if( bCloseDS ) // Delete temp tiff file.
    {
        VSIUnlink(osFilename.c_str());
    }

    if( !oFileJson.IsValid() )
    {
        return nullptr;
    }

    CPLJSONArray oUploadMeta = oFileJson.GetArray("upload_meta");
    if( !oUploadMeta.IsValid() || oUploadMeta.Size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Get unexpected response: %s.",
            oFileJson.Format(CPLJSONObject::PrettyFormat::Plain).c_str());
        return nullptr;
    }

    // Create raster layer
    // Create payload
    CPLJSONObject oPayloadRaster;
    CPLJSONObject oResource( "resource", oPayloadRaster );
    oResource.Add( "cls", "raster_layer" );
    oResource.Add( "display_name", stUri.osNewResourceName );
    if( !osKey.empty() )
    {
        oResource.Add( "keyname", osKey );
    }

    if( !osDesc.empty() )
    {
        oResource.Add( "description", osDesc );
    }

    CPLJSONObject oParent( "parent", oResource );
    oParent.Add( "id", atoi(stUri.osResourceId.c_str()) );

    CPLJSONObject oRasterLayer( "raster_layer", oPayloadRaster );
    oRasterLayer.Add( "source", oUploadMeta[0] );

    CPLJSONObject oSrs("srs", oRasterLayer);
    oSrs.Add( "id", 3857 ); // Now only Web Mercator supported.

    papszHTTPOptions = GetHeaders(osUserPwd);
    std::string osNewResourceId = NGWAPI::CreateResource( stUri.osAddress,
        oPayloadRaster.Format(CPLJSONObject::PrettyFormat::Plain), papszHTTPOptions );
    if( osNewResourceId == "-1" )
    {
        return nullptr;
    }

    // Create raster style
    CPLJSONObject oPayloadRasterStyle;
    CPLJSONObject oResourceStyle( "resource", oPayloadRasterStyle );
    if( osQMLPath.empty() )
    {
        oResourceStyle.Add( "cls", "raster_style" );
    }
    else
    {
        oResourceStyle.Add( "cls", "qgis_raster_style" );

        // Upload QML file
        papszHTTPOptions = GetHeaders(osUserPwd);
        oFileJson = NGWAPI::UploadFile(stUri.osAddress, osQMLPath,
            papszHTTPOptions, pfnProgress, pProgressData);
        oUploadMeta = oFileJson.GetArray("upload_meta");
        if( !oUploadMeta.IsValid() || oUploadMeta.Size() == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Get unexpected response: %s.",
                oFileJson.Format(CPLJSONObject::PrettyFormat::Plain).c_str());
            return nullptr;
        }
        CPLJSONObject oQGISRasterStyle( "qgis_raster_style", oPayloadRasterStyle );
        oQGISRasterStyle.Add( "file_upload", oUploadMeta[0]);
    }

    if( osStyleName.empty() )
    {
        osStyleName = stUri.osNewResourceName;
    }
    oResourceStyle.Add( "display_name", osStyleName );
    CPLJSONObject oParentRaster( "parent", oResourceStyle );
    oParentRaster.Add( "id", atoi(osNewResourceId.c_str()) );

    papszHTTPOptions = GetHeaders(osUserPwd);
    osNewResourceId = NGWAPI::CreateResource( stUri.osAddress,
        oPayloadRasterStyle.Format(CPLJSONObject::PrettyFormat::Plain), papszHTTPOptions );
    if( osNewResourceId == "-1" )
    {
        return nullptr;
    }

    OGRNGWDataset *poDS = new OGRNGWDataset();

    if( !poDS->Open( stUri.osAddress, osNewResourceId, papszOptions, true, GDAL_OF_RASTER ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/*
 * RegisterOGRNGW()
 */

void RegisterOGRNGW()
{
    if( GDALGetDriverByName( "NGW" ) != nullptr )
    {
        return;
    }

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "NGW" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "NextGIS Web" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/ngw.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "NGW:" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, "Byte" );
    poDriver->SetMetadataItem( GDAL_DCAP_CREATECOPY, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='USERPWD' scope='raster,vector' type='string' description='Username and password, separated by colon'/>"
        "   <Option name='PAGE_SIZE' scope='vector' type='integer' description='Limit feature count while fetching from server. Default value is -1 - no limit' default='-1'/>"
        "   <Option name='BATCH_SIZE' scope='vector' type='integer' description='Size of feature insert and update operations cache before send to server. If batch size is -1 batch mode is disabled' default='-1'/>"
        "   <Option name='NATIVE_DATA' scope='vector' type='boolean' description='Whether to store the native Json representation of extensions key. If EXTENSIONS not set or empty, NATIVE_DATA defaults to NO' default='NO'/>"
        "   <Option name='CACHE_EXPIRES' scope='raster' type='integer' description='Time in seconds cached files will stay valid. If cached file expires it is deleted when maximum size of cache is reached. Also expired file can be overwritten by the new one from web' default='604800'/>"
        "   <Option name='CACHE_MAX_SIZE' scope='raster' type='integer' description='The cache maximum size in bytes. If cache reached maximum size, expired cached files will be deleted' default='67108864'/>"
        "   <Option name='JSON_DEPTH' scope='raster,vector' type='integer' description='The depth of json response that can be parsed. If depth is greater than this value, parse error occurs' default='32'/>"
        "   <Option name='EXTENSIONS' scope='vector' type='string' description='Comma separated extensions list. Available are description and attachment' default=''/>"
        "</OpenOptionList>"
    );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='KEY' scope='raster,vector' type='string' description='Key value. Must be unique in whole NextGIS Web instance'/>"
        "   <Option name='DESCRIPTION' scope='raster,vector' type='string' description='Resource description'/>"
        "   <Option name='RASTER_STYLE_NAME' scope='raster' type='string' description='Raster layer style name'/>"
        "   <Option name='USERPWD' scope='raster,vector' type='string' description='Username and password, separated by colon'/>"
        "   <Option name='PAGE_SIZE' scope='vector' type='integer' description='Limit feature count while fetching from server. Default value is -1 - no limit' default='-1'/>"
        "   <Option name='BATCH_SIZE' scope='vector' type='integer' description='Size of feature insert and update operations cache before send to server. If batch size is -1 batch mode is disabled' default='-1'/>"
        "   <Option name='NATIVE_DATA' scope='vector' type='boolean' description='Whether to store the native Json representation of extensions key. If EXTENSIONS not set or empty, NATIVE_DATA defaults to NO' default='NO'/>"
        "   <Option name='CACHE_EXPIRES' scope='raster' type='integer' description='Time in seconds cached files will stay valid. If cached file expires it is deleted when maximum size of cache is reached. Also expired file can be overwritten by the new one from web' default='604800'/>"
        "   <Option name='CACHE_MAX_SIZE' scope='raster' type='integer' description='The cache maximum size in bytes. If cache reached maximum size, expired cached files will be deleted' default='67108864'/>"
        "   <Option name='JSON_DEPTH' scope='raster,vector' type='integer' description='The depth of json response that can be parsed. If depth is greater than this value, parse error occurs' default='32'/>"
        "   <Option name='RASTER_QML_PATH' scope='raster' type='string' description='Raster QMS style path'/>"
        "   <Option name='EXTENSIONS' scope='vector' type='string' description='Comma separated extensions list. Available are description and attachment' default=''/>"
        "</CreationOptionList>"
    );

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "   <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
        "   <Option name='KEY' type='string' description='Key value. Must be unique in whole NextGIS Web instance'/>"
        "   <Option name='DESCRIPTION' type='string' description='Resource description'/>"
        "</LayerCreationOptionList>"
    );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime Time" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "NO" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "NO" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->pfnOpen = OGRNGWDriverOpen;
    poDriver->pfnIdentify = OGRNGWDriverIdentify;
    poDriver->pfnCreate = OGRNGWDriverCreate;
    poDriver->pfnCreateCopy = OGRNGWDriverCreateCopy;
    poDriver->pfnDelete = OGRNGWDriverDelete;
    poDriver->pfnRename = OGRNGWDriverRename;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
