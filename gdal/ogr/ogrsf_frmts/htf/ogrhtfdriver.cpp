/******************************************************************************
 *
 * Project:  HTF Translator
 * Purpose:  Implements OGRHTFDriver.
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

#include "cpl_conv.h"
#include "ogr_htf.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRHTFDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == NULL )
        return NULL;

    if( !STARTS_WITH( reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                      "HTF HEADER") )
        return NULL;

    OGRHTFDataSource *poDS = new OGRHTFDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRHTF()                           */
/************************************************************************/

void RegisterOGRHTF()

{
    if( GDALGetDriverByName( "HTF" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "HTF" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "Hydrographic Transfer Vector" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_htf.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRHTFDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
