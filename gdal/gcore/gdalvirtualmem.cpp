/**********************************************************************
 * $Id$
 *
 * Name:     gdalvirtualmem.cpp
 * Project:  GDAL
 * Purpose:  Dataset and rasterband exposed as a virtual memory mapping.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 **********************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_virtualmem.h"

/************************************************************************/
/*                            GDALVirtualMem                            */
/************************************************************************/

class GDALVirtualMem
{
    GDALDatasetH hDS;
    GDALRasterBandH hBand;
    int nXOff;
    int nYOff;
    /*int nXSize;
    int nYSize;*/
    int nBufXSize;
    int nBufYSize;
    GDALDataType eBufType;
    int nBandCount;
    int* panBandMap;
    int nPixelSpace;
    GIntBig nLineSpace;
    GIntBig nBandSpace;

    int     bIsCompact;
    int     bIsBandSequential;

    int  IsCompact() const { return bIsCompact; }
    int  IsBandSequential() const { return bIsBandSequential; }

    void GetXYBand( size_t nOffset, int& x, int& y, int& band ) const;
    size_t GetOffset(int x, int y, int band) const;
    int  GotoNextPixel(int& x, int& y, int& band) const;

    void DoIOBandSequential( GDALRWFlag eRWFlag, size_t nOffset,
                              void* pPage, size_t nBytes ) const;
    void DoIOPixelInterleaved( GDALRWFlag eRWFlag, size_t nOffset,
                               void* pPage, size_t nBytes ) const;

public:
             GDALVirtualMem( GDALDatasetH hDS,
                             GDALRasterBandH hBand,
                          int nXOff, int nYOff,
                          int nXSize, int nYSize,
                          int nBufXSize, int nBufYSize,
                          GDALDataType eBufType,
                          int nBandCount, const int* panBandMapIn,
                          int nPixelSpace,
                          GIntBig nLineSpace,
                          GIntBig nBandSpace );
            ~GDALVirtualMem();

    static void FillCacheBandSequential(CPLVirtualMem* ctxt,  size_t nOffset,
                                         void* pPageToFill,
                                         size_t nToFill, void* pUserData);
    static void SaveFromCacheBandSequential(CPLVirtualMem* ctxt,  size_t nOffset,
                                             const void* pPageToBeEvicted,
                                             size_t nToEvicted, void* pUserData);

    static void FillCachePixelInterleaved(CPLVirtualMem* ctxt,  size_t nOffset,
                                          void* pPageToFill,
                                          size_t nToFill, void* pUserData);
    static void SaveFromCachePixelInterleaved(CPLVirtualMem* ctxt,  size_t nOffset,
                                              const void* pPageToBeEvicted,
                                              size_t nToEvicted, void* pUserData);

    static void Destroy(void* pUserData);
};

/************************************************************************/
/*                             GDALVirtualMem()                         */
/************************************************************************/

GDALVirtualMem::GDALVirtualMem( GDALDatasetH hDS,
                                GDALRasterBandH hBand,
                          int nXOff, int nYOff,
                          int nXSize, int nYSize,
                          int nBufXSize, int nBufYSize,
                          GDALDataType eBufType,
                          int nBandCount, const int* panBandMapIn,
                          int nPixelSpace,
                          GIntBig nLineSpace,
                          GIntBig nBandSpace ) :
    hDS(hDS), hBand(hBand), nXOff(nXOff), nYOff(nYOff), /*nXSize(nXSize), nYSize(nYSize),*/
    nBufXSize(nBufXSize), nBufYSize(nBufYSize), eBufType(eBufType),
    nBandCount(nBandCount), nPixelSpace(nPixelSpace), nLineSpace(nLineSpace),
    nBandSpace(nBandSpace)
{
    if( hDS != NULL )
    {
        if( panBandMapIn )
        {
            panBandMap = (int*) CPLMalloc(nBandCount * sizeof(int));
            memcpy(panBandMap, panBandMapIn, nBandCount * sizeof(int));
        }
        else
        {
            panBandMap = (int*) CPLMalloc(nBandCount * sizeof(int));
            for(int i=0;i<nBandCount;i++)
                panBandMap[i] = i + 1;
        }
    }
    else
    {
        panBandMap = NULL;
        nBandCount = 1;
    }

    int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
    if( nPixelSpace == nDataTypeSize &&
        nLineSpace == (GIntBig)nBufXSize * nPixelSpace &&
        nBandSpace == nBufYSize * nLineSpace )
        bIsCompact = TRUE;
    else if( nBandSpace == nDataTypeSize &&
            nPixelSpace == nBandCount * nBandSpace &&
            nLineSpace == (GIntBig)nBufXSize * nPixelSpace )
        bIsCompact = TRUE;
    else
        bIsCompact = FALSE;

    bIsBandSequential = ( nBandSpace >= nBufYSize * nLineSpace );
}

/************************************************************************/
/*                            ~GDALVirtualMem()                         */
/************************************************************************/

GDALVirtualMem::~GDALVirtualMem()
{
    CPLFree(panBandMap);
}

/************************************************************************/
/*                              GetXYBand()                             */
/************************************************************************/

void GDALVirtualMem::GetXYBand( size_t nOffset, int& x, int& y, int& band ) const
{
    if( IsBandSequential() )
    {
        if( nBandCount == 1 )
            band = 0;
        else
            band = nOffset / nBandSpace;
        y = (nOffset - band * nBandSpace) / nLineSpace;
        x = (nOffset - band * nBandSpace - y * nLineSpace) / nPixelSpace;
    }
    else
    {
        y = nOffset / nLineSpace;
        x = (nOffset - y * nLineSpace) / nPixelSpace;
        if( nBandCount == 1 )
            band = 0;
        else
            band = (nOffset - y * nLineSpace - x * nPixelSpace) / nBandSpace;
    }
}

/************************************************************************/
/*                            GotoNextPixel()                           */
/************************************************************************/

int GDALVirtualMem::GotoNextPixel(int& x, int& y, int& band) const
{
    if( IsBandSequential() )
    {
        x++;
        if( x == nBufXSize )
        {
            x = 0;
            y ++;
        }
        if( y == nBufYSize )
        {
            y = 0;
            band ++;
            if (band == nBandCount)
                return FALSE;
        }
    }
    else
    {
        band ++;
        if( band == nBandCount )
        {
            band = 0;
            x ++;
        }
        if( x == nBufXSize )
        {
            x = 0;
            y ++;
            if(y == nBufYSize)
                return FALSE;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                              GetOffset()                             */
/************************************************************************/

size_t GDALVirtualMem::GetOffset(int x, int y, int band) const
{
    return x * nPixelSpace + y * nLineSpace + band * nBandSpace;
}

/************************************************************************/
/*                          DoIOPixelInterleaved()                      */
/************************************************************************/

void GDALVirtualMem::DoIOPixelInterleaved( GDALRWFlag eRWFlag,
                        const size_t nOffset, void* pPage, size_t nBytes ) const
{
    int x, y, band;

    GetXYBand(nOffset, x, y, band);
    /*fprintf(stderr, "eRWFlag=%d, nOffset=%d, x=%d, y=%d, band=%d\n",
            eRWFlag, (int)nOffset, x, y, band);*/

    if( eRWFlag == GF_Read && !IsCompact() )
        memset(pPage, 0, nBytes);

    if( band >= nBandCount )
    {
        band = nBandCount - 1;
        if( !GotoNextPixel(x, y, band) )
            return;
    }
    else if( x >= nBufXSize )
    {
        x = nBufXSize - 1;
        band = nBandCount - 1;
        if( !GotoNextPixel(x, y, band) )
            return;
    }

    size_t nOffsetRecompute = GetOffset(x, y, band);
    CPLAssert(nOffsetRecompute >= nOffset);
    size_t nOffsetShift = nOffsetRecompute - nOffset;
    if( nOffsetShift >= nBytes )
        return;

    // If we don't start at the first band for that given pixel, load/store
    // the remaining bands
    if( band > 0 )
    {
        size_t nEndOffsetEndOfPixel = GetOffset(x, y, nBandCount);
        int bandEnd;
        // Check that we have enough space to load/store until last band
        // Should be always OK unless the number of bands is really huge
        if( nEndOffsetEndOfPixel - nOffset > nBytes )
        {
            // Not enough space: find last possible band
            int xEnd, yEnd;
            GetXYBand(nOffset + nBytes, xEnd, yEnd, bandEnd);
            CPLAssert(x == xEnd);
            CPLAssert(y == yEnd);
        }
        else
            bandEnd = nBandCount;

        // Finish reading/writing the remaining bands for that pixel
        GDALDatasetRasterIO( hDS, eRWFlag,
                            nXOff + x, nYOff + y, 1, 1,
                            (char*)pPage + nOffsetShift,
                            1, 1, eBufType,
                            bandEnd - band, panBandMap + band,
                            nPixelSpace, nLineSpace, nBandSpace );

        if( bandEnd < nBandCount )
            return;

        band = nBandCount - 1;
        if( !GotoNextPixel(x, y, band) )
            return;
        nOffsetRecompute = GetOffset(x, y, 0);
        nOffsetShift = nOffsetRecompute - nOffset;
        if( nOffsetShift >= nBytes )
            return;
    }

    // Is there enough place to store/load up to the end of current line ?
    size_t nEndOffsetEndOfLine = GetOffset(nBufXSize-1, y, nBandCount);
    if( nEndOffsetEndOfLine - nOffset > nBytes )
    {
        // No : read/write as many pixels on this line as possible
        int xEnd, yEnd, bandEnd;
        GetXYBand(nOffset + nBytes, xEnd, yEnd, bandEnd);
        CPLAssert(y == yEnd);

        if( x < xEnd )
        {
            GDALDatasetRasterIO( hDS, eRWFlag,
                                nXOff + x, nYOff + y, xEnd - x, 1,
                                (char*) pPage + nOffsetShift,
                                xEnd - x, 1, eBufType,
                                nBandCount, panBandMap,
                                nPixelSpace, nLineSpace, nBandSpace );
        }

        // Are there partial bands to read/write for the last pixel ?
        if( bandEnd > 0 )
        {
            x = xEnd;
            nOffsetRecompute = GetOffset(x, y, 0);
            nOffsetShift = nOffsetRecompute - nOffset;
            if( nOffsetShift >= nBytes )
                return;

            if( bandEnd >= nBandCount )
                bandEnd = nBandCount;

            GDALDatasetRasterIO( hDS, eRWFlag,
                                nXOff + x, nYOff + y, 1, 1,
                                (char*) pPage + nOffsetShift,
                                1, 1, eBufType,
                                bandEnd, panBandMap,
                                nPixelSpace, nLineSpace, nBandSpace );
        }

        return;
    }

    // Yes, enough place to read/write until end of line
    if( x > 0 || nBytes - nOffsetShift < (size_t)nLineSpace )
    {
        GDALDatasetRasterIO( hDS, eRWFlag,
                    nXOff + x, nYOff + y, nBufXSize - x, 1,
                    (char*)pPage + nOffsetShift,
                    nBufXSize - x, 1, eBufType,
                    nBandCount, panBandMap,
                    nPixelSpace, nLineSpace, nBandSpace );

        // Go to beginning of next line
        x = nBufXSize - 1;
        band = nBandCount - 1;
        if( !GotoNextPixel(x, y, band) )
            return;
        nOffsetRecompute = GetOffset(x, y, 0);
        nOffsetShift = nOffsetRecompute - nOffset;
        if( nOffsetShift >= nBytes )
            return;
    }

    // How many whole lines can we store/load ?
    int nLineCount = (nBytes - nOffsetShift) / nLineSpace;
    if( y + nLineCount > nBufYSize )
        nLineCount = nBufYSize - y;
    if( nLineCount > 0 )
    {
        GDALDatasetRasterIO( hDS, eRWFlag,
                             nXOff + 0, nYOff + y, nBufXSize, nLineCount,
                             (GByte*) pPage + nOffsetShift,
                             nBufXSize, nLineCount, eBufType,
                             nBandCount, panBandMap,
                             nPixelSpace, nLineSpace, nBandSpace );
        
        y += nLineCount;
        if( y == nBufYSize )
            return;
        nOffsetRecompute = GetOffset(x, y, 0);
        nOffsetShift = nOffsetRecompute - nOffset;
    }

    if( nOffsetShift < nBytes )
    {
        DoIOPixelInterleaved( eRWFlag, nOffsetRecompute, 
               (char*) pPage + nOffsetShift, nBytes - nOffsetShift );
    }
}

/************************************************************************/
/*                          DoIOPixelInterleaved()                      */
/************************************************************************/

void GDALVirtualMem::DoIOBandSequential( GDALRWFlag eRWFlag,
                        const size_t nOffset, void* pPage, size_t nBytes ) const
{
    int x, y, band;

    GetXYBand(nOffset, x, y, band);
    /*fprintf(stderr, "eRWFlag=%d, nOffset=%d, x=%d, y=%d, band=%d\n",
            eRWFlag, (int)nOffset, x, y, band);*/

    if( eRWFlag == GF_Read && !IsCompact() )
        memset(pPage, 0, nBytes);

    if( x >= nBufXSize )
    {
        x = nBufXSize - 1;
        if( !GotoNextPixel(x, y, band) )
            return;
    }
    else if( y >= nBufYSize )
    {
        x = nBufXSize - 1;
        y = nBufYSize - 1;
        if( !GotoNextPixel(x, y, band) )
            return;
    }

    size_t nOffsetRecompute = GetOffset(x, y, band);
    CPLAssert(nOffsetRecompute >= nOffset);
    size_t nOffsetShift = nOffsetRecompute - nOffset;
    if( nOffsetShift >= nBytes )
        return;

    // Is there enough place to store/load up to the end of current line ?
    size_t nEndOffsetEndOfLine = GetOffset(nBufXSize, y, band);
    if( nEndOffsetEndOfLine - nOffset > nBytes )
    {
        // No : read/write as many pixels on this line as possible
        int xEnd, yEnd, bandEnd;
        GetXYBand(nOffset + nBytes, xEnd, yEnd, bandEnd);
        CPLAssert(y == yEnd);
        CPLAssert(band == bandEnd);
        GDALRasterIO( (hBand) ? hBand : GDALGetRasterBand(hDS, panBandMap[band]), eRWFlag,
                      nXOff + x, nYOff + y, xEnd - x, 1,
                      (char*)pPage + nOffsetShift,
                      xEnd - x, 1, eBufType,
                      nPixelSpace, nLineSpace );

        return;
    }

    // Yes, enough place to read/write until end of line
    if( x > 0 || nBytes - nOffsetShift < (size_t)nLineSpace )
    {
        GDALRasterIO( (hBand) ? hBand : GDALGetRasterBand(hDS, panBandMap[band]), eRWFlag,
                    nXOff + x, nYOff + y, nBufXSize - x, 1,
                    (char*)pPage + nOffsetShift,
                    nBufXSize - x, 1, eBufType,
                    nPixelSpace, nLineSpace );

        // Go to beginning of next line
        x = nBufXSize - 1;
        if( !GotoNextPixel(x, y, band) )
            return;
        nOffsetRecompute = GetOffset(x, y, band);
        nOffsetShift = nOffsetRecompute - nOffset;
        if( nOffsetShift >= nBytes )
            return;
    }

    // How many whole lines can we store/load ?
    int nLineCount = (nBytes - nOffsetShift) / nLineSpace;
    if( y + nLineCount > nBufYSize )
        nLineCount = nBufYSize - y;
    if( nLineCount > 0 )
    {
        GDALRasterIO( (hBand) ? hBand : GDALGetRasterBand(hDS, panBandMap[band]), eRWFlag,
                    nXOff + 0, nYOff + y, nBufXSize, nLineCount,
                    (GByte*) pPage + nOffsetShift,
                    nBufXSize, nLineCount, eBufType,
                    nPixelSpace, nLineSpace );

        y += nLineCount;
        if( y == nBufYSize )
        {
            y = 0;
            band ++;
            if( band == nBandCount )
                return;
        }
        nOffsetRecompute = GetOffset(x, y, band);
        nOffsetShift = nOffsetRecompute - nOffset;
    }

    if( nOffsetShift < nBytes )
    {
        DoIOBandSequential( eRWFlag, nOffsetRecompute, 
               (char*) pPage + nOffsetShift, nBytes - nOffsetShift );
    }
}

/************************************************************************/
/*                    FillCacheBandSequential()                        */
/************************************************************************/

void GDALVirtualMem::FillCacheBandSequential(CPLVirtualMem* ctxt, 
                  size_t nOffset,
                  void* pPageToFill,
                  size_t nToFill,
                  void* pUserData)
{
    const GDALVirtualMem* psParms = (const GDALVirtualMem* )pUserData;
    (void)ctxt;
    psParms->DoIOBandSequential(GF_Read, nOffset, pPageToFill, nToFill);
}

/************************************************************************/
/*                    SaveFromCacheBandSequential()                    */
/************************************************************************/

void GDALVirtualMem::SaveFromCacheBandSequential(CPLVirtualMem* ctxt, 
                  size_t nOffset,
                  const void* pPageToBeEvicted,
                  size_t nToEvicted,
                  void* pUserData)
{
    const GDALVirtualMem* psParms = (const GDALVirtualMem* )pUserData;
    (void)ctxt;
    psParms->DoIOBandSequential(GF_Write, nOffset, (void*)pPageToBeEvicted, nToEvicted);
}

/************************************************************************/
/*                     FillCachePixelInterleaved()                      */
/************************************************************************/

void GDALVirtualMem::FillCachePixelInterleaved(CPLVirtualMem* ctxt, 
                  size_t nOffset,
                  void* pPageToFill,
                  size_t nToFill,
                  void* pUserData)
{
    const GDALVirtualMem* psParms = (const GDALVirtualMem* )pUserData;
    (void)ctxt;
    psParms->DoIOPixelInterleaved(GF_Read, nOffset, pPageToFill, nToFill);
}

/************************************************************************/
/*                     SaveFromCachePixelInterleaved()                  */
/************************************************************************/

void GDALVirtualMem::SaveFromCachePixelInterleaved(CPLVirtualMem* ctxt, 
                  size_t nOffset,
                  const void* pPageToBeEvicted,
                  size_t nToEvicted,
                  void* pUserData)
{
    const GDALVirtualMem* psParms = (const GDALVirtualMem* )pUserData;
    (void)ctxt;
    psParms->DoIOPixelInterleaved(GF_Write, nOffset, (void*)pPageToBeEvicted, nToEvicted);
}

/************************************************************************/
/*                                Destroy()                             */
/************************************************************************/

void GDALVirtualMem::Destroy(void* pUserData)
{
    GDALVirtualMem* psParams = (GDALVirtualMem*) pUserData;
    delete psParams;
}

/************************************************************************/
/*                      GDALCheckBandParameters()                       */
/************************************************************************/

static int GDALCheckBandParameters( GDALDatasetH hDS,
                                    int nBandCount, int* panBandMap )
{
    if( nBandCount == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "nBandCount == 0");
        return FALSE;
    }

    if( panBandMap != NULL )
    {
        for(int i=0;i<nBandCount;i++)
        {
            if( panBandMap[i] < 1 || panBandMap[i] > GDALGetRasterCount(hDS) )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "panBandMap[%d]=%d",
                        i, panBandMap[i]);
                return FALSE;
            }
        }
    }
    else if( nBandCount > GDALGetRasterCount(hDS) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "nBandCount > GDALGetRasterCount(hDS)");
        return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                          GDALGetVirtualMem()                         */
/************************************************************************/

static CPLVirtualMem* GDALGetVirtualMem( GDALDatasetH hDS,
                                         GDALRasterBandH hBand,
                                         GDALRWFlag eRWFlag,
                                         int nXOff, int nYOff,
                                         int nXSize, int nYSize,
                                         int nBufXSize, int nBufYSize,
                                         GDALDataType eBufType,
                                         int nBandCount, int* panBandMap,
                                         int nPixelSpace,
                                         GIntBig nLineSpace,
                                         GIntBig nBandSpace,
                                         size_t nCacheSize,
                                         size_t nPageSizeHint,
                                         int bSingleThreadUsage,
                                         char **papszOptions )
{
    CPLVirtualMem* view;
    GDALVirtualMem* psParams;
    GUIntBig nReqMem;
    (void) papszOptions;

    if( nXSize != nBufXSize || nYSize != nBufYSize )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "nXSize != nBufXSize || nYSize != nBufYSize");
        return NULL;
    }
    
    int nRasterXSize = (hDS) ? GDALGetRasterXSize(hDS) : GDALGetRasterBandXSize(hBand);
    int nRasterYSize = (hDS) ? GDALGetRasterYSize(hDS) : GDALGetRasterBandYSize(hBand);

    if( nXOff < 0 || nYOff < 0 ||
        nXSize == 0 || nYSize == 0 ||
        nBufXSize < 0 || nBufYSize < 0 ||
        nXOff + nXSize > nRasterXSize ||
        nYOff + nYSize > nRasterYSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid window request");
        return NULL;
    }

    if( nPixelSpace < 0 || nLineSpace < 0 || nBandSpace < 0) 
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "nPixelSpace < 0 || nLineSpace < 0 || nBandSpace < 0");
        return NULL;
    }

    if( hDS != NULL && !GDALCheckBandParameters(hDS, nBandCount, panBandMap ) )
        return NULL;

    int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
    if( nPixelSpace == 0 )
        nPixelSpace = nDataTypeSize;
    if( nLineSpace == 0 )
        nLineSpace = (GIntBig)nBufXSize * nPixelSpace;
    if( nBandSpace == 0 )
        nBandSpace = (GIntBig)nBufYSize * nLineSpace;

    // OFFSET = offset(x,y,band) = x * nPixelSpace + y * nLineSpace + band * nBandSpace
    // where 0 <= x < nBufXSize and 0 <= y < nBufYSize and 0 <= band < nBandCount
    // if nPixelSpace, nLineSpace and nBandSpace can have arbitrary values, there's
    // no way of finding a unique(x,y,band) solution. We need to restrict the
    // space of possibilities strongly.
    // if nBandSpace >= nBufYSize * nLineSpace and nLineSpace >= nBufXSize * nPixelSpace,           INTERLEAVE = BAND
    //      band = OFFSET / nBandSpace
    //      y = (OFFSET - band * nBandSpace) / nLineSpace
    //      x = (OFFSET - band * nBandSpace - y * nLineSpace) / nPixelSpace
    // else if nPixelSpace >= nBandCount * nBandSpace and nLineSpace >= nBufXSize * nPixelSpace,    INTERLEAVE = PIXEL
    //      y = OFFSET / nLineSpace
    //      x = (OFFSET - y * nLineSpace) / nPixelSpace
    //      band = (OFFSET - y * nLineSpace - x * nPixelSpace) / nBandSpace

    if( nDataTypeSize == 0 || /* to please Coverity. not needed */
        nLineSpace < (GIntBig)nBufXSize * nPixelSpace ||
        (nBandCount > 1 &&
        (nBandSpace == nPixelSpace ||
        (nBandSpace < nPixelSpace && 
         (nBandSpace < nDataTypeSize || nPixelSpace < nBandCount * nBandSpace)) ||
        (nBandSpace > nPixelSpace &&
         (nPixelSpace < nDataTypeSize || nBandSpace < nBufYSize * nLineSpace)))) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only pixel interleaving or band interleaving are supported");
        return NULL;
    }

    /* Avoid odd spacings that would complicate I/O operations */
    /* Ensuring they are multiple of nDataTypeSize should be fine, because */
    /* the page size is a power of 2 that is also a multiple of nDataTypeSize */
    if( (nPixelSpace % nDataTypeSize) != 0 ||
        (nLineSpace % nDataTypeSize) != 0 ||
        (nBandSpace % nDataTypeSize) != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                    "Unsupported spacing");
        return NULL;
    }

    int bIsBandSequential = ( nBandSpace >= nBufYSize * nLineSpace );
    if( bIsBandSequential )
        nReqMem = nBandCount * nBandSpace;
    else
        nReqMem = nBufYSize * nLineSpace;
    if( nReqMem != (GUIntBig)(size_t)nReqMem )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot reserve " CPL_FRMT_GUIB " bytes", nReqMem);
        return NULL;
    }

    psParams = new GDALVirtualMem(hDS, hBand, nXOff, nYOff,
                               nXSize, nYSize,
                               nBufXSize, nBufYSize,
                               eBufType,
                               nBandCount, panBandMap,
                               nPixelSpace,
                               nLineSpace,
                               nBandSpace);

    view = CPLVirtualMemNew((size_t)nReqMem,
                         nCacheSize,
                         nPageSizeHint,
                         bSingleThreadUsage,
                         (eRWFlag == GF_Read) ? VIRTUALMEM_READONLY_ENFORCED : VIRTUALMEM_READWRITE,
                         (bIsBandSequential) ? GDALVirtualMem::FillCacheBandSequential :
                                                GDALVirtualMem::FillCachePixelInterleaved,
                         (bIsBandSequential) ? GDALVirtualMem::SaveFromCacheBandSequential :
                                                GDALVirtualMem::SaveFromCachePixelInterleaved,
                         GDALVirtualMem::Destroy,
                         psParams);

    if( view == NULL )
    {
        delete psParams;
    }

    return view;
}

/************************************************************************/
/*                       GDALDatasetGetVirtualMem()                     */
/************************************************************************/

/** Create a CPLVirtualMem object from a GDAL dataset object.
 *
 * Only supported on Linux for now.
 *
 * This method allows creating a virtual memory object for a region of one
 * or more GDALRasterBands from  this dataset. The content of the virtual
 * memory object is automatically filled from dataset content when a virtual
 * memory page is first accessed, and it is released (or flushed in case of a
 * "dirty" page) when the cache size limit has been reached.
 *
 * The pointer to access the virtual memory object is obtained with
 * CPLVirtualMemGetAddr(). It remains valid until CPLVirtualMemFree() is called.
 * CPLVirtualMemFree() must be called before the dataset object is destroyed.
 *
 * If p is such a pointer and base_type the C type matching eBufType, for default
 * values of spacing parameters, the element of image coordinates (x, y)
 * (relative to xOff, yOff) for band b can be accessed with
 * ((base_type*)p)[x + y * nBufXSize + (b-1)*nBufXSize*nBufYSize].
 *
 * Note that the mechanism used to transparently fill memory pages when they are
 * accessed is the same (but in a controlled way) than what occurs when a memory
 * error occurs in a program. Debugging software will generally interrupt program
 * execution when that happens. If needed, CPLVirtualMemPin() can be used to avoid
 * that by ensuring memory pages are allocated before being accessed.
 *
 * The size of the region that can be mapped as a virtual memory object depends
 * on hardware and operating system limitations.
 * On Linux AMD64 platforms, the maximum value is 128 TB.
 * On Linux x86 platforms, the maximum value is 2 GB.
 *
 * Data type translation is automatically done if the data type
 * (eBufType) of the buffer is different than
 * that of the GDALRasterBand.
 *
 * Image decimation / replication is currently not supported, i.e. if the
 * size of the region being accessed (nXSize x nYSize) is different from the
 * buffer size (nBufXSize x nBufYSize).
 *
 * The nPixelSpace, nLineSpace and nBandSpace parameters allow reading into or
 * writing from various organization of buffers. Arbitrary values for the spacing
 * parameters are not supported. Those values must be multiple of the size of the
 * buffer data type, and must be either band sequential organization (typically
 * nPixelSpace = GDALGetDataTypeSize(eBufType) / 8, nLineSpace = nPixelSpace * nBufXSize,
 * nBandSpace = nLineSpace * nBufYSize), or pixel-interleaved organization
 * (typically nPixelSpace = nBandSpace * nBandCount, nLineSpace = nPixelSpace * nBufXSize,
 * nBandSpace = GDALGetDataTypeSize(eBufType) / 8)
 *
 * @param hDS Dataset object
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GF_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param nBufXSize the width of the buffer image into which the desired region
 * is to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the data buffer. The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nBandCount the number of bands being read or written. 
 *
 * @param panBandMap the list of nBandCount band numbers being read/written.
 * Note band numbers are 1 based. This may be NULL to select the first 
 * nBandCount bands.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * the buffer to the start of the next pixel value within a scanline. If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * the buffer to the start of the next. If defaulted (0) the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param nBandSpace the byte offset from the start of one bands data to the
 * start of the next. If defaulted (0) the value will be 
 * nLineSpace * nBufYSize implying band sequential organization
 * of the data buffer.
 *
 * @param nCacheSize   size in bytes of the maximum memory that will be really
 *                     allocated (must ideally fit into RAM)
 *
 * @param nPageSizeHint hint for the page size. Must be a multiple of the
 *                      system page size, returned by CPLGetPageSize().
 *                      Minimum value is generally 4096. Might be set to 0 to
 *                      let the function determine a default page size.
 *
 * @param bSingleThreadUsage set to TRUE if there will be no concurrent threads
 *                           that will access the virtual memory mapping. This can
 *                           optimize performance a bit. If set to FALSE,
 *                           CPLVirtualMemDeclareThread() must be called.
 *
 * @param papszOptions NULL terminated list of options. Unused for now.
 *
 * @return a virtual memory object that must be freed by CPLVirtualMemFree(),
 *         or NULL in case of failure.
 *
 * @since GDAL 2.0
 */

CPLVirtualMem* GDALDatasetGetVirtualMem( GDALDatasetH hDS,
                                         GDALRWFlag eRWFlag,
                                         int nXOff, int nYOff,
                                         int nXSize, int nYSize,
                                         int nBufXSize, int nBufYSize,
                                         GDALDataType eBufType,
                                         int nBandCount, int* panBandMap,
                                         int nPixelSpace,
                                         GIntBig nLineSpace,
                                         GIntBig nBandSpace,
                                         size_t nCacheSize,
                                         size_t nPageSizeHint,
                                         int bSingleThreadUsage,
                                         char **papszOptions )
{
    return GDALGetVirtualMem( hDS, NULL, eRWFlag, nXOff, nYOff, nXSize, nYSize,
                              nBufXSize, nBufYSize, eBufType,
                              nBandCount, panBandMap,
                              nPixelSpace, nLineSpace, nBandSpace,
                              nCacheSize, nPageSizeHint, bSingleThreadUsage,
                              papszOptions );
}

/************************************************************************/
/*                     GDALRasterBandGetVirtualMem()                    */
/************************************************************************/

/** Create a CPLVirtualMem object from a GDAL raster band object.
 *
 * Only supported on Linux for now.
 *
 * This method allows creating a virtual memory object for a region of a
 * GDALRasterBand. The content of the virtual
 * memory object is automatically filled from dataset content when a virtual
 * memory page is first accessed, and it is released (or flushed in case of a
 * "dirty" page) when the cache size limit has been reached.
 *
 * The pointer to access the virtual memory object is obtained with
 * CPLVirtualMemGetAddr(). It remains valid until CPLVirtualMemFree() is called.
 * CPLVirtualMemFree() must be called before the raster band object is destroyed.
 *
 * If p is such a pointer and base_type the C type matching eBufType, for default
 * values of spacing parameters, the element of image coordinates (x, y)
 * (relative to xOff, yOff) can be accessed with
 * ((base_type*)p)[x + y * nBufXSize].
 *
 * Note that the mechanism used to transparently fill memory pages when they are
 * accessed is the same (but in a controlled way) than what occurs when a memory
 * error occurs in a program. Debugging software will generally interrupt program
 * execution when that happens. If needed, CPLVirtualMemPin() can be used to avoid
 * that by ensuring memory pages are allocated before being accessed.
 *
 * The size of the region that can be mapped as a virtual memory object depends
 * on hardware and operating system limitations.
 * On Linux AMD64 platforms, the maximum value is 128 TB.
 * On Linux x86 platforms, the maximum value is 2 GB.
 *
 * Data type translation is automatically done if the data type
 * (eBufType) of the buffer is different than
 * that of the GDALRasterBand.
 *
 * Image decimation / replication is currently not supported, i.e. if the
 * size of the region being accessed (nXSize x nYSize) is different from the
 * buffer size (nBufXSize x nBufYSize).
 *
 * The nPixelSpace and nLineSpace parameters allow reading into or
 * writing from various organization of buffers. Arbitrary values for the spacing
 * parameters are not supported. Those values must be multiple of the size of the
 * buffer data type and must be such that nLineSpace >= nPixelSpace * nBufXSize.
 *
 * @param hBand Rasterband object
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GF_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param nBufXSize the width of the buffer image into which the desired region
 * is to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired
 * region is to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the data buffer. The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * the buffer to the start of the next pixel value within a scanline. If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * the buffer to the start of the next. If defaulted (0) the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @param nCacheSize   size in bytes of the maximum memory that will be really
 *                     allocated (must ideally fit into RAM)
 *
 * @param nPageSizeHint hint for the page size. Must be a multiple of the
 *                      system page size, returned by CPLGetPageSize().
 *                      Minimum value is generally 4096. Might be set to 0 to
 *                      let the function determine a default page size.
 *
 * @param bSingleThreadUsage set to TRUE if there will be no concurrent threads
 *                           that will access the virtual memory mapping. This can
 *                           optimize performance a bit. If set to FALSE,
 *                           CPLVirtualMemDeclareThread() must be called.
 *
 * @param papszOptions NULL terminated list of options. Unused for now.
 *
 * @return a virtual memory object that must be freed by CPLVirtualMemFree(),
 *         or NULL in case of failure.
 *
 * @since GDAL 2.0
 */

CPLVirtualMem* GDALRasterBandGetVirtualMem( GDALRasterBandH hBand,
                                         GDALRWFlag eRWFlag,
                                         int nXOff, int nYOff,
                                         int nXSize, int nYSize,
                                         int nBufXSize, int nBufYSize,
                                         GDALDataType eBufType,
                                         int nPixelSpace,
                                         GIntBig nLineSpace,
                                         size_t nCacheSize,
                                         size_t nPageSizeHint,
                                         int bSingleThreadUsage,
                                         char **papszOptions )
{
    return GDALGetVirtualMem( NULL, hBand, eRWFlag, nXOff, nYOff, nXSize, nYSize,
                              nBufXSize, nBufYSize, eBufType,
                              1, NULL,
                              nPixelSpace, nLineSpace, 0,
                              nCacheSize, nPageSizeHint, bSingleThreadUsage,
                              papszOptions );
}

/************************************************************************/
/*                        GDALTiledVirtualMem                           */
/************************************************************************/

class GDALTiledVirtualMem
{
    GDALDatasetH hDS;
    GDALRasterBandH hBand;
    int nXOff;
    int nYOff;
    int nXSize;
    int nYSize;
    int nTileXSize;
    int nTileYSize;
    GDALDataType eBufType;
    int nBandCount;
    int* panBandMap;
    GDALTileOrganization eTileOrganization;

    void DoIO( GDALRWFlag eRWFlag, size_t nOffset,
               void* pPage, size_t nBytes ) const;

public:
             GDALTiledVirtualMem( GDALDatasetH hDS,
                                  GDALRasterBandH hBand,
                                  int nXOff, int nYOff,
                                  int nXSize, int nYSize,
                                  int nTileXSize, int nTileYSize,
                                  GDALDataType eBufType,
                                  int nBandCount, const int* panBandMapIn,
                                  GDALTileOrganization eTileOrganization );
            ~GDALTiledVirtualMem();

    static void FillCache(CPLVirtualMem* ctxt,  size_t nOffset,
                                         void* pPageToFill,
                                         size_t nPageSize, void* pUserData);
    static void SaveFromCache(CPLVirtualMem* ctxt,  size_t nOffset,
                                             const void* pPageToBeEvicted,
                                             size_t nToEvicted, void* pUserData);

    static void Destroy(void* pUserData);
};

/************************************************************************/
/*                        GDALTiledVirtualMem()                         */
/************************************************************************/

GDALTiledVirtualMem::GDALTiledVirtualMem( GDALDatasetH hDS,
                                          GDALRasterBandH hBand,
                                  int nXOff, int nYOff,
                                  int nXSize, int nYSize,
                                  int nTileXSize, int nTileYSize,
                                  GDALDataType eBufType,
                                  int nBandCount, const int* panBandMapIn,
                                  GDALTileOrganization eTileOrganization ):
    hDS(hDS), hBand(hBand), nXOff(nXOff), nYOff(nYOff), nXSize(nXSize), nYSize(nYSize),
    nTileXSize(nTileXSize), nTileYSize(nTileYSize), eBufType(eBufType),
    nBandCount(nBandCount), eTileOrganization(eTileOrganization)
{
    if( hDS != NULL )
    {
        if( panBandMapIn )
        {
            panBandMap = (int*) CPLMalloc(nBandCount * sizeof(int));
            memcpy(panBandMap, panBandMapIn, nBandCount * sizeof(int));
        }
        else
        {
            panBandMap = (int*) CPLMalloc(nBandCount * sizeof(int));
            for(int i=0;i<nBandCount;i++)
                panBandMap[i] = i + 1;
        }
    }
    else
    {
        panBandMap = NULL;
        nBandCount = 1;
    }
}

/************************************************************************/
/*                       ~GDALTiledVirtualMem()                         */
/************************************************************************/

GDALTiledVirtualMem::~GDALTiledVirtualMem()
{
    CPLFree(panBandMap);
}

/************************************************************************/
/*                                DoIO()                                */
/************************************************************************/

void GDALTiledVirtualMem::DoIO( GDALRWFlag eRWFlag, size_t nOffset,
                                void* pPage, size_t nBytes ) const
{
    int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
    int nTilesPerRow = (nXSize + nTileXSize - 1) / nTileXSize;
    int nTilesPerCol = (nYSize + nTileYSize - 1) / nTileYSize;
    size_t nPageSize = nTileXSize * nTileYSize * nDataTypeSize;
    if( eTileOrganization != GTO_BSQ )
        nPageSize *= nBandCount;
    CPLAssert((nOffset % nPageSize) == 0);
    CPLAssert(nBytes == nPageSize);
    size_t nTile;
    int band;
    int nPixelSpace, nLineSpace, nBandSpace;
    if( eTileOrganization == GTO_TIP )
    {
        nTile = nOffset / nPageSize;
        band = 0;
        nPixelSpace = nDataTypeSize * nBandCount;
        nLineSpace = nPixelSpace * nTileXSize;
        nBandSpace = nDataTypeSize;
    }
    else if( eTileOrganization == GTO_BIT )
    {
        nTile = nOffset / nPageSize;
        band = 0;
        nPixelSpace = nDataTypeSize;
        nLineSpace = nPixelSpace * nTileXSize;
        nBandSpace = nLineSpace * nTileYSize;
    }
    else
    {
        //offset = nPageSize * (band * nTilesPerRow * nTilesPerCol + nTile)
        band = nOffset / (nPageSize * nTilesPerRow * nTilesPerCol);
        nTile = nOffset / nPageSize - band * nTilesPerRow * nTilesPerCol;
        nPixelSpace = nDataTypeSize;
        nLineSpace = nPixelSpace * nTileXSize;
        nBandSpace = 0;
        band ++;
    }
    size_t nYTile = nTile / nTilesPerRow;
    size_t nXTile = nTile - nYTile * nTilesPerRow;

    int nReqXSize = MIN( nTileXSize, nXSize - (int)(nXTile * nTileXSize) );
    int nReqYSize = MIN( nTileYSize, nYSize - (int)(nYTile * nTileYSize) );
    if( eRWFlag == GF_Read && (nReqXSize < nTileXSize || nReqYSize < nTileYSize) )
        memset(pPage, 0, nBytes);
    if( hDS != NULL )
    {
        GDALDatasetRasterIO( hDS, eRWFlag,
                            nXOff + nXTile * nTileXSize, nYOff + nYTile * nTileYSize,
                            nReqXSize, nReqYSize,
                            pPage,
                            nReqXSize, nReqYSize,
                            eBufType,
                            ( eTileOrganization != GTO_BSQ ) ? nBandCount : 1,
                            ( eTileOrganization != GTO_BSQ ) ? panBandMap : &band,
                            nPixelSpace, nLineSpace, nBandSpace );
    }
    else
    {
        GDALRasterIO(hBand, eRWFlag,
                     nXOff + nXTile * nTileXSize, nYOff + nYTile * nTileYSize,
                     nReqXSize, nReqYSize,
                     pPage,
                     nReqXSize, nReqYSize,
                     eBufType,
                     nPixelSpace, nLineSpace );
    }
}

/************************************************************************/
/*                           FillCache()                                */
/************************************************************************/

void GDALTiledVirtualMem::FillCache(CPLVirtualMem* ctxt,  size_t nOffset,
                                         void* pPageToFill,
                                         size_t nToFill, void* pUserData)
{
    const GDALTiledVirtualMem* psParms = (const GDALTiledVirtualMem* )pUserData;
    (void)ctxt;
    psParms->DoIO(GF_Read, nOffset, pPageToFill, nToFill);
}

/************************************************************************/
/*                          SaveFromCache()                             */
/************************************************************************/

void GDALTiledVirtualMem::SaveFromCache(CPLVirtualMem* ctxt,  size_t nOffset,
                                             const void* pPageToBeEvicted,
                                             size_t nToEvicted, void* pUserData)
{
    const GDALTiledVirtualMem* psParms = (const GDALTiledVirtualMem* )pUserData;
    (void)ctxt;
    psParms->DoIO(GF_Write, nOffset, (void*)pPageToBeEvicted, nToEvicted);
}

/************************************************************************/
/*                                Destroy()                             */
/************************************************************************/

void GDALTiledVirtualMem::Destroy(void* pUserData)
{
    GDALTiledVirtualMem* psParams = (GDALTiledVirtualMem*) pUserData;
    delete psParams;
}

/************************************************************************/
/*                      GDALGetTiledVirtualMem()                        */
/************************************************************************/

static CPLVirtualMem* GDALGetTiledVirtualMem( GDALDatasetH hDS,
                                              GDALRasterBandH hBand,
                                              GDALRWFlag eRWFlag,
                                              int nXOff, int nYOff,
                                              int nXSize, int nYSize,
                                              int nTileXSize, int nTileYSize,
                                              GDALDataType eBufType,
                                              int nBandCount, int* panBandMap,
                                              GDALTileOrganization eTileOrganization,
                                              size_t nCacheSize,
                                              int bSingleThreadUsage,
                                              char **papszOptions )
{
    CPLVirtualMem* view;
    GDALTiledVirtualMem* psParams;
    (void) papszOptions;

    int nRasterXSize = (hDS) ? GDALGetRasterXSize(hDS) : GDALGetRasterBandXSize(hBand);
    int nRasterYSize = (hDS) ? GDALGetRasterYSize(hDS) : GDALGetRasterBandYSize(hBand);

    if( nXOff < 0 || nYOff < 0 ||
        nTileXSize <= 0 || nTileYSize <= 0 ||
        nXOff + nXSize > nRasterXSize ||
        nYOff + nYSize > nRasterYSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid window request");
        return NULL;
    }

    if( hDS != NULL && !GDALCheckBandParameters(hDS, nBandCount, panBandMap ) )
        return NULL;

    int nDataTypeSize = GDALGetDataTypeSize(eBufType) / 8;
    int nTilesPerRow = (nXSize + nTileXSize - 1) / nTileXSize;
    int nTilesPerCol = (nYSize + nTileYSize - 1) / nTileYSize;
    GUIntBig nReqMem = (GUIntBig)nTilesPerRow * nTilesPerCol *
                        nTileXSize * nTileYSize * nBandCount * nDataTypeSize;
    if( nReqMem != (GUIntBig)(size_t)nReqMem )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot reserve " CPL_FRMT_GUIB " bytes", nReqMem);
        return NULL;
    }

    size_t nPageSizeHint = nTileXSize * nTileYSize * nDataTypeSize;
    if( eTileOrganization != GTO_BSQ )
        nPageSizeHint *= nBandCount;
    if( (nPageSizeHint % CPLGetPageSize()) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Tile dimensions incompatible with page size");
        return NULL;
    }

    psParams = new GDALTiledVirtualMem(hDS, hBand, nXOff, nYOff,
                                       nXSize, nYSize,
                                       nTileXSize, nTileYSize,
                                       eBufType,
                                       nBandCount, panBandMap,
                                       eTileOrganization);

    view = CPLVirtualMemNew((size_t)nReqMem,
                         nCacheSize,
                         nPageSizeHint,
                         bSingleThreadUsage,
                         (eRWFlag == GF_Read) ? VIRTUALMEM_READONLY_ENFORCED : VIRTUALMEM_READWRITE,
                         GDALTiledVirtualMem::FillCache,
                         GDALTiledVirtualMem::SaveFromCache,
                         GDALTiledVirtualMem::Destroy,
                         psParams);

    if( view == NULL )
    {
        delete psParams;
    }
    else if( CPLVirtualMemGetPageSize(view) != nPageSizeHint )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Did not get expected page size : %d vs %d",
                 (int)CPLVirtualMemGetPageSize(view), (int)nPageSizeHint);
        CPLVirtualMemFree(view);
        return NULL;
    }

    return view;
}

/************************************************************************/
/*                   GDALDatasetGetTiledVirtualMem()                    */
/************************************************************************/

/** Create a CPLVirtualMem object from a GDAL dataset object, with tiling
 * organization
 *
 * Only supported on Linux for now.
 *
 * This method allows creating a virtual memory object for a region of one
 * or more GDALRasterBands from  this dataset. The content of the virtual
 * memory object is automatically filled from dataset content when a virtual
 * memory page is first accessed, and it is released (or flushed in case of a
 * "dirty" page) when the cache size limit has been reached.
 *
 * Contrary to GDALDatasetGetVirtualMem(), pixels will be organized by tiles
 * instead of scanlines. Different ways of organizing pixel within/accross tiles
 * can be selected with the eTileOrganization parameter.
 *
 * If nXSize is not a multiple of nTileXSize or nYSize is not a multiple of
 * nTileYSize, partial tiles will exists at the right and/or bottom of the region
 * of interest. Those partial tiles will also have nTileXSize * nTileYSize dimension,
 * with padding pixels.
 *
 * The pointer to access the virtual memory object is obtained with
 * CPLVirtualMemGetAddr(). It remains valid until CPLVirtualMemFree() is called.
 * CPLVirtualMemFree() must be called before the dataset object is destroyed.
 *
 * If p is such a pointer and base_type the C type matching eBufType, for default
 * values of spacing parameters, the element of image coordinates (x, y)
 * (relative to xOff, yOff) for band b can be accessed with :
 *  - for eTileOrganization = GTO_TIP, ((base_type*)p)[tile_number(x,y)*nBandCount*tile_size + offset_in_tile(x,y)*nBandCount + (b-1)].
 *  - for eTileOrganization = GTO_BIT, ((base_type*)p)[(tile_number(x,y)*nBandCount + (b-1)) * tile_size + offset_in_tile(x,y)].
 *  - for eTileOrganization = GTO_BSQ, ((base_type*)p)[(tile_number(x,y) + (b-1)*nTilesCount) * tile_size + offset_in_tile(x,y)].
 *
 * where nTilesPerRow = ceil(nXSize / nTileXSize)
 *       nTilesPerCol = ceil(nYSize / nTileYSize)
 *       nTilesCount = nTilesPerRow * nTilesPerCol
 *       tile_number(x,y) = (y / nTileYSize) * nTilesPerRow + (x / nTileXSize)
 *       offset_in_tile(x,y) = (y % nTileYSize) * nTileXSize  + (x % nTileXSize)
 *       tile_size = nTileXSize * nTileYSize
 *
 * Note that for a single band request, all tile organizations are equivalent.
 *
 * Note that the mechanism used to transparently fill memory pages when they are
 * accessed is the same (but in a controlled way) than what occurs when a memory
 * error occurs in a program. Debugging software will generally interrupt program
 * execution when that happens. If needed, CPLVirtualMemPin() can be used to avoid
 * that by ensuring memory pages are allocated before being accessed.
 *
 * The size of the region that can be mapped as a virtual memory object depends
 * on hardware and operating system limitations.
 * On Linux AMD64 platforms, the maximum value is 128 TB.
 * On Linux x86 platforms, the maximum value is 2 GB.
 *
 * Data type translation is automatically done if the data type
 * (eBufType) of the buffer is different than
 * that of the GDALRasterBand.
 *
 * @param hDS Dataset object
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GF_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param nTileXSize the width of the tiles.
 *
 * @param nTileYSize the height of the tiles.
 *
 * @param eBufType the type of the pixel values in the data buffer. The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nBandCount the number of bands being read or written. 
 *
 * @param panBandMap the list of nBandCount band numbers being read/written.
 * Note band numbers are 1 based. This may be NULL to select the first 
 * nBandCount bands.
 *
 * @param eTileOrganization tile organization.
 *
 * @param nCacheSize   size in bytes of the maximum memory that will be really
 *                     allocated (must ideally fit into RAM)
 *
 * @param bSingleThreadUsage set to TRUE if there will be no concurrent threads
 *                           that will access the virtual memory mapping. This can
 *                           optimize performance a bit. If set to FALSE,
 *                           CPLVirtualMemDeclareThread() must be called.
 *
 * @param papszOptions NULL terminated list of options. Unused for now.
 *
 * @return a virtual memory object that must be freed by CPLVirtualMemFree(),
 *         or NULL in case of failure.
 *
 * @since GDAL 2.0
 */

CPLVirtualMem* GDALDatasetGetTiledVirtualMem( GDALDatasetH hDS,
                                              GDALRWFlag eRWFlag,
                                              int nXOff, int nYOff,
                                              int nXSize, int nYSize,
                                              int nTileXSize, int nTileYSize,
                                              GDALDataType eBufType,
                                              int nBandCount, int* panBandMap,
                                              GDALTileOrganization eTileOrganization,
                                              size_t nCacheSize,
                                              int bSingleThreadUsage,
                                              char **papszOptions )
{
    return GDALGetTiledVirtualMem( hDS, NULL, eRWFlag, nXOff, nYOff,
                                   nXSize, nYSize, nTileXSize, nTileYSize,
                                   eBufType, nBandCount, panBandMap,
                                   eTileOrganization,
                                   nCacheSize, bSingleThreadUsage, papszOptions );
}

/************************************************************************/
/*                   GDALRasterBandGetTiledVirtualMem()                 */
/************************************************************************/

/** Create a CPLVirtualMem object from a GDAL rasterband object, with tiling
 * organization
 *
 * Only supported on Linux for now.
 *
 * This method allows creating a virtual memory object for a region of one
 * GDALRasterBand. The content of the virtual
 * memory object is automatically filled from dataset content when a virtual
 * memory page is first accessed, and it is released (or flushed in case of a
 * "dirty" page) when the cache size limit has been reached.
 *
 * Contrary to GDALDatasetGetVirtualMem(), pixels will be organized by tiles
 * instead of scanlines.
 *
 * If nXSize is not a multiple of nTileXSize or nYSize is not a multiple of
 * nTileYSize, partial tiles will exists at the right and/or bottom of the region
 * of interest. Those partial tiles will also have nTileXSize * nTileYSize dimension,
 * with padding pixels.
 *
 * The pointer to access the virtual memory object is obtained with
 * CPLVirtualMemGetAddr(). It remains valid until CPLVirtualMemFree() is called.
 * CPLVirtualMemFree() must be called before the raster band object is destroyed.
 *
 * If p is such a pointer and base_type the C type matching eBufType, for default
 * values of spacing parameters, the element of image coordinates (x, y)
 * (relative to xOff, yOff) can be accessed with :
 *  ((base_type*)p)[tile_number(x,y)*tile_size + offset_in_tile(x,y)].
 *
 * where nTilesPerRow = ceil(nXSize / nTileXSize)
 *       nTilesCount = nTilesPerRow * nTilesPerCol
 *       tile_number(x,y) = (y / nTileYSize) * nTilesPerRow + (x / nTileXSize)
 *       offset_in_tile(x,y) = (y % nTileYSize) * nTileXSize  + (x % nTileXSize)
 *       tile_size = nTileXSize * nTileYSize
 *
 * Note that the mechanism used to transparently fill memory pages when they are
 * accessed is the same (but in a controlled way) than what occurs when a memory
 * error occurs in a program. Debugging software will generally interrupt program
 * execution when that happens. If needed, CPLVirtualMemPin() can be used to avoid
 * that by ensuring memory pages are allocated before being accessed.
 *
 * The size of the region that can be mapped as a virtual memory object depends
 * on hardware and operating system limitations.
 * On Linux AMD64 platforms, the maximum value is 128 TB.
 * On Linux x86 platforms, the maximum value is 2 GB.
 *
 * Data type translation is automatically done if the data type
 * (eBufType) of the buffer is different than
 * that of the GDALRasterBand.
 *
 * @param hBand Rasterband object
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GF_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed.  This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param nTileXSize the width of the tiles.
 *
 * @param nTileYSize the height of the tiles.
 *
 * @param eBufType the type of the pixel values in the data buffer. The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nCacheSize   size in bytes of the maximum memory that will be really
 *                     allocated (must ideally fit into RAM)
 *
 * @param bSingleThreadUsage set to TRUE if there will be no concurrent threads
 *                           that will access the virtual memory mapping. This can
 *                           optimize performance a bit. If set to FALSE,
 *                           CPLVirtualMemDeclareThread() must be called.
 *
 * @param papszOptions NULL terminated list of options. Unused for now.
 *
 * @return a virtual memory object that must be freed by CPLVirtualMemFree(),
 *         or NULL in case of failure.
 *
 * @since GDAL 2.0
 */

CPLVirtualMem* GDALRasterBandGetTiledVirtualMem( GDALRasterBandH hBand,
                                              GDALRWFlag eRWFlag,
                                              int nXOff, int nYOff,
                                              int nXSize, int nYSize,
                                              int nTileXSize, int nTileYSize,
                                              GDALDataType eBufType,
                                              size_t nCacheSize,
                                              int bSingleThreadUsage,
                                              char **papszOptions )
{
    return GDALGetTiledVirtualMem( NULL, hBand, eRWFlag, nXOff, nYOff,
                                   nXSize, nYSize, nTileXSize, nTileYSize,
                                   eBufType, 1, NULL,
                                   GTO_BSQ,
                                   nCacheSize, bSingleThreadUsage, papszOptions );
}
