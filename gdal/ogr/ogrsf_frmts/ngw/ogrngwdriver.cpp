/*******************************************************************************
 *  Project: NextGIS Web Driver
 *  Purpose: Implements NextGIS Web Driver
 *  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2018, NextGIS
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
    oParent.Add( "id", std::stoi(stUri.osResourceId) );

    std::string osPayload = "POSTFIELDS=" + oPayload.Format(CPLJSONObject::Plain);

    char** papszHTTPOptions = nullptr;
    papszHTTPOptions = CSLAddString( papszHTTPOptions, "CUSTOMREQUEST=POST" );
    papszHTTPOptions = CSLAddString( papszHTTPOptions, osPayload.c_str() );
    papszHTTPOptions = CSLAddString( papszHTTPOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    CPLJSONDocument oRequest;
    bool bResult = oRequest.LoadUrl( NGWAPI::GetResource(stUri.osAddress, ""),
        papszHTTPOptions );
    CSLDestroy(papszHTTPOptions);

    if( !bResult )
    {
        return nullptr;
    }

    CPLJSONObject oRoot = oRequest.GetRoot();
    if( !oRoot.IsValid() )
    {
        return nullptr;
    }

    std::string osNewResourceId = oRoot.GetString( "id" );
    if( osNewResourceId.empty() )
    {
        return nullptr;
    }

    OGRNGWDataset *poDS = new OGRNGWDataset();

    if( !poDS->Open( stUri.osAddress, osNewResourceId, papszOptions, true, GDAL_OF_ALL ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/*
 * GetHeaders()
 */
static char **GetHeaders()
{
    char **papszOptions = nullptr;
    papszOptions = CSLAddString(papszOptions, "HEADERS=Accept: */*");
    std::string osUserPwd = CPLGetConfigOption("NGW_USERPWD", "");
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
 * OGRNGWDriverDelete()
 */
static CPLErr OGRNGWDriverDelete( const char *pszName )
{
    NGWAPI::Uri stUri = NGWAPI::ParseUri(pszName);
    CPLErrorReset();
    if( !stUri.osNewResourceName.empty() )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported url %s", pszName);
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
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_ngw.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "NGW:" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "   <Option name='USERPWD' scope='raster,vector' type='string' description='Username and password, separated by colon'/>"
        "   <Option name='PAGE_SIZE' scope='vector' type='integer' description='Limit feature count while fetching from server. Default value is -1 - no limit' default='-1'/>"
        "   <Option name='BATCH_SIZE' scope='vector' type='integer' description='Size of feature insert and update operations cache before send to server. If batch size is -1 batch mode is disabled' default='-1'/>"
        "   <Option name='CACHE_EXPIRES' scope='raster' type='integer' description='Time in seconds cached files will stay valid. If cached file expires it is deleted when maximum size of cache is reached. Also expired file can be overwritten by the new one from web' default='604800'/>"
        "   <Option name='CACHE_MAX_SIZE' scope='raster' type='integer' description='The cache maximum size in bytes. If cache reached maximum size, expired cached files will be deleted' default='67108864'/>"
        "</OpenOptionList>"
    );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='KEY' scope='raster,vector' type='string' description='Key value. Must be unique in whole NextGIS Web instance'/>"
        "   <Option name='DESCRIPTION' scope='raster,vector' type='string' description='Resource description'/>"
        "   <Option name='USERPWD' type='string' description='Username and password, separated by colon'/>"
        "   <Option name='PAGE_SIZE' type='integer' description='Limit feature count while fetching from server. Default value is -1 - no limit' default='-1'/>"
        "   <Option name='BATCH_SIZE' type='integer' description='Size of feature insert and update operations cache before send to server. If batch size is -1 batch mode is disabled' default='-1'/>"
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

    poDriver->pfnOpen = OGRNGWDriverOpen;
    poDriver->pfnIdentify = OGRNGWDriverIdentify;
    poDriver->pfnCreate = OGRNGWDriverCreate;
    poDriver->pfnDelete = OGRNGWDriverDelete;
    poDriver->pfnRename = OGRNGWDriverRename;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
