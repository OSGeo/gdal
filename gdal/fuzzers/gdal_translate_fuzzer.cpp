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

#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

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
    // to avoid timeout in WMS driver
    CPLSetConfigOption("GDAL_WMS_ABORT_CURL_REQUEST", "YES");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
    CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "1");
    CPLSetConfigOption("GDAL_CACHEMAX", "1000"); // Limit to 1 GB
    GDALAllRegister();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSILFILE* fp = VSIFileFromMemBuffer( "/vsimem/test.tar",
            reinterpret_cast<GByte*>(const_cast<uint8_t*>(buf)), len, FALSE );
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);

    char** papszArgv = nullptr;

    // Prevent generating too big output raster. Make sure they are set at
    // the beginning to avoid being accidentally eaten by invalid arguments
    // afterwards.
    papszArgv = CSLAddString(papszArgv, "-limit_outsize");
    papszArgv = CSLAddString(papszArgv, "1000000");

    fp = VSIFOpenL("/vsitar//vsimem/test.tar/cmd.txt", "rb");
    if( fp != nullptr )
    {
        const char* pszLine = nullptr;
        while( (pszLine = CPLReadLineL(fp)) != nullptr )
        {
            if( !EQUAL(pszLine, "-limit_outsize") )
                papszArgv = CSLAddString(papszArgv, pszLine);
        }
        VSIFCloseL(fp);
    }

    int nXDim = -1;
    int nYDim = -1;
    bool bXDimPct = false;
    bool bYDimPct = false;
    bool bNonNearestResampling = false;
    int nBlockXSize = 0;
    int nBlockYSize = 0;
    if( papszArgv != nullptr )
    {
        int nCount = CSLCount(papszArgv);
        for( int i = 0; i < nCount; i++ )
        {
            if( EQUAL(papszArgv[i], "-outsize") && i + 2 < nCount )
            {
                nXDim = atoi(papszArgv[i+1]);
                bXDimPct = (papszArgv[i+1][0] != '\0' &&
                            papszArgv[i+1][strlen(papszArgv[i+1])-1] == '%');
                nYDim = atoi(papszArgv[i+2]);
                bYDimPct = (papszArgv[i+2][0] != '\0' &&
                            papszArgv[i+2][strlen(papszArgv[i+2])-1] == '%');
            }
            else if( EQUAL(papszArgv[i], "-r") && i + 1 < nCount )
            {
                bNonNearestResampling = !STARTS_WITH_CI(papszArgv[i+1], "NEAR");
            }
            else if( EQUAL(papszArgv[i], "-co") && i + 1 < nCount )
            {
                if( STARTS_WITH_CI(papszArgv[i+1], "BLOCKSIZE=") )
                {
                    nBlockXSize = std::max(nBlockXSize,
                                atoi(papszArgv[i+1]+strlen("BLOCKSIZE=")));
                    nBlockYSize = std::max(nBlockYSize,
                                atoi(papszArgv[i+1]+strlen("BLOCKSIZE=")));
                }
                else if( STARTS_WITH_CI(papszArgv[i+1], "BLOCKXSIZE=") )
                {
                    nBlockXSize = std::max(nBlockXSize,
                                atoi(papszArgv[i+1]+strlen("BLOCKXSIZE=")));
                }
                else if( STARTS_WITH_CI(papszArgv[i+1], "BLOCKYSIZE=") )
                {
                    nBlockYSize = std::max(nBlockYSize,
                                atoi(papszArgv[i+1]+strlen("BLOCKYSIZE=")));
                }
            }
        }
    }

    if( papszArgv != nullptr )
    {
        GDALTranslateOptions* psOptions = GDALTranslateOptionsNew(papszArgv, nullptr);
        if( psOptions )
        {
            GDALDatasetH hSrcDS = GDALOpen( "/vsitar//vsimem/test.tar/in", GA_ReadOnly );
            if( hSrcDS != nullptr )
            {
                // Also check that reading the source doesn't involve too
                // much memory
                GDALDataset* poSrcDS = reinterpret_cast<GDALDataset*>(hSrcDS);
                int nBands = poSrcDS->GetRasterCount();
                if( nBands < 10 )
                {
                    // Prevent excessive downsampling which might require huge
                    // memory allocation
                    bool bOKForResampling = true;
                    if( bNonNearestResampling && nXDim >= 0 && nYDim >= 0 )
                    {
                        if( bXDimPct && nXDim > 0 )
                        {
                            nXDim = static_cast<int>(
                                poSrcDS->GetRasterXSize() / 100.0 * nXDim);
                        }
                        if( bYDimPct && nYDim > 0 )
                        {
                            nYDim = static_cast<int>(
                                poSrcDS->GetRasterYSize() / 100.0 * nYDim);
                        }
                        if( nXDim > 0 && poSrcDS->GetRasterXSize() / nXDim > 100 )
                            bOKForResampling = false;
                        if( nYDim > 0 && poSrcDS->GetRasterYSize() / nYDim > 100 )
                            bOKForResampling = false;
                    }

                    bool bOKForSrc = true;
                    if( nBands )
                    {
                        const int nDTSize = GDALGetDataTypeSizeBytes(
                            poSrcDS->GetRasterBand(1)->GetRasterDataType() );
                        vsi_l_offset nSize =
                            static_cast<vsi_l_offset>(nBands) *
                                poSrcDS->GetRasterXSize() *
                                poSrcDS->GetRasterYSize() * nDTSize;
                        if( nSize > 10 * 1024 * 1024 )
                        {
                            bOKForSrc = false;
                        }

                        int nBXSize = 0, nBYSize = 0;
                        GDALGetBlockSize( GDALGetRasterBand(hSrcDS, 1), &nBXSize,
                                          &nBYSize );
                        const char* pszInterleave =
                            GDALGetMetadataItem( hSrcDS, "INTERLEAVE",
                                                 "IMAGE_STRUCTURE" );
                        int nSimultaneousBands =
                            (pszInterleave && EQUAL(pszInterleave, "PIXEL")) ?
                                        nBands : 1;
                        if( static_cast<GIntBig>(nSimultaneousBands)*
                                nBXSize * nBYSize * nDTSize > 10 * 1024 * 1024 )
                        {
                            bOKForSrc = false;
                        }

                        if( static_cast<GIntBig>(nBlockXSize) * nBlockYSize
                                    > 10 * 1024 * 1024 / (nBands * nDTSize) )
                        {
                            bOKForSrc = false;
                        }
                    }

                    if( bOKForSrc && bOKForResampling )
                    {
                        GDALDatasetH hOutDS = GDALTranslate("/vsimem/out", hSrcDS,
                                                            psOptions, nullptr);
                        if( hOutDS )
                            GDALClose(hOutDS);
                    }
                }
                GDALClose(hSrcDS);
            }
            GDALTranslateOptionsFree(psOptions);
        }
    }
    CSLDestroy(papszArgv);

    VSIRmdirRecursive("/vsimem/");

    CPLPopErrorHandler();

    return 0;
}
