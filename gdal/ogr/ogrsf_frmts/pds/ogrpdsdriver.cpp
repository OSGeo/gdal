/******************************************************************************
 * $Id$
 *
 * Project:  PDS Translator
 * Purpose:  Implements OGRPDSDriver.
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

#include "ogr_pds.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

extern "C" void RegisterOGRPDS();

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRPDSDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == NULL )
        return NULL;

    if( strstr((const char*)poOpenInfo->pabyHeader, "PDS_VERSION_ID") == NULL )
        return NULL;

    OGRPDSDataSource   *poDS = new OGRPDSDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}


/************************************************************************/
/*                           RegisterOGRPDS()                      */
/************************************************************************/

void RegisterOGRPDS()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "OGR_PDS" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "OGR_PDS" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Planetary Data Systems TABLE" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "drv_pds.html" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = OGRPDSDriverOpen;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

