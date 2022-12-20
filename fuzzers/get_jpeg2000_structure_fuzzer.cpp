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

#include "gdal.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_frmts.h"

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv);

int LLVMFuzzerInitialize(int * /*argc*/, char *** /*argv*/)
{
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

#define MEM_FILENAME "/vsimem/test"

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    GDALRegister_GTiff();
    GDALRegister_VRT();
    VSILFILE *fp = VSIFileFromMemBuffer(
        MEM_FILENAME, reinterpret_cast<GByte *>(const_cast<uint8_t *>(buf)),
        len, FALSE);
    VSIFCloseL(fp);
    char **papszOptions = CSLSetNameValue(nullptr, "ALL", "YES");
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLXMLNode *psNode = GDALGetJPEG2000Structure(MEM_FILENAME, papszOptions);
    CPLPopErrorHandler();
    CSLDestroy(papszOptions);
    if (psNode)
        CPLDestroyXMLNode(psNode);
    VSIUnlink(MEM_FILENAME);
    return 0;
}
