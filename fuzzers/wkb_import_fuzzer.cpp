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
#include "cpl_string.h"

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);

int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/)
{
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    OGRGeometryH hGeom = nullptr;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGR_G_CreateFromWkb( const_cast<unsigned char*>(buf), nullptr, &hGeom,
                         static_cast<int>(len) );
    if( hGeom )
    {
        const int nWKBSize = OGR_G_WkbSize(hGeom);
        if( nWKBSize )
        {
            GByte* pabyWKB = new GByte[nWKBSize];
            OGR_G_ExportToWkb(hGeom, wkbNDR, pabyWKB);
            OGR_G_ExportToIsoWkb(hGeom, wkbNDR, pabyWKB);
            delete[] pabyWKB;
        }

        char* pszWKT = nullptr;
        OGR_G_ExportToWkt(hGeom, &pszWKT);
        CPLFree(pszWKT);

        pszWKT = nullptr;
        OGR_G_ExportToIsoWkt(hGeom, &pszWKT);
        CPLFree(pszWKT);

        CPLFree(OGR_G_ExportToGML(hGeom));

        char** papszOptions = CSLSetNameValue(nullptr, "FORMAT", "GML3");
        CPLFree(OGR_G_ExportToGMLEx(hGeom, papszOptions));
        CSLDestroy(papszOptions);

        CPLDestroyXMLNode(OGR_G_ExportEnvelopeToGMLTree(hGeom));

        CPLFree(OGR_G_ExportToKML(hGeom, nullptr));

        CPLFree(OGR_G_ExportToJson(hGeom));
    }
    OGR_G_DestroyGeometry(hGeom);
    CPLPopErrorHandler();
    return 0;
}
