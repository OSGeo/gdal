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
#include <string.h>

extern "C"
{
int LLVMFuzzerTestOneInput(const void *buf, size_t len);
int LLVMFuzzerInitialize(int* argc, char*** argv);
}

template<class T> static void CPL_IGNORE_RET_VAL(T) {}

static void Usage(int, char* argv[])
{
    fprintf(stderr, "%s [--help] [-repeat N] filename.\n", argv[0]);
    exit(1);
}

int main(int argc, char* argv[])
{
    LLVMFuzzerInitialize(&argc, &argv);

    int nRet = 0;
    void* buf = nullptr;
    int nLen = 0;
    int nLoops = 1;
    const char* pszFilename = nullptr;
    for(int i = 1; i < argc; i++ )
    {
        if( i + 1 < argc && strcmp(argv[i], "-repeat") == 0 )
        {
            nLoops = atoi(argv[i+1]);
            i++;
        }
        else if( strcmp(argv[i], "-dummy") == 0 )
        {
            return LLVMFuzzerTestOneInput(" ", 1);
        }
        else if( strcmp(argv[i], "--help") == 0 )
        {
            Usage(argc, argv);
        }
        else if( argv[i][0] == '-' )
        {
            fprintf(stderr, "Unrecognized option: %s", argv[i]);
            Usage(argc, argv);
        }
        else
        {
            pszFilename = argv[i];
        }
    }
    if( pszFilename == nullptr )
    {
        fprintf(stderr, "No filename specified\n");
        Usage(argc, argv);
    }
    FILE* f = fopen(pszFilename, "rb");
    if( !f )
    {
        fprintf(stderr, "%s does not exist.\n", pszFilename);
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
    CPL_IGNORE_RET_VAL(fread(buf, nLen, 1, f));
    fclose(f);
    for( int i = 0; i < nLoops; i++ )
    {
        nRet = LLVMFuzzerTestOneInput(buf, nLen);
        if( nRet != 0 )
            break;
    }
    free(buf);
    return nRet;
}
