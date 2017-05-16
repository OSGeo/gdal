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

#include <stdlib.h>
#include <stdio.h>

int LLVMFuzzerTestOneInput(void *buf, size_t len);

int main(int argc, char* argv[])
{
    if( argc < 2 )
    {
        return LLVMFuzzerTestOneInput(" ", 1);
    }
    else
    {
        int nRet = 0;
        void* buf = NULL;
        int nLen = 0;
        FILE* f = fopen(argv[1], "rb");
        if( !f )
        {
            fprintf(stderr, "%s does not exist.\n", argv[1]);
            exit(1);
        }
        fseek(f, 0, SEEK_END);
        nLen = (int)ftell(f);
        fseek(f, 0, SEEK_SET);
        buf = malloc(nLen);
        if( !buf )
        {
            fprintf(stderr, "malloc failed.\n");
            fclose(f);
            exit(1);
        }
        fread(buf, nLen, 1, f);
        fclose(f);
        nRet = LLVMFuzzerTestOneInput(buf, nLen);
        free(buf);
        return nRet;
    }
}
