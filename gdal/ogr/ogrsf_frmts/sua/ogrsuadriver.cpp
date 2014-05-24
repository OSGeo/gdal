/******************************************************************************
 * $Id$
 *
 * Project:  SUA Translator
 * Purpose:  Implements OGRSUADriver.
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

#include "ogr_sua.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

extern "C" void RegisterOGRSUA();

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSUADriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == NULL ||
        !poOpenInfo->TryToIngest(10000) )
        return NULL;

    int bIsSUA = ( strstr((const char*)poOpenInfo->pabyHeader, "\nTYPE=") != NULL &&
            strstr((const char*)poOpenInfo->pabyHeader, "\nTITLE=") != NULL &&
            (strstr((const char*)poOpenInfo->pabyHeader, "\nPOINT=") != NULL ||
            strstr((const char*)poOpenInfo->pabyHeader, "\nCIRCLE ") != NULL));
    if( !bIsSUA )
        return NULL;

    OGRSUADataSource   *poDS = new OGRSUADataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRSUA()                           */
/************************************************************************/

void RegisterOGRSUA()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "SUA" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "SUA" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Tim Newport-Peace's Special Use Airspace Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_sua.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRSUADriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

