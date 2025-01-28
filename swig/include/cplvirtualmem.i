/******************************************************************************
 *
 * Name:     cplvirtualmem.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault
 *
 * SPDX-License-Identifier: MIT
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
