/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

// Must be first for DEBUG_BOOL case
#include "ogr_gmlas.h"

// g++ -I/usr/include/json -DxDEBUG_VERBOSE -DDEBUG   -g -DDEBUG -ftrapv  -Wall -Wextra -Winit-self -Wunused-parameter -Wformat -Werror=format-security -Wno-format-nonliteral -Wlogical-op -Wshadow -Werror=vla -Wmissing-declarations -Wnon-virtual-dtor -Woverloaded-virtual -fno-operator-names ogr/ogrsf_frmts/gmlas/*.cpp -fPIC -shared -o ogr_GMLAS.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mem  -L. -lgdal   -I/home/even/spatialys/eea/inspire_gml/install-xerces-c-3.1.3/include 

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRGMLASDriverIdentify()                      */
/************************************************************************/

static int OGRGMLASDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "GMLAS:") )
        return FALSE;
    return TRUE;
}

/************************************************************************/
/*                           OGRGMLASDriverOpen()                       */
/************************************************************************/

static GDALDataset *OGRGMLASDriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRGMLASDataSource    *poDS;

    if( poOpenInfo->eAccess == GA_Update )
        return NULL;

    if( OGRGMLASDriverIdentify( poOpenInfo ) == FALSE )
        return NULL;

    poDS = new OGRGMLASDataSource();

    if( !poDS->Open(  poOpenInfo ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}


/************************************************************************/
/*                          RegisterOGRGMLAS()                          */
/************************************************************************/

void RegisterOGRGMLAS()

{
    if( GDALGetDriverByName( "GMLAS" ) != NULL )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "GMLAS" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Geography Markup Language (GML) "
                               "driven by application schemas" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gml" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "gml xml" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_gmlas.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, "GMLAS:" );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='XSD' type='string' description='Space separated list of "
            "filenames of XML schemas that apply to the data file'/>"
"  <Option name='CONFIG_FILE' type='string' "
            "description='Filename of the configuration file'/>"
"  <Option name='EXPOSE_METADATA_LAYERS' type='boolean' "
        "description='Whether metadata layers should be reported by default.' "
        "default='NO'/>"
"  <Option name='VALIDATE' type='boolean' description='Whether validation "
        "against the schema should be done' default='NO'/>"
"  <Option name='FAIL_IF_VALIDATION_ERROR' type='boolean' "
    "description='Whether a validation error should cause dataset opening to fail' "
    "default='NO'/>"
"  <Option name='REFRESH_CACHE' type='boolean' "
    "description='Whether remote schemas and resolved xlink resources should be downloaded from the server' "
    "default='NO'/>"
"  <Option name='SWAP_COORDINATES' type='string-select' "
    "description='Whether the order of geometry coordinates should be inverted.' "
    "default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"  <Option name='REMOVE_UNUSED_LAYERS' type='boolean' "
    "description='Whether unused layers should be removed' " "default='NO'/>"
"  <Option name='REMOVE_UNUSED_FIELDS' type='boolean' "
    "description='Whether unused fields should be removed' " "default='NO'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRGMLASDriverOpen;
    poDriver->pfnIdentify = OGRGMLASDriverIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
