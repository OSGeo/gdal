/******************************************************************************
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                      OGROSMDriverIdentify()                          */
/************************************************************************/

static int OGROSMDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->fpL == NULL || poOpenInfo->nHeaderBytes == 0 )
        return GDAL_IDENTIFY_FALSE;

    if( strstr((const char*)poOpenInfo->pabyHeader, "<osm") != NULL )
    {
        return GDAL_IDENTIFY_TRUE;
    }

    const int nLimitI =
        poOpenInfo->nHeaderBytes - static_cast<int>(strlen("OSMHeader"));
    for(int i = 0; i < nLimitI; i++)
    {
        if( memcmp( poOpenInfo->pabyHeader + i, "OSMHeader",
                    strlen("OSMHeader") ) == 0 )
        {
            return GDAL_IDENTIFY_TRUE;
        }
    }

    return GDAL_IDENTIFY_FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGROSMDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update )
        return NULL;
    if( OGROSMDriverIdentify(poOpenInfo) == FALSE )
        return NULL;

    OGROSMDataSource *poDS = new OGROSMDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions ) )
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
    if( !GDAL_CHECK_VERSION("OGR/OSM driver") )
        return;

    if( GDALGetDriverByName( "OSM" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "OSM" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "OpenStreetMap XML and PBF" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "osm pbf" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_osm.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='CONFIG_FILE' type='string' description='Configuration filename.'/>"
"  <Option name='USE_CUSTOM_INDEXING' type='boolean' description='Whether to enable custom indexing.' default='YES'/>"
"  <Option name='COMPRESS_NODES' type='boolean' description='Whether to compress nodes in temporary DB.' default='NO'/>"
"  <Option name='MAX_TMPFILE_SIZE' type='int' description='Maximum size in MB of in-memory temporary file. If it exceeds that value, it will go to disk' default='100'/>"
"  <Option name='INTERLEAVED_READING' type='boolean' description='Whether to enable interleaved reading.' default='NO'/>"
"</OpenOptionList>" );

    poDriver->pfnOpen = OGROSMDriverOpen;
    poDriver->pfnIdentify = OGROSMDriverIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
