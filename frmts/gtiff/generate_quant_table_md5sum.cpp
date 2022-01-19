/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  Generator of quant_table_md5sum.h
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys dot com>
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

#include <cassert>
#include "cpl_md5.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include <tiffio.h>
#include "gdal_priv.h"

static const GByte* GTIFFFindNextTable( const GByte* paby, GByte byMarker,
                                        int nLen, int* pnLenTable )
{
    for( int i = 0; i + 1 < nLen; )
    {
        if( paby[i] != 0xFF )
            return nullptr;
        ++i;
        if( paby[i] == 0xD8 )
        {
            ++i;
            continue;
        }
        if( i + 2 >= nLen )
            return nullptr;
        int nMarkerLen = paby[i+1] * 256 + paby[i+2];
        if( i+1+nMarkerLen >= nLen )
            return nullptr;
        if( paby[i] == byMarker )
        {
            if( pnLenTable ) *pnLenTable = nMarkerLen;
            return paby + i + 1;
        }
        i += 1 + nMarkerLen;
    }
    return nullptr;
}

void generate(int nBands, uint16_t nPhotometric, uint16_t nBitsPerSample)
{
    char** papszOpts = nullptr;
    papszOpts = CSLSetNameValue(papszOpts,
                                            "COMPRESS", "JPEG");
    if( nPhotometric == PHOTOMETRIC_YCBCR )
        papszOpts = CSLSetNameValue(papszOpts,
                                            "PHOTOMETRIC", "YCBCR");
    else if( nPhotometric == PHOTOMETRIC_SEPARATED )
        papszOpts = CSLSetNameValue(papszOpts,
                                            "PHOTOMETRIC", "CMYK");
    papszOpts = CSLSetNameValue(papszOpts,
                                            "BLOCKYSIZE", "16");
    if( nBitsPerSample == 12 )
        papszOpts = CSLSetNameValue(papszOpts,
                                                "NBITS", "12");

    CPLString osTmpFilename;
    osTmpFilename.Printf( "/vsimem/gtiffdataset_guess_jpeg_quality_tmp" );

    for( int nQuality = 1; nQuality <= 100; ++nQuality )
    {
        papszOpts = CSLSetNameValue(papszOpts,
                               "JPEG_QUALITY", CPLSPrintf("%d", nQuality));
        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLString osTmp;
        std::unique_ptr<GDALDataset> poDS(
            GDALDriver::FromHandle(GDALGetDriverByName("GTiff"))
             ->Create(osTmpFilename.c_str(),
                      16, 16,
                      (nBands <= 4) ? nBands : 1,
                      nBitsPerSample == 8 ? GDT_Byte : GDT_UInt16,
                      papszOpts));
        assert( poDS );
        poDS.reset();
        CPLPopErrorHandler();

        TIFF* hTIFFTmp = TIFFOpen(osTmpFilename.c_str(), "rb");
        uint32_t nJPEGTableSizeTry = 0;
        void* pJPEGTableTry = nullptr;
        if( TIFFGetField(hTIFFTmp, TIFFTAG_JPEGTABLES,
                         &nJPEGTableSizeTry, &pJPEGTableTry) )
        {
            const GByte* const pabyTable = static_cast<const GByte*>(pJPEGTableTry);
            int nLen = static_cast<int>(nJPEGTableSizeTry);
            const GByte* paby = pabyTable;

            struct CPLMD5Context context;
            CPLMD5Init( &context );

            while( true )
            {
                int nLenTable = 0;
                const GByte* pabyNew =
                    GTIFFFindNextTable(paby, 0xDB, nLen, &nLenTable);
                if( pabyNew == nullptr )
                    break;
                CPLMD5Update( &context, pabyNew, nLenTable);
                pabyNew += nLenTable;
                nLen -= static_cast<int>(pabyNew - paby);
                paby = pabyNew;
            }

            GByte digest[16];
            CPLMD5Final(digest, &context);
            printf("{");  /*ok*/
            for(int i=0; i <16;i++)
                printf("0x%02X,", digest[i]);  /*ok*/
            printf("}, // quality %d\n", nQuality);  /*ok*/
        }

        TIFFClose(hTIFFTmp);
    }

    CSLDestroy(papszOpts);
}

int main()
{
    GDALAllRegister();
    printf("// This file is automatically generated by generate_quant_table_md5sum. DO NOT EDIT !!!\n\n");  /*ok*/

    {
        printf("// Valid for bands = 1, PHOTOMETRIC_MINISBLACK\n");  /*ok*/
        printf("// Valid for bands = 3, PHOTOMETRIC_RGB\n");  /*ok*/
        printf("// Valid for bands = 4, PHOTOMETRIC_SEPARATED\n");  /*ok*/
        printf("const uint8_t md5JPEGQuantTable_generic_8bit[][16] = {\n");  /*ok*/
        int nBands = 1;
        uint16_t nPhotometric = PHOTOMETRIC_MINISBLACK;
        uint16_t nBitsPerSample = 8;
        generate(nBands, nPhotometric, nBitsPerSample);
        printf("};\n");  /*ok*/
    }
    printf("\n");  /*ok*/
    {
        printf("const uint8_t md5JPEGQuantTable_3_YCBCR_8bit[][16] = {\n"); /*ok*/
        int nBands = 3;
        uint16_t nPhotometric = PHOTOMETRIC_YCBCR;
        uint16_t nBitsPerSample = 8;
        generate(nBands, nPhotometric, nBitsPerSample);
        printf("};\n");  /*ok*/
    }

    return 0;
}
