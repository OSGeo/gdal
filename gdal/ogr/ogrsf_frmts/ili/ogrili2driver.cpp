/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 2 Translator
 * Purpose:  Implements OGRILI2Layer class.
 * Author:   Markus Schnider, Sourcepole AG
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

#include "ogr_ili2.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRILI2DriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRILI2DataSource    *poDS;

    if( poOpenInfo->eAccess == GA_Update ||
        (!poOpenInfo->bStatOK && strchr(poOpenInfo->pszFilename, ',') == NULL) )
        return NULL;

    if( poOpenInfo->fpL != NULL )
    {
        if( poOpenInfo->pabyHeader[0] != '<' 
            || strstr((const char*)poOpenInfo->pabyHeader,"interlis.ch/INTERLIS2") == NULL )
        {
            return NULL;
        }
    }
    else if( poOpenInfo->bIsDirectory )
        return NULL;

    poDS = new OGRILI2DataSource();

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

static GDALDataset *OGRILI2DriverCreate( const char * pszName,
                                         CPL_UNUSED int nBands,
                                         CPL_UNUSED int nXSize,
                                         CPL_UNUSED int nYSize,
                                         CPL_UNUSED GDALDataType eDT,
                                         char **papszOptions )
{
    OGRILI2DataSource    *poDS = new OGRILI2DataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRILI2()                           */
/************************************************************************/

void RegisterOGRILI2() {
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "Interlis 2" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "Interlis 2" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Interlis 2" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_ili.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "xtf xml ili imd" );

        poDriver->pfnOpen = OGRILI2DriverOpen;
        poDriver->pfnCreate = OGRILI2DriverCreate;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
