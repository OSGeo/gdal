/******************************************************************************
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM generic driver.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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

#include "gnm_frmts.h"
#include "gnm_priv.h"
#include "gnmdb.h"

CPL_CVSID("$Id$")

static int GNMDBDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "PGB:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "PG:") )
        return FALSE;
    if( (poOpenInfo->nOpenFlags & GDAL_OF_GNM) == 0 )
        return FALSE;

    // TODO: do we need to open datasource end check tables/layer exist?

    return TRUE;
}

static GDALDataset *GNMDBDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !GNMDBDriverIdentify(poOpenInfo) )
        return nullptr;

    GNMDatabaseNetwork* poFN = new GNMDatabaseNetwork();

    if( poFN->Open( poOpenInfo ) != CE_None)
    {
        delete poFN;
        poFN = nullptr;
    }

    return poFN;
}

static GDALDataset *GNMDBDriverCreate( const char * pszName,
                                        CPL_UNUSED int nBands,
                                        CPL_UNUSED int nXSize,
                                        CPL_UNUSED int nYSize,
                                        CPL_UNUSED GDALDataType eDT,
                                        char **papszOptions )
{
    CPLAssert( nullptr != pszName );
    CPLDebug( "GNM", "Attempt to create network at: %s", pszName );

    GNMDatabaseNetwork *poFN = new GNMDatabaseNetwork();

    if( poFN->Create( pszName, papszOptions ) != CE_None )
    {
        delete poFN;
        poFN = nullptr;
    }

    return poFN;
}

static CPLErr GNMDBDriverDelete( const char *pszDataSource )

{
    GDALOpenInfo oOpenInfo(pszDataSource, GA_Update);
    GNMDatabaseNetwork* poFN = new GNMDatabaseNetwork();

    if( poFN->Open( &oOpenInfo ) != CE_None)
    {
        delete poFN;
        poFN = nullptr;

        return CE_Failure;
    }

    return poFN->Delete();
}

void RegisterGNMDatabase()
{
    if( GDALGetDriverByName( "GNMDatabase" ) == nullptr )
    {
        GDALDriver  *poDriver = new GDALDriver();

        poDriver->SetDescription( "GNMDatabase" );
        poDriver->SetMetadataItem( GDAL_DCAP_GNM, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Geographic Network generic DB based "
                                   "model" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, CPLSPrintf(
"<CreationOptionList>"
"  <Option name='%s' type='string' description='The network name. Also it will be a folder name, so the limits for folder name distribute on network name'/>"
"  <Option name='%s' type='string' description='The network description. Any text describes the network'/>"
"  <Option name='%s' type='string' description='The network Spatial reference. All network features will reproject to this spatial reference. May be a WKT text or EPSG code'/>"
"  <Option name='FORMAT' type='string' description='The OGR format to store network data.'/>"
"  <Option name='OVERWRITE' type='boolean' description='Overwrite exist network or not' default='NO'/>"
"</CreationOptionList>", GNM_MD_NAME, GNM_MD_DESCR, GNM_MD_SRS) );

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
                                   "<LayerCreationOptionList/>" );
        poDriver->pfnOpen = GNMDBDriverOpen;
        poDriver->pfnIdentify = GNMDBDriverIdentify;
        poDriver->pfnCreate = GNMDBDriverCreate;
        poDriver->pfnDelete = GNMDBDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
