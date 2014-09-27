/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONDriver class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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
#include "ogr_geojson.h"
#include <cpl_conv.h>

/************************************************************************/
/*                        OGRGeoJSONDriverIdentify()                    */
/************************************************************************/

static int OGRGeoJSONDriverIdentifyInternal( GDALOpenInfo* poOpenInfo,
                                     GeoJSONSourceType& nSrcType )
{
/* -------------------------------------------------------------------- */
/*      Determine type of data source: text file (.geojson, .json),     */
/*      Web Service or text passed directly and load data.              */
/* -------------------------------------------------------------------- */

    nSrcType = GeoJSONGetSourceType( poOpenInfo );
    if( nSrcType == eGeoJSONSourceUnknown )
        return FALSE;
    if( nSrcType == eGeoJSONSourceService )
        return -1;
    return TRUE;
}

/************************************************************************/
/*                        OGRGeoJSONDriverIdentify()                    */
/************************************************************************/

static int OGRGeoJSONDriverIdentify( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType nSrcType;
    return OGRGeoJSONDriverIdentifyInternal(poOpenInfo, nSrcType);
}

/************************************************************************/
/*                           Open()                                     */
/************************************************************************/

static GDALDataset* OGRGeoJSONDriverOpen( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType nSrcType;
    if( OGRGeoJSONDriverIdentifyInternal(poOpenInfo, nSrcType) == FALSE )
        return NULL;

    OGRGeoJSONDataSource* poDS = NULL;
    poDS = new OGRGeoJSONDataSource();

/* -------------------------------------------------------------------- */
/*      Processing configuration options.                               */
/* -------------------------------------------------------------------- */

    // TODO: Currently, options are based on environment variables.
    //       This is workaround for not yet implemented Andrey's concept
    //       described in document 'RFC 10: OGR Open Parameters'.

    poDS->SetGeometryTranslation( OGRGeoJSONDataSource::eGeometryPreserve );
    const char* pszOpt = CPLGetConfigOption("GEOMETRY_AS_COLLECTION", NULL);
    if( NULL != pszOpt && EQUALN(pszOpt, "YES", 3) )
    {
            poDS->SetGeometryTranslation(
                OGRGeoJSONDataSource::eGeometryAsCollection );
    }

    poDS->SetAttributesTranslation( OGRGeoJSONDataSource::eAtributesPreserve );
    pszOpt = CPLGetConfigOption("ATTRIBUTES_SKIP", NULL);
    if( NULL != pszOpt && EQUALN(pszOpt, "YES", 3) )
    {
        poDS->SetAttributesTranslation( 
            OGRGeoJSONDataSource::eAtributesSkip );
    }

/* -------------------------------------------------------------------- */
/*      Open and start processing GeoJSON datasoruce to OGR objects.    */
/* -------------------------------------------------------------------- */
    if( !poDS->Open( poOpenInfo, nSrcType ) )
    {
        delete poDS;
        poDS= NULL;
    }

    if( NULL != poDS && poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "GeoJSON Driver doesn't support update." );
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRGeoJSONDriverCreate( const char * pszName,
                                            CPL_UNUSED int nBands,
                                            CPL_UNUSED int nXSize,
                                            CPL_UNUSED int nYSize,
                                            CPL_UNUSED GDALDataType eDT,
                                            char **papszOptions )
{
    OGRGeoJSONDataSource* poDS = new OGRGeoJSONDataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRGeoJSONDriverDelete( const char *pszFilename )
{
    if( VSIUnlink( pszFilename ) == 0 )
    {
        return CE_None;
    }
    
    CPLDebug( "GeoJSON", "Failed to delete \'%s\'", pszFilename);

    return CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRGeoJSON()                       */
/************************************************************************/

void RegisterOGRGeoJSON()
{
    if( !GDAL_CHECK_VERSION("OGR/GeoJSON driver") )
        return;

    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "GeoJSON" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GeoJSON" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "GeoJSON" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "json geojson topojson" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_geojson.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='WRITE_BBOX' type='boolean' description='whether to write a bbox property with the bounding box of the geometries at the feature and feature collection level' default='NO'/>"
"  <Option name='COORDINATE_PRECISION' type='int' description='Number of decimal for coordinates' default='10'/>"
"</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRGeoJSONDriverOpen;
        poDriver->pfnIdentify = OGRGeoJSONDriverIdentify;
        poDriver->pfnCreate = OGRGeoJSONDriverCreate;
        poDriver->pfnDelete = OGRGeoJSONDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
