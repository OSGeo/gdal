/******************************************************************************
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

#include "xercesc_headers.h"

#include "cpl_conv.h"
#include "ogr_ili2.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$");


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRILI2DriverOpen( GDALOpenInfo* poOpenInfo )

{
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

    OGRILI2DataSource *poDS = new OGRILI2DataSource();

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

static GDALDataset *OGRILI2DriverCreate( const char * pszName,
                                         int /* nBands */,
                                         int /* nXSize */,
                                         int /* nYSize */,
                                         GDALDataType /* eDT */,
                                         char **papszOptions )
{
    OGRILI2DataSource *poDS = new OGRILI2DataSource();

    if( !poDS->Create( pszName, papszOptions ) )
    {
        delete poDS;
        return NULL;
    }

    return poDS;
}

/************************************************************************/
/*                         OGRILI2DriverUnload()                        */
/************************************************************************/

static void OGRILI2DriverUnload ( GDALDriver* )
{
    if( getenv("ILI2_TERMINATE_XERCES") )
        XMLPlatformUtils::Terminate();
}

/************************************************************************/
/*                           RegisterOGRILI2()                           */
/************************************************************************/

void RegisterOGRILI2() {
    if( GDALGetDriverByName( "Interlis 2" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "Interlis 2" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Interlis 2" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_ili.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "xtf xml ili" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='MODEL' type='string' description='Filename of the model in IlisMeta format (.imd)'/>"
"</OpenOptionList>" );

    poDriver->pfnOpen = OGRILI2DriverOpen;
    poDriver->pfnCreate = OGRILI2DriverCreate;
    poDriver->pfnUnloadDriver = OGRILI2DriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
