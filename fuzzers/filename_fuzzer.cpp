/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_priv.h"

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerInitialize(int * /*argc*/, char ***argv)
{
    const char *exe_path = (*argv)[0];
    if (CPLGetConfigOption("GDAL_DATA", nullptr) == nullptr)
    {
        CPLSetConfigOption("GDAL_DATA", CPLGetPath(exe_path));
    }
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    CPLSetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", "YES");
    // Disable PDF text rendering as fontconfig cannot access its config files
    CPLSetConfigOption("GDAL_PDF_RENDERING_OPTIONS", "RASTER,VECTOR");
    // to avoid timeout in WMS driver
    CPLSetConfigOption("GDAL_WMS_ABORT_CURL_REQUEST", "YES");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
    CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "1");
    CPLSetConfigOption("GDAL_CACHEMAX", "1000");  // Limit to 1 GB
#ifdef GTIFF_USE_MMAP
    CPLSetConfigOption("GTIFF_USE_MMAP", "YES");
#endif

#ifdef GDAL_SKIP
    CPLSetConfigOption("GDAL_SKIP", GDAL_SKIP);
#endif
    GDALAllRegister();

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSIFCloseL(VSIFileFromMemBuffer(
        "/vsimem/input.tar",
        reinterpret_cast<GByte *>(const_cast<uint8_t *>(buf)), len, FALSE));

    CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);

    GByte *paby = nullptr;
    vsi_l_offset nSize = 0;
    if (!VSIIngestFile(nullptr, "/vsitar//vsimem/input.tar/filename", &paby,
                       &nSize, -1))
    {
        VSIUnlink("/vsimem/input.tar");
        return 0;
    }
    const std::string osFilename(reinterpret_cast<const char *>(paby));
    VSIFree(paby);

    paby = nullptr;
    nSize = 0;
    int ret = VSIIngestFile(nullptr, "/vsitar//vsimem/input.tar/content", &paby,
                            &nSize, -1);
    VSIUnlink("/vsimem/input.tar");
    if (!ret)
    {
        return 0;
    }

    const std::string osRealFilename("/vsimem/" + osFilename);
    VSIFCloseL(VSIFileFromMemBuffer(osRealFilename.c_str(), paby,
                                    static_cast<size_t>(nSize), TRUE));

    delete GDALDataset::Open(osRealFilename.c_str());

    VSIUnlink(osRealFilename.c_str());

    return 0;
}
