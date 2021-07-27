/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLDriver class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_mysql.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$")

static CPLMutex* hMutex = nullptr;
static int   bInitialized = FALSE;

/************************************************************************/
/*                        OGRMySQLDriverUnload()                        */
/************************************************************************/

static void OGRMySQLDriverUnload( CPL_UNUSED GDALDriver* poDriver )
{
    if( bInitialized )
    {
        mysql_library_end();
        bInitialized = FALSE;
    }
    if( hMutex != nullptr )
    {
        CPLDestroyMutex(hMutex);
        hMutex = nullptr;
    }
}

/************************************************************************/
/*                         OGRMySQLDriverIdentify()                     */
/************************************************************************/

static int OGRMySQLDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "MYSQL:");
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRMySQLDriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRMySQLDataSource     *poDS;

    if( !OGRMySQLDriverIdentify(poOpenInfo) )
        return nullptr;

    {
        CPLMutexHolderD(&hMutex);
        if( !bInitialized )
        {
            if ( mysql_library_init( 0, nullptr, nullptr ) )
            {
                CPLError( CE_Failure, CPLE_AppDefined, "Could not initialize MySQL library" );
                return nullptr;
            }
            bInitialized = TRUE;
        }
    }

    poDS = new OGRMySQLDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                     poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRMySQLDriverCreate( const char * pszName,
                                          CPL_UNUSED int nBands,
                                          CPL_UNUSED int nXSize,
                                          CPL_UNUSED int nYSize,
                                          CPL_UNUSED GDALDataType eDT,
                                          CPL_UNUSED char **papszOptions )
{
    OGRMySQLDataSource     *poDS;

    poDS = new OGRMySQLDataSource();

    if( !poDS->Open( pszName, nullptr, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
         "MySQL driver doesn't currently support database creation.\n"
                  "Please create database before using." );
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                          RegisterOGRMySQL()                          */
/************************************************************************/

void RegisterOGRMySQL()

{
    if (! GDAL_CHECK_VERSION("MySQL driver"))
        return;

    if( GDALGetDriverByName( "MySQL" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "MySQL" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "MySQL" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/mysql.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "MYSQL:" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='DBNAME' type='string' description='Database name' required='true'/>"
"  <Option name='PORT' type='int' description='Port'/>"
"  <Option name='USER' type='string' description='User name'/>"
"  <Option name='PASSWORD' type='string' description='Password'/>"
"  <Option name='HOST' type='string' description='Server hostname'/>"
"  <Option name='TABLES' type='string' description='Restricted set of tables to list (comma separated)'/>"
"</OpenOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                               "<CreationOptionList/>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
    "<LayerCreationOptionList>"
    "  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
    "  <Option name='LAUNDER' type='boolean' description='Whether layer and field names will be laundered' default='YES'/>"
    "  <Option name='PRECISION' type='boolean' description='Whether fields created should keep the width and precision' default='YES'/>"
    "  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column.' default='SHAPE'/>"
    "  <Option name='SPATIAL_INDEX' type='boolean' description='Whether to create a spatial index' default='YES'/>"
    "  <Option name='FID' type='string' description='Name of the FID column to create' default='OGR_FID' deprecated_alias='MYSQL_FID'/>"
    "  <Option name='FID64' type='boolean' description='Whether to create the FID column with BIGINT type to handle 64bit wide ids' default='NO'/>"
    "  <Option name='ENGINE' type='string' description='Database engine to use.'/>"
    "</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime "
                               "Time Binary" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    poDriver->pfnOpen = OGRMySQLDriverOpen;
    poDriver->pfnIdentify = OGRMySQLDriverIdentify;
    poDriver->pfnCreate = OGRMySQLDriverCreate;
    poDriver->pfnUnloadDriver = OGRMySQLDriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
