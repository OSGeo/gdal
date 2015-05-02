/******************************************************************************
 * $Id$
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

#include "ogr_ili1.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRILI1DriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRILI1DataSource    *poDS;

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

    poDS = new OGRILI1DataSource();

    if( !poDS->Open( poOpenInfo->pszFilename, TRUE )
        || poDS->GetLayerCount() == 0 )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGRILI1DriverCreate( const char * pszName,
                                         CPL_UNUSED int nBands,
                                         CPL_UNUSED int nXSize,
                                         CPL_UNUSED int nYSize,
                                         CPL_UNUSED GDALDataType eDT,
                                         char **papszOptions )
{
    OGRILI1DataSource    *poDS = new OGRILI1DataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRILI1()                           */
/************************************************************************/

void RegisterOGRILI1() {
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "Interlis 1" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "Interlis 1" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Interlis 1" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_ili.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "itf ili imd" );

        poDriver->pfnOpen = OGRILI1DriverOpen;
        poDriver->pfnCreate = OGRILI1DriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
