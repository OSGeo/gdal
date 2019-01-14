/******************************************************************************
 *
 * Project:  Raster Matrix Format
 * Purpose:  Implementation of the JPEG decompression algorithm as used in
 *           GIS "Panorama" raster files.
 * Author:   Andrew Sudorgin (drons [a] list dot ru)
 *
 ******************************************************************************
 * Copyright (c) 2018, Andrew Sudorgin
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

#ifdef HAVE_LIBJPEG

#include <algorithm>
#include "cpl_conv.h"
#include "rmfdataset.h"
#include "../mem/memdataset.h"

/************************************************************************/
/*                          JPEGDecompress()                            */
/************************************************************************/

size_t RMFDataset::JPEGDecompress(const GByte* pabyIn, GUInt32 nSizeIn,
                                  GByte* pabyOut, GUInt32 nSizeOut,
                                  GUInt32 nRawXSize, GUInt32 nRawYSize)
{
    if(pabyIn == nullptr ||
       pabyOut == nullptr ||
       nSizeOut < nSizeIn ||
       nSizeIn < 2)
       return 0;

    CPLString   osTmpFilename;
    VSILFILE*   fp;

    osTmpFilename.Printf("/vsimem/rmfjpeg/%p.jpg", pabyIn);

    fp = VSIFileFromMemBuffer(osTmpFilename, const_cast<GByte*>(pabyIn),
                              nSizeIn, FALSE);

    if(fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RMF JPEG: Can't create %s file", osTmpFilename.c_str());
        return 0;
    }

    const char*     apszAllowedDrivers[] = {"JPEG", nullptr};
    GDALDatasetH    hTile;


    CPLConfigOptionSetter   oNoReadDir("GDAL_DISABLE_READDIR_ON_OPEN",
                                       "EMPTY_DIR", false);

    hTile = GDALOpenEx(osTmpFilename, GDAL_OF_RASTER | GDAL_OF_INTERNAL,
                       apszAllowedDrivers, nullptr, nullptr);

    if(hTile == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RMF JPEG: Can't open %s file", osTmpFilename.c_str());
        VSIFCloseL(fp);
        VSIUnlink(osTmpFilename);
        return 0;
    }

    if(GDALGetRasterCount(hTile) != RMF_JPEG_BAND_COUNT)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RMF JPEG: Invalid band count %d in tile, must be %d",
                 GDALGetRasterCount(hTile), (int)RMF_JPEG_BAND_COUNT);
        GDALClose(hTile);
        VSIFCloseL(fp);
        VSIUnlink(osTmpFilename);
        return 0;
    }

    int nBandCount = GDALGetRasterCount(hTile);

    int nImageWidth = std::min(GDALGetRasterXSize(hTile),
                               static_cast<int>(nRawXSize));
    int nImageHeight = std::min(GDALGetRasterYSize(hTile),
                                static_cast<int>(nRawYSize));

    if( nRawXSize * nBandCount * nImageHeight > nSizeOut )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RMF JPEG: Too small output buffer");
        GDALClose(hTile);
        VSIFCloseL(fp);
        VSIUnlink(osTmpFilename);
        return 0;
    }

    CPLErr  eErr;
    size_t  nRet;
    int     aBandMap[RMF_JPEG_BAND_COUNT] = {3, 2, 1};
    eErr = GDALDatasetRasterIO(hTile, GF_Read, 0, 0,
                               nImageWidth, nImageHeight, pabyOut,
                               nImageWidth, nImageHeight, GDT_Byte,
                               nBandCount, aBandMap,
                               nBandCount, nRawXSize * nBandCount, 1);
    if(CE_None != eErr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RMF JPEG: Error decompress JPEG tile");
        nRet = 0;
    }
    else
    {
        nRet = static_cast<size_t>(nRawXSize * nBandCount * nImageHeight);
    }

    GDALClose(hTile);
    VSIFCloseL(fp);
    VSIUnlink(osTmpFilename);

    return nRet;
}

/************************************************************************/
/*                            JPEGCompress()                            */
/************************************************************************/

size_t RMFDataset::JPEGCompress(const GByte* pabyIn, GUInt32 nSizeIn,
                                GByte* pabyOut, GUInt32 nSizeOut,
                                GUInt32 nRawXSize, GUInt32 nRawYSize,
                                const RMFDataset* poDS)
{
    if(pabyIn == nullptr ||
       pabyOut == nullptr ||
       nSizeIn < 2)
       return 0;

    GDALDriverH hJpegDriver = GDALGetDriverByName("JPEG");

    if(hJpegDriver == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RMF: JPEG driver not found");
        return 0;
    }

    GDALDataType    eType = GDT_Byte;
    GDALDataset*    poMemDS = MEMDataset::Create("", nRawXSize, nRawYSize, 0,
                                                 eType, nullptr);

    for(int iBand = 0; iBand < RMF_JPEG_BAND_COUNT; ++iBand)
    {
        char szBuffer[32] = {};
        const GByte* pabyBand = pabyIn + (RMF_JPEG_BAND_COUNT - iBand - 1);
        int nRet = CPLPrintPointer(szBuffer, (void*)(pabyBand),
                                   sizeof(szBuffer));
        szBuffer[nRet] = 0;

        char szBuffer0[64] = {};
        snprintf( szBuffer0, sizeof(szBuffer0), "DATAPOINTER=%s", szBuffer );

        char szBuffer1[64] = "PIXELOFFSET=3";
        char szBuffer2[64] = {};
        snprintf( szBuffer2, sizeof(szBuffer2),
                  "LINEOFFSET=%d", nRawXSize*RMF_JPEG_BAND_COUNT );

        char* apszOptions[4] = { szBuffer0, szBuffer1, szBuffer2, nullptr };

        poMemDS->AddBand(eType, apszOptions);
    }

    CPLString   osTmpFilename;
    osTmpFilename.Printf("/vsimem/rmfjpeg/%p.jpg", pabyIn);

    char szQuality[32] = {};
    if(poDS != nullptr && poDS->sHeader.iJpegQuality > 0)
    {
        snprintf(szQuality, sizeof(szQuality),
                 "QUALITY=%d", (int)poDS->sHeader.iJpegQuality);
    }
    else
    {
        snprintf(szQuality, sizeof(szQuality), "QUALITY=75");
    }

    char* apszJpegOptions[2] = {szQuality, nullptr};

    GDALDatasetH   hJpeg = GDALCreateCopy(hJpegDriver, osTmpFilename, poMemDS,
                                          0, apszJpegOptions, nullptr, nullptr);
    GDALClose(poMemDS);

    if(hJpeg == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RMF JPEG: Error compress JPEG tile");
        VSIUnlink(osTmpFilename);
        return 0;
    }

    GDALClose(hJpeg);

    vsi_l_offset    nDataLength = 0;
    GByte* pabyBuffer = VSIGetMemFileBuffer(osTmpFilename, &nDataLength, TRUE);

    if(nDataLength < nSizeOut)
    {
        memcpy(pabyOut, pabyBuffer, static_cast<size_t>(nDataLength));
        CPLFree(pabyBuffer);
        return static_cast<size_t>(nDataLength);
    }

    CPLFree(pabyBuffer);
    return 0;
}
#endif //HAVE_LIBJPEG
