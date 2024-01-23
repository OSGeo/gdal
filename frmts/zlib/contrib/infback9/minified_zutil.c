/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2017 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "minified_zutil.h"

#include <stdlib.h>

voidpf ZLIB_INTERNAL zcalloc(voidpf opaque, unsigned items, unsigned size)
{
    (void)opaque;
    return sizeof(uInt) > 2 ? (voidpf)malloc((size_t)items * size) :
                              (voidpf)calloc(items, size);
}

void ZLIB_INTERNAL zcfree(voidpf opaque, voidpf ptr)
{
    (void)opaque;
    free(ptr);
}
