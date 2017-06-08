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
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_alg.h"
#include "gdal_frmts.h"

#ifndef REGISTER_FUNC
#define REGISTER_FUNC GDALAllRegister
#endif

#ifndef GDAL_SKIP
#define GDAL_SKIP "CAD"
#endif

#ifndef EXTENSION
#define EXTENSION "bin"
#endif

#ifndef MEM_FILENAME
#define MEM_FILENAME "/vsimem/test"
#endif

#ifndef GDAL_FILENAME
#define GDAL_FILENAME MEM_FILENAME
#endif

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerInitialize(int* /*argc*/, char*** argv)
{
    const char* exe_path = (*argv)[0];
    CPLSetConfigOption("GDAL_DATA", CPLGetPath(exe_path));
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    CPLSetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", "YES");
    // Disable PDF text rendering as fontconfig cannot access its config files
    CPLSetConfigOption("GDAL_PDF_RENDERING_OPTIONS", "RASTER,VECTOR");
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
#ifdef USE_FILESYSTEM
    char szTempFilename[64];
    snprintf(szTempFilename, sizeof(szTempFilename),
             "/tmp/gdal_fuzzer_%d.%s",
             (int)getpid(), EXTENSION);
    VSILFILE* fp = VSIFOpenL(szTempFilename, "wb");
    if( !fp )
    {
        fprintf(stderr, "Cannot create %s\n", szTempFilename);
        return 1;
    }
    VSIFWriteL( buf, 1, len, fp );
#else
    VSILFILE* fp = VSIFileFromMemBuffer( MEM_FILENAME,
            reinterpret_cast<GByte*>(const_cast<uint8_t*>(buf)), len, FALSE );
#endif
    VSIFCloseL(fp);
#ifdef GDAL_SKIP
    CPLSetConfigOption("GDAL_SKIP", GDAL_SKIP);
#endif
    REGISTER_FUNC();
    CPLPushErrorHandler(CPLQuietErrorHandler);
#ifdef USE_FILESYSTEM
    GDALDatasetH hDS = GDALOpen( szTempFilename, GA_ReadOnly );
#else
    GDALDatasetH hDS = GDALOpen( GDAL_FILENAME, GA_ReadOnly );
#endif
    if( hDS )
    {
        const int nTotalBands = GDALGetRasterCount(hDS);
        const int nBands = std::min(10, nTotalBands);
        bool bDoCheckSum = true;
        if( nBands > 0 )
        {
            // If we know that we will need to allocate a lot of memory
            // given the block size and interleaving mode, do not read
            // pixels to avoid out of memory conditions by ASAN
            int nBlockXSize = 0, nBlockYSize = 0;
            GDALGetBlockSize( GDALGetRasterBand(hDS, 1), &nBlockXSize,
                              &nBlockYSize );
            const GDALDataType eDT =
                GDALGetRasterDataType( GDALGetRasterBand(hDS, 1) );
            const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
            const char* pszInterleave =
                GDALGetMetadataItem( hDS, "INTERLEAVE", "IMAGE_STRUCTURE" );
            const int nSimultaneousBands =
                (pszInterleave && EQUAL(pszInterleave, "PIXEL")) ?
                            nTotalBands : 1;
            if( nBlockXSize >
                10 * 1024 * 1024 / nDTSize / nBlockYSize / nSimultaneousBands )
            {
                bDoCheckSum = false;
            }
        }
        if( bDoCheckSum )
        {
            for( int i = 0; i < nBands; i++ )
            {
                GDALRasterBandH hBand = GDALGetRasterBand(hDS, i+1);
                GDALChecksumImage(hBand, 0, 0,
                                    std::min(1024, GDALGetRasterXSize(hDS)),
                                    std::min(1024, GDALGetRasterYSize(hDS)));
            }
        }

        // Test other API
        GDALGetProjectionRef(hDS);
        double adfGeoTransform[6];
        GDALGetGeoTransform(hDS, adfGeoTransform);
        CSLDestroy(GDALGetFileList(hDS));
        GDALGetGCPCount(hDS);
        GDALGetGCPs(hDS);
        GDALGetGCPProjection(hDS);
        GDALGetMetadata(hDS, NULL);

        GDALClose(hDS);
    }
    CPLPopErrorHandler();
#ifdef USE_FILESYSTEM
    VSIUnlink( szTempFilename );
#else
    VSIUnlink( MEM_FILENAME );
#endif
    return 0;
}
