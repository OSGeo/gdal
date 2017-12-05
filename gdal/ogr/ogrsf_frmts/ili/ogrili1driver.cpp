/******************************************************************************
 *
 * Project:  Interlis 1 Translator
 * Purpose:  Implements OGRILI1Layer class.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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

#include "cpl_conv.h"
#include "ogr_ili1.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRILI1DriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update ||
        (!poOpenInfo->bStatOK && strchr(poOpenInfo->pszFilename, ',') == NULL) )
        return NULL;

    if( poOpenInfo->fpL != NULL )
    {
        if( strstr((const char*)poOpenInfo->pabyHeader,"SCNT") == NULL )
        {
            return NULL;
        }
    }
    else if( poOpenInfo->bIsDirectory )
        return NULL;

    OGRILI1DataSource *poDS = new OGRILI1DataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                     TRUE )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRILI1DriverCreate( const char * pszName,
                                         int /* nBands */,
                                         int /* nXSize */,
                                         int /* nYSize */,
                                         GDALDataType /* eDT */,
                                         char **papszOptions )
{
    OGRILI1DataSource *poDS = new OGRILI1DataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRILI1()                           */
/************************************************************************/

void RegisterOGRILI1() {
    if( GDALGetDriverByName( "Interlis 1" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "Interlis 1" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Interlis 1" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_ili.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "itf ili" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='MODEL' type='string' description='Filename of the model in IlisMeta format (.imd)'/>"
"</OpenOptionList>" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRILI1DriverOpen;
    poDriver->pfnCreate = OGRILI1DriverCreate;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
