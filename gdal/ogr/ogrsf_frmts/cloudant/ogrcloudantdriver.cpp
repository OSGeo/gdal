/******************************************************************************
 *
 * Project:  CouchDB Translator
 * Purpose:  Implements OGRCloudantDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_cloudant.h"

CPL_CVSID("$Id$")

extern "C" void RegisterOGRCloudant();

/************************************************************************/
/*                   OGRCloudantDriverIdentify()                        */
/************************************************************************/

static int OGRCloudantDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "Cloudant:"))
        return 1;
    else
        return 0;

}

/************************************************************************/
/*                  OGRCloudantDriverOpen()                             */
/************************************************************************/

static GDALDataset* OGRCloudantDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( OGRCloudantDriverIdentify(poOpenInfo) == 0 )
        return NULL;

    OGRCloudantDataSource   *poDS = new OGRCloudantDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/************************************************************************/

static GDALDataset* OGRCloudantDriverCreate( const char * pszName,
                                            int /* nXSize */,
                                            int /* nYSize */,
                                            int /* nBands */,
                                            GDALDataType /* eDT */,
                                            char ** /* papszOptions */ )
{
    OGRCloudantDataSource   *poDS = new OGRCloudantDataSource();

    if( !poDS->Open( pszName, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                         RegisterOGRCloudant()                        */
/************************************************************************/

void RegisterOGRCloudant()

{
    if( GDALGetDriverByName( "Cloudant" ) != NULL )
      return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "Cloudant" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Cloudant / CouchDB" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_cloudant.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "Cloudant:" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                               "<CreationOptionList/>");

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
    "<LayerCreationOptionList>"
    "  <Option name='UPDATE_PERMISSIONS' type='string' description='Update permissions for the new layer.'/>"
    "  <Option name='GEOJSON ' type='boolean' description='Whether to write documents as GeoJSON documents.' default='YES'/>"
    "  <Option name='COORDINATE_PRECISION' type='int' description='Maximum number of figures after decimal separator to write in coordinates.' default='15'/>"
    "</LayerCreationOptionList>");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES,
                               "Integer Integer64 Real String Date DateTime "
                               "Time IntegerList Integer64List RealList "
                               "StringList Binary" );

    poDriver->pfnIdentify = OGRCloudantDriverIdentify;
    poDriver->pfnOpen = OGRCloudantDriverOpen;
    poDriver->pfnCreate = OGRCloudantDriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
