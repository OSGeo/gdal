/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even.rouault at spatialys.com>
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
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "gdal_utils.h"

#ifndef REGISTER_FUNC
#define REGISTER_FUNC OGRRegisterAll
#endif

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerInitialize(int* /*argc*/, char*** argv)
{
    const char* exe_path = (*argv)[0];
    if( CPLGetConfigOption("GDAL_DATA", nullptr) == nullptr )
    {
        CPLSetConfigOption("GDAL_DATA", CPLGetPath(exe_path));
    }
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    CPLSetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", "YES");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
    CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "1");
#ifdef OGR_SKIP
    CPLSetConfigOption("OGR_SKIP", OGR_SKIP);
#endif
    REGISTER_FUNC();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSILFILE* fp = VSIFileFromMemBuffer( "/vsimem/test.tar",
            reinterpret_cast<GByte*>(const_cast<uint8_t*>(buf)), len, FALSE );
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);

    char** papszArgv = nullptr;

    CPLString osOutFilename("out");
    fp = VSIFOpenL("/vsitar//vsimem/test.tar/cmd.txt", "rb");
    if( fp != nullptr )
    {
        const char* pszLine = nullptr;
        if( (pszLine = CPLReadLineL(fp)) != nullptr )
        {
            osOutFilename = pszLine;
            osOutFilename = osOutFilename.replaceAll('/', '_');
        }
        int nCandidateLayerNames = 0;
        while( (pszLine = CPLReadLineL(fp)) != nullptr )
        {
            if( pszLine[0] != '-' )
            {
                nCandidateLayerNames ++;
                if( nCandidateLayerNames == 10 )
                    break;
            }
            papszArgv = CSLAddString(papszArgv, pszLine);
        }
        VSIFCloseL(fp);
    }

    char** papszDrivers = CSLAddString(nullptr, "CSV");
    GDALDatasetH hSrcDS = GDALOpenEx( "/vsitar//vsimem/test.tar/in",
                        GDAL_OF_VECTOR, papszDrivers, nullptr, nullptr );
    CSLDestroy(papszDrivers);

    if( papszArgv != nullptr && hSrcDS != nullptr )
    {
        const int nLayerCount = GDALDatasetGetLayerCount(hSrcDS);
        for( int i = 0; i < nLayerCount; i++ )
        {
            OGRLayerH hLayer = GDALDatasetGetLayer(hSrcDS, i);
            if( hLayer )
            {
                int nFieldCount = OGR_FD_GetFieldCount(
                    OGR_L_GetLayerDefn(hLayer));
                if( nFieldCount > 100 )
                {
                    papszArgv = CSLAddString(papszArgv, "-limit");
                    papszArgv = CSLAddString(papszArgv, "100");
                    break;
                }
            }
        }

        GDALVectorTranslateOptions* psOptions =
            GDALVectorTranslateOptionsNew(papszArgv, nullptr);
        if( psOptions )
        {
            CPLString osFullOutFilename("/vsimem/" + osOutFilename);
            GDALDatasetH hOutDS = GDALVectorTranslate(
                osFullOutFilename.c_str(),
                nullptr, 1, &hSrcDS, psOptions, nullptr);
            if( hOutDS )
            {
                GDALDriverH hOutDrv = GDALGetDatasetDriver(hOutDS);
                GDALClose(hOutDS);

                // Try re-opening generated file
                GDALClose(
                    GDALOpenEx(osFullOutFilename, GDAL_OF_VECTOR,
                            nullptr, nullptr, nullptr));

                if( hOutDrv )
                    GDALDeleteDataset(hOutDrv, osFullOutFilename);
            }
            GDALVectorTranslateOptionsFree(psOptions);
        }
    }
    CSLDestroy(papszArgv);
    GDALClose(hSrcDS);

    VSIRmdirRecursive("/vsimem/");

    CPLPopErrorHandler();

    return 0;
}
