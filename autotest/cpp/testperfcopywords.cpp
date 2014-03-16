/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test performance of GDALCopyWords().
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <time.h>

#include "gdal.h"

int main(int argc, char* argv[])
{
    void* in = calloc(1, 256 * 256 * 16);
    void* out = malloc(256 * 256 * 16);
    
    int i;
    int intype, outtype;

    clock_t start, end;
    
    for(intype=GDT_Byte;intype<=GDT_CFloat64;intype++)
    {
        for(outtype=GDT_Byte;outtype<=GDT_CFloat64;outtype++)
        {
            start = clock();
            
            for(i=0;i<1000;i++)
                GDALCopyWords(in, (GDALDataType)intype, 16, out, (GDALDataType)outtype, 16, 256 * 256);
                
            end = clock();
            
            printf("%s -> %s : %.2f s\n",
                   GDALGetDataTypeName((GDALDataType)intype),
                   GDALGetDataTypeName((GDALDataType)outtype),
                   (end - start) * 1.0 / CLOCKS_PER_SEC);
        }
    }

    return 0;
}

