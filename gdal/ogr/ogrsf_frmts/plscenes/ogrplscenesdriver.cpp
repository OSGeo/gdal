/******************************************************************************
 *
 * Project:  PlanetLabs scene driver
 * Purpose:  PlanetLabs scene driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2015-2016, Planet Labs
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

#include "ogr_plscenes.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRPLScenesIdentify()                        */
/************************************************************************/

static int OGRPLScenesIdentify(GDALOpenInfo* poOpenInfo)
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "PLSCENES:");
}

/************************************************************************/
/*                            OGRPLScenesOpen()                         */
/************************************************************************/

static GDALDataset* OGRPLScenesOpen(GDALOpenInfo* poOpenInfo)
{
    if( !OGRPLScenesIdentify(poOpenInfo) || poOpenInfo->eAccess == GA_Update )
        return NULL;

    char** papszOptions = CSLTokenizeStringComplex(
            poOpenInfo->pszFilename+strlen("PLScenes:"), ",", TRUE, FALSE );
    CPLString osVersion = CSLFetchNameValueDef(papszOptions, "version",
                CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "VERSION", ""));
    CSLDestroy(papszOptions);

    if( EQUAL(osVersion, "v0") || EQUAL(osVersion, "v1") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This API version has been removed or deprecated. "
                 "Please use DATA_V1 API instead");
        return NULL;
    }
    else if( EQUAL(osVersion, "data_v1") || EQUAL(osVersion, "")  )
    {
        return OGRPLScenesDataV1Dataset::Open(poOpenInfo);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Unhandled API version: %s", osVersion.c_str());
        return NULL;
    }
}

/************************************************************************/
/*                        RegisterOGRPLSCENES()                         */
/************************************************************************/

void RegisterOGRPLSCENES()

{
    if( GDALGetDriverByName( "PLSCENES" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "PLSCENES" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Planet Labs Scenes API" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_plscenes.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "PLSCENES:" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='VERSION' type='string-select' description='API version' default='DATA_V1'>"
"    <Value>DATA_V1</Value>"
"  </Option>"
"  <Option name='API_KEY' type='string' description='Account API key' required='true'/>"
"  <Option name='FOLLOW_LINKS' type='boolean' description='Whether assets links should be followed for each scene' default='NO'/>"
"  <Option name='SCENE' type='string' description='Scene id (for raster fetching)'/>"
"  <Option name='ITEMTYPES' alias='CATALOG' type='string' description='Catalog id (mandatory for raster fetching)'/>"
"  <Option name='ASSET' type='string' description='Asset category' default='visual'/>"
"  <Option name='RANDOM_ACCESS' type='boolean' description='Whether raster should be accessed in random access mode (but with potentially not optimal throughput). If no, in-memory ingestion is done' default='YES'/>"
"  <Option name='ACTIVATION_TIMEOUT' type='int' description='Number of seconds during which to wait for asset activation (raster)' default='3600'/>"
"  <Option name='FILTER' type='string' description='Custom filter'/>"
"  <Option name='METADATA' type='boolean' description='(Raster only) Whether scene metadata should be fetched from the API and attached to the raster dataset' default='YES'/>"
"</OpenOptionList>");

    poDriver->pfnOpen = OGRPLScenesOpen;
    poDriver->pfnIdentify = OGRPLScenesIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
