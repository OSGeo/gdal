/******************************************************************************
 * $Id$
 *
 * Project:  GPX Translator
 * Purpose:  Implements OGRGPXDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2007-2008, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_gpx.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGPXDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == NULL )
        return NULL;

    if( strstr((const char*)poOpenInfo->pabyHeader, "<gpx") == NULL )
        return NULL;

    OGRGPXDataSource   *poDS = new OGRGPXDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, FALSE ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRGPXDriverCreate( const char * pszName,
                                    int nBands, int nXSize, int nYSize, GDALDataType eDT,
                                    char **papszOptions )

{
    OGRGPXDataSource   *poDS = new OGRGPXDataSource();

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

static CPLErr OGRGPXDriverDelete( const char *pszFilename )

{
    if( VSIUnlink( pszFilename ) == 0 )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRGPX()                           */
/************************************************************************/

void RegisterOGRGPX()

{
    if (! GDAL_CHECK_VERSION("OGR/GPX driver"))
        return;
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "GPX" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "GPX" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "GPX" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gpx" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_gpx.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
#ifdef WIN32
"  <Option name='LINEFORMAT' type='string-select' description='end-of-line sequence' default='CRLF'>"
#else
"  <Option name='LINEFORMAT' type='string-select' description='end-of-line sequence' default='LF'>"
#endif
"    <Value>CRLF</Value>"
"    <Value>LF</Value>"
"  </Option>"
"  <Option name='GPX_USE_EXTENSIONS' type='boolean' description='Whether to write non-GPX attributes in an <extensions> tag' default='NO'/>"
"  <Option name='GPX_EXTENSIONS_NS' type='string' description='Namespace value used for extension tags' default='ogr'/>"
"  <Option name='GPX_EXTENSIONS_NS_URL' type='string' description='Namespace URI' default='http://osgeo.org/gdal'/>"
"</CreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='FORCE_GPX_TRACK' type='boolean' description='Whether to force layers with geometries of type wkbLineString as tracks' default='NO'/>"
"  <Option name='FORCE_GPX_ROUTE' type='boolean' description='Whether to force layers with geometries of type wkbMultiLineString (with single line string in them) as routes' default='NO'/>"
"</LayerCreationOptionList>");

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRGPXDriverOpen;
        poDriver->pfnCreate = OGRGPXDriverCreate;
        poDriver->pfnDelete = OGRGPXDriverDelete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
