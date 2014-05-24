/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGROSMDriver class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_osm.h"
#include "cpl_conv.h"

/* g++ -DHAVE_EXPAT -fPIC -g -Wall ogr/ogrsf_frmts/osm/ogrosmdriver.cpp ogr/ogrsf_frmts/osm/ogrosmdatasource.cpp ogr/ogrsf_frmts/osm/ogrosmlayer.cpp -Iport -Igcore -Iogr -Iogr/ogrsf_frmts/osm -Iogr/ogrsf_frmts/mitab -Iogr/ogrsf_frmts -shared -o ogr_OSM.so -L. -lgdal */

extern "C" void CPL_DLL RegisterOGROSM();

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGROSMDriverIdentify()                          */
/************************************************************************/

static int OGROSMDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if (poOpenInfo->fpL == NULL )
        return FALSE;
    const char* pszExt = CPLGetExtension(poOpenInfo->pszFilename);
    if( EQUAL(pszExt, "pbf") ||
        EQUAL(pszExt, "osm") )
        return TRUE;
    if( EQUALN(poOpenInfo->pszFilename, "/vsicurl_streaming/", strlen("/vsicurl_streaming/")) ||
        strcmp(poOpenInfo->pszFilename, "/vsistdin/") == 0 ||
        strcmp(poOpenInfo->pszFilename, "/dev/stdin/") == 0 )
        return -1;
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGROSMDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if (poOpenInfo->eAccess == GA_Update )
        return NULL;
    if( OGROSMDriverIdentify(poOpenInfo) == FALSE )
        return NULL;

    OGROSMDataSource   *poDS = new OGROSMDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                        RegisterOGROSM()                           */
/************************************************************************/

void RegisterOGROSM()
{
    if (! GDAL_CHECK_VERSION("OGR/OSM driver"))
        return;
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "OSM" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "OSM" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "OpenStreetMap XML and PBF" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "osm pbf" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_osm.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGROSMDriverOpen;
        poDriver->pfnIdentify = OGROSMDriverIdentify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

