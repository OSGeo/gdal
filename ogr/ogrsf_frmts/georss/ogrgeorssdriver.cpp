/******************************************************************************
 *
 * Project:  GeoRSS Translator
 * Purpose:  Implements OGRGeoRSSDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_georss.h"

#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_port.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_core.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGeoRSSDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == nullptr )
        return nullptr;

    if( strstr((const char*)poOpenInfo->pabyHeader, "<rss") == nullptr &&
        strstr((const char*)poOpenInfo->pabyHeader, "<feed") == nullptr &&
        strstr((const char*)poOpenInfo->pabyHeader, "<atom:feed") == nullptr )
        return nullptr;

    OGRGeoRSSDataSource   *poDS = new OGRGeoRSSDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename,
                     poOpenInfo->eAccess == GA_Update ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRGeoRSSDriverCreate( const char * pszName,
                                           CPL_UNUSED int nBands,
                                           CPL_UNUSED int nXSize,
                                           CPL_UNUSED int nYSize,
                                           CPL_UNUSED GDALDataType eDT,
                                           char **papszOptions )
{
    OGRGeoRSSDataSource *poDS = new OGRGeoRSSDataSource();

    if( !poDS->Create(pszName, papszOptions) )
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

static CPLErr OGRGeoRSSDriverDelete( const char *pszFilename )

{
    if( VSIUnlink( pszFilename ) == 0 )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                           RegisterOGRGeoRSS()                           */
/************************************************************************/

void RegisterOGRGeoRSS()

{
    if( ! GDAL_CHECK_VERSION("OGR/GeoRSS driver") )
        return;

    if( GDALGetDriverByName( "GeoRSS" ) != nullptr )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription("GeoRSS");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "GeoRSS");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/georss.html");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='FORMAT' type='string-select' description='whether the document must be in RSS 2.0 or Atom 1.0 format' default='RSS'>"
"    <Value>RSS</Value>"
"    <Value>ATOM</Value>"
"  </Option>"
"  <Option name='GEOM_DIALECT' type='string-select' description='encoding of location information' default='SIMPLE'>"
"    <Value>SIMPLE</Value>"
"    <Value>GML</Value>"
"    <Value>W3C_GEO</Value>"
"  </Option>"
"  <Option name='USE_EXTENSIONS' type='boolean' description='Whether extension fields (that is to say fields not in the base schema of RSS or Atom documents) will be written' default='NO'/>"
"  <Option name='WRITE_HEADER_AND_FOOTER' type='boolean' description='Whether header and footer are written' default='YES'/>"
"  <Option name='HEADER' type='string' description='XML content that will be put between the &lt;channel&gt; element and the first &lt;item&gt; element for a RSS document, or between the xml tag and the first &lt;entry&gt; element for an Atom document. If it is specified, it will overload the following options'/>"
"  <Option name='TITLE' type='string' description='value put inside the &lt;title&gt; element in the header'/>"
"  <Option name='DESCRIPTION' type='string' description='(RSS only) value put inside the &lt;description&gt; element in the header'/>"
"  <Option name='LINK' type='string' description='(RSS only) value put inside the &lt;link&gt; element in the header'/>"
"  <Option name='UPDATED' type='string' description='(RSS only) value put inside the &lt;updated&gt; element in the header. Should be formatted as a XML datetime'/>"
"  <Option name='AUTHOR_NAME' type='string' description='(ATOM only) value put inside the &lt;author&gt;&lt;name&gt; element in the header'/>"
"  <Option name='ID' type='string' description='(ATOM only) value put inside the &lt;id&gt; element in the header.'/>"
"</CreationOptionList>");
    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
                              "<LayerCreationOptionList/>");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = OGRGeoRSSDriverOpen;
    poDriver->pfnCreate = OGRGeoRSSDriverCreate;
    poDriver->pfnDelete = OGRGeoRSSDriverDelete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
