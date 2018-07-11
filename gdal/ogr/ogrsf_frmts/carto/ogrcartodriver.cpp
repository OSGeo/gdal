/******************************************************************************
 *
 * Project:  Carto Translator
 * Purpose:  Implements OGRCARTODriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_carto.h"

// g++ -g -Wall -fPIC -shared -o ogr_CARTO.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/carto ogr/ogrsf_frmts/carto/*.c* -L. -lgdal -Iogr/ogrsf_frmts/geojson/libjson

CPL_CVSID("$Id$")

extern "C" void RegisterOGRCarto();

/************************************************************************/
/*                        OGRCartoDriverIdentify()                    */
/************************************************************************/

static int OGRCartoDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, "CARTO:") ||
           STARTS_WITH_CI(poOpenInfo->pszFilename, "CARTODB:");
}

/************************************************************************/
/*                           OGRCartoDriverOpen()                     */
/************************************************************************/

static GDALDataset *OGRCartoDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRCartoDriverIdentify(poOpenInfo) )
        return nullptr;

    OGRCARTODataSource   *poDS = new OGRCARTODataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                     poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                      OGRCartoDriverCreate()                        */
/************************************************************************/

static GDALDataset *OGRCartoDriverCreate( const char * pszName,
                                            CPL_UNUSED int nBands,
                                            CPL_UNUSED int nXSize,
                                            CPL_UNUSED int nYSize,
                                            CPL_UNUSED GDALDataType eDT,
                                            CPL_UNUSED char **papszOptions )

{
    OGRCARTODataSource   *poDS = new OGRCARTODataSource();

    if( !poDS->Open( pszName, nullptr, TRUE ) )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Carto driver doesn't support database creation." );
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         RegisterOGRCARTO()                         */
/************************************************************************/

void RegisterOGRCarto()

{
    if( GDALGetDriverByName( "Carto" ) != nullptr )
      return;

    GDALDriver* poDriver = new GDALDriver();

    poDriver->SetDescription( "Carto" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                  "Carto" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                    "drv_carto.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "CARTO:" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
    "<OpenOptionList>"
    "  <Option name='API_KEY' type='string' description='Account API key'/>"
    "  <Option name='ACCOUNT' type='string' description='Account name' required='true'/>"
    "  <Option name='BATCH_INSERT' type='boolean' description='Whether to group features to be inserted in a batch' default='YES'/>"
    "  <Option name='COPY_MODE' type='boolean' description='Whether to use the COPY API for faster uploads' default='YES'/>"
    "</OpenOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
    "<LayerCreationOptionList>"
    "  <Option name='OVERWRITE' type='boolean' description='Whether to overwrite an existing table with the layer name to be created' default='NO'/>"
    "  <Option name='LAUNDER' type='boolean' description='Whether layer and field names will be laundered' default='YES'/>"
    "  <Option name='GEOMETRY_NULLABLE' type='boolean' description='Whether the values of the geometry column can be NULL' default='YES'/>"
    "  <Option name='CARTODBFY' alias='CARTODBIFY' type='boolean' description='Whether the created layer should be \"Cartodbifi&apos;ed\" (i.e. registered in dashboard)' default='YES'/>"
    "</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Integer64 Real String Date DateTime Time" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );

    poDriver->pfnOpen = OGRCartoDriverOpen;
    poDriver->pfnIdentify = OGRCartoDriverIdentify;
    poDriver->pfnCreate = OGRCartoDriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
