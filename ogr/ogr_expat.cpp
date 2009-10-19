/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Convenience function for parsing with Expat library
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2009, Even Rouault
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

#ifdef HAVE_EXPAT

#include "ogr_expat.h"
#include "cpl_error.h"

CPL_CVSID("$Id$");

#define OGR_EXPAT_MAX_ALLOWED_ALLOC 10000000

static void* OGRExpatMalloc(size_t size)
{
    if (size < OGR_EXPAT_MAX_ALLOWED_ALLOC)
        return malloc(size);
    else
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Expat tried to malloc %d bytes. File probably corrupted", (int)size);
        return NULL;
    }
}

static void* OGRExpatRealloc(void *ptr, size_t size)
{
    if (size < OGR_EXPAT_MAX_ALLOWED_ALLOC)
        return realloc(ptr, size);
    else
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Expat tried to realloc %d bytes. File probably corrupted", (int)size);
        free(ptr);
        return NULL;
    }
}

XML_Parser OGRCreateExpatXMLParser()
{
    XML_Memory_Handling_Suite memsuite;
    memsuite.malloc_fcn = OGRExpatMalloc;
    memsuite.realloc_fcn = OGRExpatRealloc;
    memsuite.free_fcn = free;
    return XML_ParserCreate_MM(NULL, &memsuite, NULL);
}

#endif
