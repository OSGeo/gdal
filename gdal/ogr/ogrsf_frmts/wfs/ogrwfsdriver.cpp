/******************************************************************************
 * $Id $
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGRWFSDriver.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_wfs.h"
#include "cpl_conv.h"

// g++ -fPIC -g -Wall ogr/ogrsf_frmts/wfs/*.cpp -shared -o ogr_WFS.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/gml -Iogr/ogrsf_frmts/wfs -L. -lgdal

CPL_CVSID("$Id$");

extern "C" void RegisterOGRWFS();

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

static int OGRWFSDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( !EQUALN(poOpenInfo->pszFilename, "WFS:", 4) )
    {
        if( poOpenInfo->fpL == NULL )
            return FALSE;
        if( !EQUALN((const char*)poOpenInfo->pabyHeader,"<OGRWFSDataSource>",18) &&
            strstr((const char*)poOpenInfo->pabyHeader,"<WFS_Capabilities") == NULL &&
            strstr((const char*)poOpenInfo->pabyHeader,"<wfs:WFS_Capabilities") == NULL)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRWFSDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !OGRWFSDriverIdentify(poOpenInfo) )
        return NULL;

    OGRWFSDataSource   *poDS = new OGRWFSDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename,
                     poOpenInfo->eAccess == GA_Update,
                     poOpenInfo->papszOpenOptions ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRWFS()                           */
/************************************************************************/

void RegisterOGRWFS()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "WFS" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "WFS" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "OGC WFS (Web Feature Service)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_wfs.html" );

        poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "WFS:" );

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='URL' type='string' description='URL to the WFS server endpoint' required='true'/>"
"  <Option name='TRUST_CAPABILITIES_BOUNDS' type='boolean' description='Whether to trust layer bounds declared in GetCapabilities response' default='NO'/>"
"</OpenOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnIdentify = OGRWFSDriverIdentify;
        poDriver->pfnOpen = OGRWFSDriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

