/******************************************************************************
 *
 * Project:  OpenAir Translator
 * Purpose:  Implements OGROpenAirDriver.
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
#include "ogr_openair.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGROpenAirDriverOpen( GDALOpenInfo* poOpenInfo )

{
    if( poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == NULL ||
        !poOpenInfo->TryToIngest(10000) )
        return NULL;

    const char *pabyHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    bool bIsOpenAir =
        strstr(pabyHeader, "\nAC ") != NULL &&
        strstr(pabyHeader, "\nAN ") != NULL &&
        strstr(pabyHeader, "\nAL ") != NULL &&
        strstr(pabyHeader, "\nAH") != NULL;
    if( !bIsOpenAir )
    {
        // Some files, such as
        // http://soaringweb.org/Airspace/CZ/CZ_combined_2014_05_01.txt ,
        // have very long comments in the header, so we will have to
        // check further, but only do this is we have a hint that the
        // file might be a candidate.
        int nLen = poOpenInfo->nHeaderBytes;
        if( nLen < 10000 )
            return NULL;
        /* Check the 'Airspace' word in the header */
        if( strstr(pabyHeader, "Airspace")
            == NULL )
            return NULL;
        // Check that the header is at least UTF-8
        // but do not take into account partial UTF-8 characters at the end
        int nTruncated = 0;
        while(nLen > 0)
        {
            if( (poOpenInfo->pabyHeader[nLen-1] & 0xc0) != 0x80 )
            {
                break;
            }
            nLen --;
            nTruncated ++;
            if( nTruncated == 7 )
                return NULL;
        }
        if( !CPLIsUTF8(pabyHeader, nLen) )
            return NULL;
        if( !poOpenInfo->TryToIngest(30000) )
            return NULL;
        pabyHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
        bIsOpenAir =
            strstr(pabyHeader, "\nAC ") != NULL &&
            strstr(pabyHeader, "\nAN ") != NULL &&
            strstr(pabyHeader, "\nAL ") != NULL &&
            strstr(pabyHeader, "\nAH") != NULL;
        if( !bIsOpenAir )
            return NULL;
    }

    OGROpenAirDataSource *poDS = new OGROpenAirDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename ) )
    {
        delete poDS;
        poDS = NULL;
    }

    return poDS;
}

/************************************************************************/
/*                         RegisterOGROpenAir()                         */
/************************************************************************/

void RegisterOGROpenAir()

{
    if( GDALGetDriverByName( "OpenAir" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "OpenAir" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "OpenAir" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_openair.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGROpenAirDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
