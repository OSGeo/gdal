/******************************************************************************
 *
 * Project:  CouchDB Translator
 * Purpose:  Implements OGRCouchDBDriver.
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

#include "ogr_couchdb.h"

// g++ -g -Wall -fPIC -shared -o ogr_CouchDB.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/couchdb ogr/ogrsf_frmts/couchdb/*.c* -L. -lgdal -Iogr/ogrsf_frmts/geojson/jsonc

CPL_CVSID("$Id$")

extern "C" void RegisterOGRCouchDB();

/************************************************************************/
/*                   OGRCouchDBDriverIdentify()                         */
/************************************************************************/

static int OGRCouchDBDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if (STARTS_WITH(poOpenInfo->pszFilename, "http://") ||
        STARTS_WITH(poOpenInfo->pszFilename, "https://"))
    {
        return -1;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "CouchDB:"))
        return 1;
    else
        return 0;

}

/************************************************************************/
/*                  OGRCouchDBDriverOpen()                              */
/************************************************************************/

static GDALDataset* OGRCouchDBDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( OGRCouchDBDriverIdentify(poOpenInfo) == 0 )
        return NULL;

    OGRCouchDBDataSource   *poDS = new OGRCouchDBDataSource();

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

static GDALDataset* OGRCouchDBDriverCreate( const char * pszName,
                                            int /* nXSize */,
                                            int /* nYSize */,
                                            int /* nBands */,
                                            GDALDataType /* eDT */,
                                            char ** /* papszOptions */ )
{
    OGRCouchDBDataSource   *poDS = new OGRCouchDBDataSource();

    if( !poDS->Open( pszName, TRUE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                         RegisterOGRCouchDB()                         */
/************************************************************************/

void RegisterOGRCouchDB()

{
    if( GDALGetDriverByName( "CouchDB" ) != NULL )
      return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "CouchDB" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "CouchDB / GeoCouch" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_couchdb.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "CouchDB:" );
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

    poDriver->pfnIdentify = OGRCouchDBDriverIdentify;
    poDriver->pfnOpen = OGRCouchDBDriverOpen;
    poDriver->pfnCreate = OGRCouchDBDriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
