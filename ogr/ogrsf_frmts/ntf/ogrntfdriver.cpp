/******************************************************************************
 *
 * Project:  UK NTF Reader
 * Purpose:  Implements OGRNTFDriver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ntf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                            OGRNTFDriver                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRNTFDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( !poOpenInfo->bStatOK )
        return nullptr;

    if( poOpenInfo->nHeaderBytes != 0 )
    {
        if( poOpenInfo->nHeaderBytes < 80 )
            return nullptr;
        const char* pszHeader = (const char*)poOpenInfo->pabyHeader;
        if( !STARTS_WITH_CI(pszHeader, "01") )
            return nullptr;

        int j = 0;  // Used after for.
        for( ; j < 80; j++ )
        {
            if( pszHeader[j] == 10 || pszHeader[j] == 13 )
                break;
        }

        if( j == 80 || pszHeader[j-1] != '%' )
            return nullptr;
    }

    OGRNTFDataSource *poDS = new OGRNTFDataSource;
    if( !poDS->Open( poOpenInfo->pszFilename, TRUE ) )
    {
        delete poDS;
        poDS = nullptr;
    }

    if( poDS != nullptr && poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "NTF Driver doesn't support update." );
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRNTF()                           */
/************************************************************************/

void RegisterOGRNTF()

{
    if( GDALGetDriverByName( "UK .NTF" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "UK .NTF" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "UK .NTF" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/ntf.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRNTFDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
