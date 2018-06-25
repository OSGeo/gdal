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

/************************************************************************/
/*                          JPEGDecompress()                            */
/************************************************************************/

int RMFDataset::JPEGDecompress(const GByte* pabyIn, GUInt32 nSizeIn,
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
    CPLErr  eErr;
    int     nRet;

    eErr = GDALDatasetRasterIO(hTile, GF_Read, 0, 0,
                               nImageWidth, nImageHeight, pabyOut,
                               nImageWidth, nImageHeight, GDT_Byte,
                               nBandCount, nullptr,
                               nBandCount, nRawXSize * nBandCount, 1);
    if(CE_None != eErr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "RMF JPEG: Error decompress JPEG tile");
        nRet = 0;
    }
    else
    {
        nRet = nRawXSize * nBandCount * nImageHeight;
    }

    GDALClose(hTile);
    VSIFCloseL(fp);
    VSIUnlink(osTmpFilename);

    return nRet;
}

#endif //HAVE_LIBJPEG
