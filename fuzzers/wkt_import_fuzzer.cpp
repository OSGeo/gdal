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

#include "ogr_api.h"
#include "cpl_conv.h"
#include "cpl_error.h"

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);

int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/)
{
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    OGRGeometryH hGeom = nullptr;
    char* pszWKT = static_cast<char*>(CPLMalloc( len + 1 ));
    memcpy(pszWKT, buf, len);
    pszWKT[len] = '\0';
    char* pszWKTParam = pszWKT;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGR_G_CreateFromWkt( &pszWKTParam, nullptr, &hGeom );
    CPLPopErrorHandler();
    CPLFree(pszWKT);
    OGR_G_DestroyGeometry(hGeom);
    return 0;
}
