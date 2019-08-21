/******************************************************************************
 * $Id$
 *
 * Name:     cplvirtualmem.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault
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
 *****************************************************************************/

//************************************************************************/
//
// Define the extensions for CPLVirtualMem (nee CPLVirtualMemShadow)
//
//************************************************************************/

#if defined(SWIGPYTHON)
%{
#include "gdal.h"

typedef struct
{
    CPLVirtualMem *vmem;
    int            bAuto;
    GDALDataType   eBufType;
    int            bIsBandSequential;
    int            bReadOnly;
    int            nBufXSize;
    int            nBufYSize;
    int            nBandCount;
    GDALTileOrganization eTileOrganization;
    int                  nTileXSize;
    int                  nTileYSize;
    int            nPixelSpace; /* if bAuto == TRUE */
    GIntBig        nLineSpace; /* if bAuto == TRUE */
} CPLVirtualMemShadow;

%}

%rename (VirtualMem) CPLVirtualMemShadow;

class CPLVirtualMemShadow {
private:
  CPLVirtualMemShadow();
public:
%extend {
    ~CPLVirtualMemShadow()
    {
        CPLVirtualMemFree( self->vmem );
        free(self);
    }

    void GetAddr(void** pptr, size_t* pnsize, GDALDataType* pdatatype, int* preadonly)
    {
        *pptr = CPLVirtualMemGetAddr( self->vmem );
        *pnsize = CPLVirtualMemGetSize( self->vmem );
        *pdatatype = self->eBufType;
        *preadonly = self->bReadOnly;
    }

    void Pin(size_t start_offset = 0, size_t nsize = 0, int bWriteOp = 0 )
    {
        if( nsize == 0 || start_offset + nsize >= CPLVirtualMemGetSize( self->vmem ) )
            nsize = CPLVirtualMemGetSize( self->vmem ) - start_offset;
        char* start_addr = (char*)CPLVirtualMemGetAddr( self->vmem ) + start_offset;
        CPLVirtualMemPin(self->vmem, start_addr, nsize, bWriteOp);
    }
} /* extend */
}; /* CPLVirtualMemShadow */
#endif
