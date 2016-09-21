/******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
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
 *****************************************************************************
 *
 *  geo_simpletags.h
 *
 * Provides interface for a "simple tags io in memory" mechanism
 * as an alternative to accessing a real tiff file using libtiff.
 *
 ****************************************************************************/

#ifndef LIBGEOTIFF_GEO_SIMPLETAGS_H_
#define LIBGEOTIFF_GEO_SIMPLETAGS_H_

#include "geotiff.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define STT_SHORT   1
#define STT_DOUBLE  2
#define STT_ASCII   3

typedef struct {
    int tag;
    int count;
    int type;
    void *data;
} ST_KEY;

typedef struct {
    int key_count;
    ST_KEY *key_list;
} ST_TIFF;

typedef void *STIFF;

void CPL_DLL GTIFSetSimpleTagsMethods(TIFFMethod *method);

int CPL_DLL ST_SetKey( ST_TIFF *, int tag, int count,
                       int st_type, void *data );
int CPL_DLL ST_GetKey( ST_TIFF *, int tag, int *count,
                       int *st_type, void **data_ptr );

ST_TIFF CPL_DLL *ST_Create( void );
void CPL_DLL ST_Destroy( ST_TIFF * );

int CPL_DLL ST_TagType( int tag );

#if defined(__cplusplus)
}
#endif

#endif /* LIBGEOTIFF_GEO_SIMPLETAGS_H_ */
