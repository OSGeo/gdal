/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Include tiledb headers
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2019, TileDB, Inc
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

#ifndef TILEDB_HEADERS_H
#define TILEDB_HEADERS_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) /* XXXX was deprecated */
#endif

#include "tiledb/tiledb"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef enum
{
    BAND = 0,
    PIXEL = 1,
    ATTRIBUTES = 2
} TILEDB_INTERLEAVE_MODE;

#endif  // TILEDB_HEADERS_H
