/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "gdal.h"
#include "cpl_vsi.h"
#include "gdal_alg.h"
#include "gdal_frmts.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSILFILE* fp = VSIFileFromMemBuffer( "/vsimem/test",
            reinterpret_cast<GByte*>(const_cast<uint8_t*>(buf)), len, FALSE );
    VSIFCloseL(fp);
#ifdef REGISTER_FUNC
    REGISTER_FUNC();
#else
    GDALAllRegister();
#endif
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDatasetH hDS = GDALOpen( "/vsimem/test", GA_ReadOnly );
    if( hDS )
    {
        const int nBands = std::min(10, GDALGetRasterCount(hDS));
        for( int i = 0; i < nBands; i++ )
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDS, i+1);
            GDALChecksumImage(hBand, 0, 0,
                                  std::min(1024, GDALGetRasterXSize(hDS)),
                                  std::min(1024, GDALGetRasterYSize(hDS)));
        }
        GDALClose(hDS);
    }
    CPLPopErrorHandler();
    VSIUnlink( "/vsimem/test" );
    return 0;
}
