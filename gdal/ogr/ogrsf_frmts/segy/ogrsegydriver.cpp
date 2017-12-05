/******************************************************************************
 *
 * Project:  SEG-Y Translator
 * Purpose:  Implements OGRSEGYDriver class.
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMSEGYS OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_segy.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           EBCDICToASCII                              */
/************************************************************************/

static const GByte EBCDICToASCII[] =
{
    0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F, 0x97, 0x8D, 0x8E, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x9D, 0x85, 0x08, 0x87,
    0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F, 0x80, 0x81, 0x82, 0x83,
    0x84, 0x0A, 0x17, 0x1B, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,
    0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04, 0x98, 0x99, 0x9A, 0x9B,
    0x14, 0x15, 0x9E, 0x1A, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C, 0x26, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0xAC,
    0x2D, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA6, 0x2C,
    0x25, 0x5F, 0x3E, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22, 0x00, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
    0x51, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C, 0x00, 0x53, 0x54,
    0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x9F,
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRSEGYDriverOpen( GDALOpenInfo* poOpenInfo )
{
    if( poOpenInfo->eAccess == GA_Update ||
        poOpenInfo->fpL == NULL ||
        !poOpenInfo->TryToIngest(3200+400) ||
        poOpenInfo->nHeaderBytes < 3200+400 )
    {
        return NULL;
    }
    if( STARTS_WITH_CI((const char*)poOpenInfo->pabyHeader, "%PDF"))
    {
        return NULL;
    }

// --------------------------------------------------------------------
//      Try to decode the header encoded as EBCDIC and then ASCII
// --------------------------------------------------------------------

    const GByte* pabyTextHeader = poOpenInfo->pabyHeader;
    GByte* pabyASCIITextHeader = static_cast<GByte *>(CPLMalloc(3200 + 40 + 1));
    for( int k = 0; k < 2; k++ )
    {
        int i = 0;  // Used after for.
        int j = 0;  // Used after for.
        for( ; i < 3200; i++ )
        {
            GByte chASCII = (k == 0) ? EBCDICToASCII[pabyTextHeader[i]] :
                                       pabyTextHeader[i];
            if( chASCII < 32 && chASCII != '\t' &&
                chASCII != '\n' && chASCII != '\r' )
            {
                // Nuls are okay in an ASCII header if after the first "C1".
                if( !(i > 2 && chASCII == '\0') )
                {
                    break;
                }
            }
            pabyASCIITextHeader[j++] = chASCII;
            if( chASCII != '\n' && ((i + 1) % 80) == 0 )
                pabyASCIITextHeader[j++] = '\n';
        }
        pabyASCIITextHeader[j] = '\0';

        if( i == 3200 )
            break;
        if( k == 1 )
        {
            CPLFree(pabyASCIITextHeader);
            return NULL;
        }
    }

#if DEBUG_VERBOSE
    CPLDebug("SEGY", "Header = \n%s", pabyASCIITextHeader);
#endif
    CPLFree(pabyASCIITextHeader);
    pabyASCIITextHeader = NULL;

// --------------------------------------------------------------------
//      Read the next 400 bytes, where the Binary File Header is
//      located
// --------------------------------------------------------------------

    const GByte* abyFileHeader = poOpenInfo->pabyHeader + 3200;

// --------------------------------------------------------------------
//      First check that this binary header is not EBCDIC nor ASCII
// --------------------------------------------------------------------
    for( int k = 0; k < 2; k++ )
    {
        int i = 0;  // Used after for.
        for( ; i < 400; i++ )
        {
            GByte chASCII = (k == 0) ? abyFileHeader[i] :
                                       EBCDICToASCII[abyFileHeader[i]];
            // A translated 0 value, when source value is not 0, means an
            // invalid EBCDIC value. Bail out also for control characters.
            if( chASCII < 32 && chASCII != '\t' &&
                chASCII != '\n' && chASCII != '\r' )
            {
                break;
            }
        }
        if( i == 400 )
        {
            CPLFree(pabyASCIITextHeader);
            return NULL;
        }
    }

    OGRSEGYDataSource *poDS = new OGRSEGYDataSource();

    if( !poDS->Open( poOpenInfo->pszFilename,
                     (const char*)pabyASCIITextHeader ) )
    {
        CPLFree(pabyASCIITextHeader);
        delete poDS;
        poDS = NULL;
    }

    CPLFree(pabyASCIITextHeader);
    return poDS;
}

/************************************************************************/
/*                           RegisterOGRSEGY()                           */
/************************************************************************/

void RegisterOGRSEGY()

{
    if( GDALGetDriverByName( "SEGY" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "SEGY" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "SEG-Y" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_segy.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRSEGYDriverOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
