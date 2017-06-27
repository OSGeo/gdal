/******************************************************************************
 *
 * Project:  SVG Translator
 * Purpose:  Implements OGRSVGDriver.
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

#include "ogr_svg.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

CPL_C_START
void RegisterOGRSVG();
CPL_C_END

// g++ -g -Wall -fPIC ogr/ogrsf_frmts/svg/*.c* -shared -o ogr_SVG.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/svg -L. -lgdal -DHAVE_EXPAT

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSVGDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == NULL )
        return NULL;

    if( strstr((const char*)poOpenInfo->pabyHeader, "<svg") == NULL )
        return NULL;

    OGRSVGDataSource *poDS = new OGRSVGDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRSVG()                           */
/************************************************************************/

void RegisterOGRSVG()

{
    if(! GDAL_CHECK_VERSION("OGR/SVG driver") )
        return;

    if( GDALGetDriverByName( "SVG" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SVG" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Scalable Vector Graphics" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "svg" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_svg.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRSVGDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
