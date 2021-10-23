/******************************************************************************
 * $Id$
 *
 * Project:  FIT Driver
 * Purpose:  Implement FIT Support - not using the SGI iflFIT library.
 * Author:   Philip Nemec, nemec@keyholecorp.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Keyhole, Inc.
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

#ifndef FIT_H_
#define FIT_H_

#include "gdal.h"

struct FITinfo {
    unsigned short magic;       // file ident
    unsigned short version;     // file version
    unsigned int xSize;         // image size
    unsigned int ySize;
    unsigned int zSize;
    unsigned int cSize;
    int dtype;                  // data type
    int order;                  // RGBRGB.. or RR..GG..BB..
    int space;                  // coordinate space
    int cm;                     // color model
    unsigned int xPageSize;     // page size
    unsigned int yPageSize;
    unsigned int zPageSize;
    unsigned int cPageSize;
                                // NOTE: a word of padding is inserted here
                                //       due to struct alignment rules
    double minValue;            // min/max pixel values
    double maxValue;
    unsigned int dataOffset;    // offset to first page of data

    // non-header values
    unsigned int userOffset;    // offset to area of user data
};

struct FIThead02 {              // file header for version 02
    unsigned short magic;       // file ident
    unsigned short version;     // file version
    unsigned int xSize;         // image size
    unsigned int ySize;
    unsigned int zSize;
    unsigned int cSize;
    int dtype;                  // data type
    int order;                  // RGBRGB.. or RR..GG..BB..
    int space;                  // coordinate space
    int cm;                     // color model
    unsigned int xPageSize;     // page size
    unsigned int yPageSize;
    unsigned int zPageSize;
    unsigned int cPageSize;
    short _padding;             // NOTE: a word of padding is inserted here
                                //       due to struct alignment rules
    double minValue;            // min/max pixel values
    double maxValue;
    unsigned int dataOffset;    // offset to first page of data
    // user extensible area...
};

struct FIThead01 {              // file header for version 01
    unsigned short magic;       // file ident
    unsigned short version;     // file version
    unsigned int xSize;         // image size
    unsigned int ySize;
    unsigned int zSize;
    unsigned int cSize;
    int dtype;                  // data type
    int order;                  // RGBRGB.. or RR..GG..BB..
    int space;                  // coordinate space
    int cm;                     // color model
    unsigned int xPageSize;     // page size
    unsigned int yPageSize;
    unsigned int zPageSize;
    unsigned int cPageSize;
    unsigned int dataOffset;   // offset to first page of data
    // user extensible area...
};

#ifdef __cplusplus
extern "C" {
#endif

GDALDataType fitDataType(int dtype);
int fitGetDataType(GDALDataType eDataType);
int fitGetColorModel(GDALColorInterp colorInterp, int nBands);

#ifdef __cplusplus
}
#endif

#endif // FIT_H_
