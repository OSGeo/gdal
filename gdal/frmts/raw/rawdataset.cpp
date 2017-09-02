/******************************************************************************
 *
 * Project:  Generic Raw Binary Driver
 * Purpose:  Implementation of RawDataset and RawRasterBand classes.
 * Author:   Frank Warmerdam, warmerda@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "rawdataset.h"

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_virtualmem.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( GDALDataset *poDSIn, int nBandIn,
                              void *fpRawIn, vsi_l_offset nImgOffsetIn,
                              int nPixelOffsetIn, int nLineOffsetIn,
                              GDALDataType eDataTypeIn, int bNativeOrderIn,
                              int bIsVSILIn, int bOwnsFPIn ) :
    fpRaw(NULL),
    fpRawL(NULL),
    bIsVSIL(bIsVSILIn),
    nImgOffset(nImgOffsetIn),
    nPixelOffset(nPixelOffsetIn),
    nLineOffset(nLineOffsetIn),
    bNativeOrder(bNativeOrderIn),
    bOwnsFP(bOwnsFPIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDataTypeIn;

    if (bIsVSIL)
    {
        fpRawL = reinterpret_cast<VSILFILE *>(fpRawIn);
    }
    else
    {
        fpRaw = reinterpret_cast<FILE *>(fpRawIn);
    }

    CPLDebug("GDALRaw",
             "RawRasterBand(%p,%d,%p,\n"
             "              Off=%d,PixOff=%d,LineOff=%d,%s,%d)",
             poDS, nBand, fpRaw,
             static_cast<unsigned int>(nImgOffset), nPixelOffset, nLineOffset,
             GDALGetDataTypeName(eDataType), bNativeOrder);

    // Treat one scanline as the block size.
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    // Initialize other fields, and setup the line buffer.
    Initialize();
}

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( void *fpRawIn, vsi_l_offset nImgOffsetIn,
                              int nPixelOffsetIn, int nLineOffsetIn,
                              GDALDataType eDataTypeIn, int bNativeOrderIn,
                              int nXSize, int nYSize, int bIsVSILIn,
                              int bOwnsFPIn ) :
    fpRaw(NULL),
    fpRawL(NULL),
    bIsVSIL(bIsVSILIn),
    nImgOffset(nImgOffsetIn),
    nPixelOffset(nPixelOffsetIn),
    nLineOffset(nLineOffsetIn),
    nLineSize(0),
    bNativeOrder(bNativeOrderIn),
    nLoadedScanline(0),
    pLineStart(NULL),
    bDirty(FALSE),
    poCT(NULL),
    eInterp(GCI_Undefined),
    papszCategoryNames(NULL),
    bOwnsFP(bOwnsFPIn)
{
    poDS = NULL;
    nBand = 1;
    eDataType = eDataTypeIn;

    if (bIsVSIL)
    {
        fpRawL = reinterpret_cast<VSILFILE *>(fpRawIn);
    }
    else
    {
        fpRaw = reinterpret_cast<FILE *>(fpRawIn);
    }

    CPLDebug("GDALRaw",
             "RawRasterBand(floating,Off=%d,PixOff=%d,LineOff=%d,%s,%d)",
             static_cast<unsigned int>(nImgOffset),
             nPixelOffset, nLineOffset,
             GDALGetDataTypeName(eDataType), bNativeOrder);

    // Treat one scanline as the block size.
    nBlockXSize = nXSize;
    nBlockYSize = 1;
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    if (!GDALCheckDatasetDimensions(nXSize, nYSize))
    {
        pLineBuffer = NULL;
        return;
    }

    // Initialize other fields, and setup the line buffer.
    Initialize();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void RawRasterBand::Initialize()

{
    poCT = NULL;
    eInterp = GCI_Undefined;

    papszCategoryNames = NULL;

    bDirty = FALSE;

    // Allocate working scanline.
    nLoadedScanline = -1;
    if (nBlockXSize <= 0 ||
        std::abs(nPixelOffset) > std::numeric_limits<int>::max() / nBlockXSize)
    {
        nLineSize = 0;
        pLineBuffer = NULL;
    }
    else
    {
        nLineSize = std::abs(nPixelOffset) * nBlockXSize;
        pLineBuffer = VSIMalloc2(std::abs(nPixelOffset), nBlockXSize);
    }
    if (pLineBuffer == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not allocate line buffer: "
                 "nPixelOffset=%d, nBlockXSize=%d",
                 nPixelOffset, nBlockXSize);
    }

    if( nPixelOffset >= 0 )
        pLineStart = pLineBuffer;
    else
        pLineStart = static_cast<char *>(pLineBuffer) +
                     static_cast<std::ptrdiff_t>(std::abs(nPixelOffset)) *
                         (nBlockXSize - 1);
}

/************************************************************************/
/*                           ~RawRasterBand()                           */
/************************************************************************/

RawRasterBand::~RawRasterBand()

{
    if( poCT )
        delete poCT;

    CSLDestroy(papszCategoryNames);

    FlushCache();

    if (bOwnsFP)
    {
        if ( bIsVSIL )
        {
            if( VSIFCloseL(fpRawL) != 0 )
            {
                CPLError(CE_Failure, CPLE_FileIO, "I/O error");
            }
        }
        else
        {
            VSIFClose(fpRaw);
        }
    }

    CPLFree(pLineBuffer);
}

/************************************************************************/
/*                             SetAccess()                              */
/************************************************************************/

void RawRasterBand::SetAccess(GDALAccess eAccessIn) { eAccess = eAccessIn; }

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we have the opportunity to call             */
/*      fflush().  We don't want to do this all the time in the         */
/*      write block function as it is kind of expensive.                */
/************************************************************************/

CPLErr RawRasterBand::FlushCache()

{
    CPLErr eErr = GDALRasterBand::FlushCache();
    if( eErr != CE_None )
    {
        bDirty = FALSE;
        return eErr;
    }

    // If we have unflushed raw, flush it to disk now.
    if ( bDirty )
    {
        int nRet = 0;
        if( bIsVSIL )
            nRet = VSIFFlushL(fpRawL);
        else
            /* nRet = */ VSIFFlush(fpRaw);

        bDirty = FALSE;
        if( nRet < 0 )
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             AccessLine()                             */
/************************************************************************/

CPLErr RawRasterBand::AccessLine( int iLine )

{
    if (pLineBuffer == NULL)
        return CE_Failure;

    if( nLoadedScanline == iLine )
        return CE_None;

    // Figure out where to start reading.
    // Write formulas such that unsigned int overflow doesn't occur
    const GUIntBig nPixelOffsetToSubstract =
        nPixelOffset >= 0
        ? 0 : static_cast<GUIntBig>(-static_cast<GIntBig>(nPixelOffset)) * (nBlockXSize - 1);
    const vsi_l_offset nReadStart = static_cast<vsi_l_offset>(
        (nLineOffset >= 0 ?
            nImgOffset + static_cast<GUIntBig>(nLineOffset) * iLine :
            nImgOffset - static_cast<GUIntBig>(-static_cast<GIntBig>(nLineOffset)) * iLine )
        - nPixelOffsetToSubstract);

    // Seek to the correct line.
    if( Seek(nReadStart, SEEK_SET) == -1 )
    {
        if (poDS != NULL && poDS->GetAccess() == GA_ReadOnly)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed to seek to scanline %d @ " CPL_FRMT_GUIB ".",
                     iLine, nReadStart);
            return CE_Failure;
        }
        else
        {
            memset(pLineBuffer, 0, nLineSize);
            nLoadedScanline = iLine;
            return CE_None;
        }
    }

    // Read the line.  Take care not to request any more bytes than
    // are needed, and not to lose a partially successful scanline read.
    const size_t nBytesToRead = std::abs(nPixelOffset) * (nBlockXSize - 1)
        + GDALGetDataTypeSizeBytes(GetRasterDataType());

    const size_t nBytesActuallyRead = Read(pLineBuffer, 1, nBytesToRead);
    if( nBytesActuallyRead < nBytesToRead )
    {
        if (poDS != NULL && poDS->GetAccess() == GA_ReadOnly)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed to read scanline %d.",
                     iLine);
            return CE_Failure;
        }
        else
        {
            memset(
                static_cast<GByte *>(pLineBuffer) + nBytesActuallyRead,
                0, nBytesToRead - nBytesActuallyRead);
        }
    }

    // Byte swap the interesting data, if required.
    if( !bNativeOrder && eDataType != GDT_Byte )
    {
        if( GDALDataTypeIsComplex(eDataType) )
        {
            const int nWordSize = GDALGetDataTypeSize(eDataType) / 16;
            GDALSwapWords(pLineBuffer, nWordSize, nBlockXSize,
                          std::abs(nPixelOffset));
            GDALSwapWords(
                static_cast<GByte *>(pLineBuffer) + nWordSize,
                nWordSize, nBlockXSize, std::abs(nPixelOffset));
        }
        else
        {
            GDALSwapWords(pLineBuffer, GDALGetDataTypeSizeBytes(eDataType),
                          nBlockXSize, std::abs(nPixelOffset));
        }
    }

    nLoadedScanline = iLine;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RawRasterBand::IReadBlock(CPL_UNUSED int nBlockXOff,
                                 int nBlockYOff,
                                 void *pImage)
{
    CPLAssert(nBlockXOff == 0);

    if (pLineBuffer == NULL)
        return CE_Failure;

    const CPLErr eErr = AccessLine(nBlockYOff);
    if( eErr == CE_Failure )
        return eErr;

    // Copy data from disk buffer to user block buffer.
    GDALCopyWords(pLineStart, eDataType, nPixelOffset,
                  pImage, eDataType, GDALGetDataTypeSizeBytes(eDataType),
                  nBlockXSize);

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr RawRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff,
                                   int nBlockYOff,
                                   void *pImage )
{
    CPLAssert(nBlockXOff == 0);

    if (pLineBuffer == NULL)
        return CE_Failure;

    // If the data for this band is completely contiguous, we don't
    // have to worry about pre-reading from disk.
    CPLErr eErr = CE_None;
    if( std::abs(nPixelOffset) > GDALGetDataTypeSizeBytes(eDataType) )
        eErr = AccessLine(nBlockYOff);

    // Copy data from user buffer into disk buffer.
    GDALCopyWords(pImage, eDataType, GDALGetDataTypeSizeBytes(eDataType),
                  pLineStart, eDataType, nPixelOffset, nBlockXSize);


    // Byte swap (if necessary) back into disk order before writing.
    if( !bNativeOrder && eDataType != GDT_Byte )
    {
        if( GDALDataTypeIsComplex(eDataType) )
        {
            const int nWordSize = GDALGetDataTypeSize(eDataType) / 16;
            GDALSwapWords(pLineBuffer, nWordSize, nBlockXSize,
                          std::abs(nPixelOffset));
            GDALSwapWords(static_cast<GByte *>(pLineBuffer) + nWordSize,
                          nWordSize, nBlockXSize, std::abs(nPixelOffset));
        }
        else
        {
            GDALSwapWords(pLineBuffer, GDALGetDataTypeSizeBytes(eDataType),
                          nBlockXSize, std::abs(nPixelOffset));
        }
    }

    // Figure out where to start writing.
    // Write formulas such that unsigned int overflow doesn't occur
    const GUIntBig nPixelOffsetToSubstract =
        nPixelOffset >= 0
        ? 0 : static_cast<GUIntBig>(-static_cast<GIntBig>(nPixelOffset)) * (nBlockXSize - 1);
    const vsi_l_offset nWriteStart = static_cast<vsi_l_offset>(
        (nLineOffset >= 0 ?
            nImgOffset + static_cast<GUIntBig>(nLineOffset) * nBlockYOff :
            nImgOffset - static_cast<GUIntBig>(-static_cast<GIntBig>(nLineOffset)) * nBlockYOff )
        - nPixelOffsetToSubstract);

    // Seek to correct location.
    if( Seek(nWriteStart, SEEK_SET) == -1 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to seek to scanline %d @ " CPL_FRMT_GUIB
                 " to write to file.",
                 nBlockYOff, nImgOffset + nBlockYOff * nLineOffset);

        eErr = CE_Failure;
    }

    // Write data buffer.
    const int nBytesToWrite = std::abs(nPixelOffset) * (nBlockXSize - 1) +
                              GDALGetDataTypeSizeBytes(GetRasterDataType());

    if( eErr == CE_None
        && Write(pLineBuffer, 1, nBytesToWrite)
        < static_cast<size_t>(nBytesToWrite) )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to write scanline %d to file.",
                 nBlockYOff);

        eErr = CE_Failure;
    }

    // Byte swap (if necessary) back into machine order so the
    // buffer is still usable for reading purposes.
    if( !bNativeOrder && eDataType != GDT_Byte )
    {
        if( GDALDataTypeIsComplex(eDataType) )
        {
            const int nWordSize = GDALGetDataTypeSize(eDataType) / 16;
            GDALSwapWords(pLineBuffer, nWordSize, nBlockXSize,
                          std::abs(nPixelOffset));
            GDALSwapWords(static_cast<GByte *>(pLineBuffer) +
                          nWordSize, nWordSize, nBlockXSize,
                          std::abs(nPixelOffset));
        }
        else
        {
            GDALSwapWords(pLineBuffer, GDALGetDataTypeSizeBytes(eDataType),
                          nBlockXSize, std::abs(nPixelOffset));
        }
    }

    bDirty = TRUE;
    return eErr;
}

/************************************************************************/
/*                             AccessBlock()                            */
/************************************************************************/

CPLErr RawRasterBand::AccessBlock(vsi_l_offset nBlockOff, size_t nBlockSize,
                                  void *pData)
{
    // Seek to the correct block.
    if( Seek(nBlockOff, SEEK_SET) == -1 )
    {
        memset(pData, 0, nBlockSize);
        return CE_None;
    }

    // Read the block.
    const size_t nBytesActuallyRead = Read(pData, 1, nBlockSize);
    if( nBytesActuallyRead < nBlockSize )
    {

        memset(static_cast<GByte *>(pData) + nBytesActuallyRead,
               0, nBlockSize - nBytesActuallyRead);
        return CE_None;
    }

    // Byte swap the interesting data, if required.
    if( !bNativeOrder && eDataType != GDT_Byte )
    {
        if( GDALDataTypeIsComplex(eDataType) )
        {
            const int nWordSize = GDALGetDataTypeSize(eDataType) / 16;
            GDALSwapWordsEx(pData, nWordSize, nBlockSize / nPixelOffset,
                            nPixelOffset);
            GDALSwapWordsEx(static_cast<GByte *>(pData) + nWordSize,
                            nWordSize, nBlockSize / nPixelOffset, nPixelOffset);
        }
        else
        {
            GDALSwapWordsEx(pData, GDALGetDataTypeSizeBytes(eDataType),
                            nBlockSize / nPixelOffset, nPixelOffset);
        }
    }

    return CE_None;
}

/************************************************************************/
/*               IsSignificantNumberOfLinesLoaded()                     */
/*                                                                      */
/*  Check if there is a significant number of scanlines (>20%) from the */
/*  specified block of lines already cached.                            */
/************************************************************************/

int RawRasterBand::IsSignificantNumberOfLinesLoaded( int nLineOff, int nLines )
{
    int nCountLoaded = 0;

    for ( int iLine = nLineOff; iLine < nLineOff + nLines; iLine++ )
    {
        GDALRasterBlock *poBlock = TryGetLockedBlockRef(0, iLine);
        if( poBlock != NULL )
        {
            poBlock->DropLock();
            nCountLoaded++;
            if( nCountLoaded > nLines / 20 )
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

/************************************************************************/
/*                           CanUseDirectIO()                           */
/************************************************************************/

int RawRasterBand::CanUseDirectIO(int /* nXOff */,
                                  int nYOff,
                                  int nXSize,
                                  int nYSize,
                                  GDALDataType /* eBufType*/)
{

    // Use direct IO without caching if:
    //
    // GDAL_ONE_BIG_READ is enabled
    //
    // or
    //
    // the length of a scanline on disk is more than 50000 bytes, and the
    // width of the requested chunk is less than 40% of the whole scanline and
    // no significant number of requested scanlines are already in the cache.

    if( nPixelOffset < 0 )
    {
        return FALSE;
    }

    const char *pszGDAL_ONE_BIG_READ =
        CPLGetConfigOption("GDAL_ONE_BIG_READ", NULL);
    if ( pszGDAL_ONE_BIG_READ == NULL )
    {
        if ( nLineSize < 50000
             || nXSize > nLineSize / nPixelOffset / 5 * 2
             || IsSignificantNumberOfLinesLoaded(nYOff, nYSize) )
        {
            return FALSE;
        }
        return TRUE;
    }

    return CPLTestBool(pszGDAL_ONE_BIG_READ);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr RawRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    const int nBandDataSize = GDALGetDataTypeSizeBytes(eDataType);
#ifdef DEBUG
    // Otherwise Coverity thinks that a divide by zero is possible in
    // AccessBlock() in the complex data type wapping case.
    if( nBandDataSize == 0 )
        return CE_Failure;
#endif
    const int nBufDataSize = GDALGetDataTypeSizeBytes(eBufType);

    if( !CanUseDirectIO(nXOff, nYOff, nXSize, nYSize, eBufType) )
    {
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff,
                                         nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize,
                                         eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg);
    }

    CPLDebug("RAW", "Using direct IO implementation");

    // Read data.
    if ( eRWFlag == GF_Read )
    {
        // Do we have overviews that are appropriate to satisfy this request?
        if( (nBufXSize < nXSize || nBufYSize < nYSize)
            && GetOverviewCount() > 0 )
        {
            if( OverviewRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize,
                                 eBufType, nPixelSpace, nLineSpace,
                                 psExtraArg) == CE_None)
                return CE_None;
        }

        // 1. Simplest case when we should get contiguous block
        //    of uninterleaved pixels.
        if ( nXSize == GetXSize()
             && nXSize == nBufXSize
             && nYSize == nBufYSize
             && eBufType == eDataType
             && nPixelOffset == nBandDataSize
             && nPixelSpace == nBufDataSize
             && nLineSpace == nPixelSpace * nXSize )
        {
            const vsi_l_offset nOffset =
                nImgOffset + static_cast<vsi_l_offset>(nYOff) * nLineOffset +
                nXOff;
            const size_t nBytesToRead =
                static_cast<size_t>(nXSize) * nYSize * nBandDataSize;
            if ( AccessBlock(nOffset, nBytesToRead, pData) != CE_None )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed to read " CPL_FRMT_GUIB
                         " bytes at " CPL_FRMT_GUIB ".",
                         static_cast<GUIntBig>(nBytesToRead), nOffset);
                return CE_Failure;
            }
        }
        // 2. Case when we need deinterleave and/or subsample data.
        else
        {
            const double dfSrcXInc = static_cast<double>(nXSize) / nBufXSize;
            const double dfSrcYInc = static_cast<double>(nYSize) / nBufYSize;

            const size_t nBytesToRW =
                static_cast<size_t>(nPixelOffset) * nXSize;
            GByte *pabyData =
                static_cast<GByte *>(VSI_MALLOC_VERBOSE(nBytesToRW));
            if( pabyData == NULL )
                return CE_Failure;

            for ( int iLine = 0; iLine < nBufYSize; iLine++ )
            {
                const vsi_l_offset nOffset =
                    nImgOffset +
                    ((static_cast<vsi_l_offset>(nYOff) +
                      static_cast<vsi_l_offset>(iLine * dfSrcYInc)) *
                     nLineOffset) +
                    nXOff * nPixelOffset;
                if ( AccessBlock(nOffset,
                                 nBytesToRW, pabyData) != CE_None )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Failed to read " CPL_FRMT_GUIB
                             " bytes at " CPL_FRMT_GUIB ".",
                             static_cast<GUIntBig>(nBytesToRW), nOffset);
                    CPLFree(pabyData);
                    return CE_Failure;
                }
                // Copy data from disk buffer to user block buffer and
                // subsample, if needed.
                if ( nXSize == nBufXSize && nYSize == nBufYSize )
                {
                    GDALCopyWords(
                        pabyData, eDataType, nPixelOffset,
                        static_cast<GByte *>(pData) +
                            static_cast<vsi_l_offset>(iLine) * nLineSpace,
                        eBufType, static_cast<int>(nPixelSpace), nXSize);
                }
                else
                {
                    for ( int iPixel = 0; iPixel < nBufXSize; iPixel++ )
                    {
                        GDALCopyWords(
                            pabyData +
                                static_cast<vsi_l_offset>(iPixel * dfSrcXInc) *
                                    nPixelOffset,
                            eDataType, nPixelOffset,
                            static_cast<GByte *>(pData) +
                                static_cast<vsi_l_offset>(iLine) * nLineSpace +
                                static_cast<vsi_l_offset>(iPixel) * nPixelSpace,
                            eBufType, static_cast<int>(nPixelSpace), 1);
                    }
                }

                if( psExtraArg->pfnProgress != NULL &&
                    !psExtraArg->pfnProgress(1.0 * (iLine + 1) / nBufYSize, "",
                                            psExtraArg->pProgressData) )
                {
                    CPLFree(pabyData);
                    return CE_Failure;
                }
            }

            CPLFree(pabyData);
        }
    }
    // Write data.
    else
    {
        // 1. Simplest case when we should write contiguous block of
        //    uninterleaved pixels.
        if ( nXSize == GetXSize()
             && nXSize == nBufXSize
             && nYSize == nBufYSize
             && eBufType == eDataType
             && nPixelOffset == nBandDataSize
             && nPixelSpace == nBufDataSize
             && nLineSpace == nPixelSpace * nXSize )
        {
            // Byte swap the data buffer, if required.
            if( !bNativeOrder && eDataType != GDT_Byte )
            {
                if( GDALDataTypeIsComplex(eDataType) )
                {
                    const int nWordSize = GDALGetDataTypeSize(eDataType) / 16;
                    GDALSwapWords(pData, nWordSize, nXSize, nPixelOffset);
                    GDALSwapWords(static_cast<GByte *>(pData) + nWordSize,
                                  nWordSize, nXSize, nPixelOffset);
                }
                else
                {
                    GDALSwapWords(pData, nBandDataSize, nXSize, nPixelOffset);
                }
            }

            // Seek to the correct block.
            const vsi_l_offset nOffset =
                nImgOffset + static_cast<vsi_l_offset>(nYOff) * nLineOffset +
                nXOff;
            if( Seek(nOffset, SEEK_SET) == -1 )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed to seek to " CPL_FRMT_GUIB " to write data.",
                         nOffset);

                return CE_Failure;
            }

            // Write the block.
            const size_t nBytesToRW =
                static_cast<size_t>(nXSize) * nYSize * nBandDataSize;

            const size_t nBytesActuallyWritten = Write(pData, 1, nBytesToRW);
            if( nBytesActuallyWritten < nBytesToRW )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed to write " CPL_FRMT_GUIB
                         " bytes to file. " CPL_FRMT_GUIB " bytes written",
                         static_cast<GUIntBig>(nBytesToRW),
                         static_cast<GUIntBig>(nBytesActuallyWritten));

                return CE_Failure;
            }

            // Byte swap (if necessary) back into machine order so the
            // buffer is still usable for reading purposes.
            if( !bNativeOrder  && eDataType != GDT_Byte )
            {
                if( GDALDataTypeIsComplex(eDataType) )
                {
                    const int nWordSize = GDALGetDataTypeSize(eDataType) / 16;
                    GDALSwapWords(pData, nWordSize, nXSize, nPixelOffset);
                    GDALSwapWords(static_cast<GByte *>(pData) + nWordSize,
                                  nWordSize, nXSize, nPixelOffset);
                }
                else
                {
                    GDALSwapWords(pData, nBandDataSize, nXSize, nPixelOffset);
                }
            }
        }
        // 2. Case when we need deinterleave and/or subsample data.
        else
        {
            const double dfSrcXInc = static_cast<double>(nXSize) / nBufXSize;
            const double dfSrcYInc = static_cast<double>(nYSize) / nBufYSize;

            const size_t nBytesToRW =
                static_cast<size_t>(nPixelOffset) * nXSize;
            GByte *pabyData =
                static_cast<GByte *>(VSI_MALLOC_VERBOSE(nBytesToRW));
            if( pabyData == NULL )
                return CE_Failure;

            for ( int iLine = 0; iLine < nBufYSize; iLine++ )
            {
                const vsi_l_offset nBlockOff =
                    nImgOffset +
                    (static_cast<vsi_l_offset>(nYOff) +
                     static_cast<vsi_l_offset>(iLine * dfSrcYInc)) *
                        nLineOffset +
                    static_cast<vsi_l_offset>(nXOff) * nPixelOffset;

                // If the data for this band is completely contiguous we don't
                // have to worry about pre-reading from disk.
                if( nPixelOffset > nBandDataSize )
                    AccessBlock(nBlockOff, nBytesToRW, pabyData);

                // Copy data from user block buffer to disk buffer and
                // subsample, if needed.
                if ( nXSize == nBufXSize && nYSize == nBufYSize )
                {
                    GDALCopyWords(static_cast<GByte *>(pData) +
                                      static_cast<vsi_l_offset>(iLine) *
                                          nLineSpace,
                                  eBufType, static_cast<int>(nPixelSpace),
                                  pabyData, eDataType, nPixelOffset, nXSize);
                }
                else
                {
                    for ( int iPixel = 0; iPixel < nBufXSize; iPixel++ )
                    {
                        GDALCopyWords(
                            static_cast<GByte *>(pData) +
                                static_cast<vsi_l_offset>(iLine) * nLineSpace +
                                static_cast<vsi_l_offset>(iPixel) * nPixelSpace,
                            eBufType, static_cast<int>(nPixelSpace),
                            pabyData +
                                static_cast<vsi_l_offset>(iPixel * dfSrcXInc) *
                                    nPixelOffset,
                            eDataType, nPixelOffset, 1);
                    }
                }

                // Byte swap the data buffer, if required.
                if( !bNativeOrder && eDataType != GDT_Byte )
                {
                    if( GDALDataTypeIsComplex(eDataType) )
                    {
                        const int nWordSize =
                            GDALGetDataTypeSize(eDataType) / 16;
                        GDALSwapWords(pabyData, nWordSize, nXSize,
                                      nPixelOffset);
                        GDALSwapWords(static_cast<GByte *>(pabyData) +
                                          nWordSize,
                                      nWordSize, nXSize, nPixelOffset);
                    }
                    else
                    {
                        GDALSwapWords(pabyData, nBandDataSize, nXSize,
                                      nPixelOffset);
                    }
                }

                // Seek to the right line in block.
                if( Seek(nBlockOff, SEEK_SET) == -1 )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Failed to seek to " CPL_FRMT_GUIB " to read.",
                             nBlockOff);
                    CPLFree(pabyData);
                    return CE_Failure;
                }

                // Write the line of block.
                const size_t nBytesActuallyWritten =
                    Write(pabyData, 1, nBytesToRW);
                if( nBytesActuallyWritten < nBytesToRW )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Failed to write " CPL_FRMT_GUIB
                             " bytes to file. " CPL_FRMT_GUIB " bytes written",
                             static_cast<GUIntBig>(nBytesToRW),
                             static_cast<GUIntBig>(nBytesActuallyWritten));
                    CPLFree(pabyData);
                    return CE_Failure;
                }


                // Byte swap (if necessary) back into machine order so the
                // buffer is still usable for reading purposes.
                if( !bNativeOrder && eDataType != GDT_Byte )
                {
                    if( GDALDataTypeIsComplex(eDataType) )
                    {
                        const int nWordSize =
                            GDALGetDataTypeSize(eDataType) / 16;
                        GDALSwapWords(pabyData, nWordSize, nXSize,
                                      nPixelOffset);
                        GDALSwapWords(static_cast<GByte *>(pabyData) +
                                          nWordSize,
                                      nWordSize, nXSize, nPixelOffset);
                    }
                    else
                    {
                        GDALSwapWords(pabyData, nBandDataSize, nXSize,
                                      nPixelOffset);
                    }
                }
            }

            bDirty = TRUE;
            CPLFree(pabyData);
        }
    }

    return CE_None;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int RawRasterBand::Seek( vsi_l_offset nOffset, int nSeekMode )

{
    if( bIsVSIL )
        return VSIFSeekL(fpRawL, nOffset, nSeekMode);

    return VSIFSeek(fpRaw, static_cast<long>(nOffset), nSeekMode);
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t RawRasterBand::Read( void *pBuffer, size_t nSize, size_t nCount )

{
    if( bIsVSIL )
        return VSIFReadL(pBuffer, nSize, nCount, fpRawL);

    return VSIFRead(pBuffer, nSize, nCount, fpRaw);
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t RawRasterBand::Write( void *pBuffer, size_t nSize, size_t nCount )

{
    if( bIsVSIL )
        return VSIFWriteL(pBuffer, nSize, nCount, fpRawL);

    return VSIFWrite(pBuffer, nSize, nCount, fpRaw);
}

/************************************************************************/
/*                          StoreNoDataValue()                          */
/*                                                                      */
/*      This is a helper function for datasets to associate a no        */
/*      data value with this band, it isn't intended to be called by    */
/*      applications.                                                   */
/************************************************************************/

void RawRasterBand::StoreNoDataValue( double dfValue )

{
    SetNoDataValue(dfValue);
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **RawRasterBand::GetCategoryNames() { return papszCategoryNames; }

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr RawRasterBand::SetCategoryNames( char **papszNewNames )

{
    CSLDestroy(papszCategoryNames);
    papszCategoryNames = CSLDuplicate(papszNewNames);

    return CE_None;
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

CPLErr RawRasterBand::SetColorTable( GDALColorTable *poNewCT )

{
    if( poCT )
        delete poCT;
    if( poNewCT == NULL )
        poCT = NULL;
    else
        poCT = poNewCT->Clone();

    return CE_None;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *RawRasterBand::GetColorTable() { return poCT; }

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

CPLErr RawRasterBand::SetColorInterpretation( GDALColorInterp eNewInterp )

{
    eInterp = eNewInterp;

    return CE_None;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp RawRasterBand::GetColorInterpretation() { return eInterp; }

/************************************************************************/
/*                           GetVirtualMemAuto()                        */
/************************************************************************/

CPLVirtualMem  *RawRasterBand::GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                                  int *pnPixelSpace,
                                                  GIntBig *pnLineSpace,
                                                  char **papszOptions )
{
    CPLAssert(pnPixelSpace);
    CPLAssert(pnLineSpace);

    const vsi_l_offset nSize =
        static_cast<vsi_l_offset>(nRasterYSize - 1) * nLineOffset +
        (nRasterXSize - 1) * nPixelOffset + GDALGetDataTypeSizeBytes(eDataType);

    const char *pszImpl = CSLFetchNameValueDef(
            papszOptions, "USE_DEFAULT_IMPLEMENTATION", "AUTO");
    if( !bIsVSIL || VSIFGetNativeFileDescriptorL(fpRawL) == NULL ||
        !CPLIsVirtualMemFileMapAvailable() ||
        (eDataType != GDT_Byte && !bNativeOrder) ||
        static_cast<size_t>(nSize) != nSize ||
        nPixelOffset < 0 ||
        nLineOffset < 0 ||
        EQUAL(pszImpl, "YES") || EQUAL(pszImpl, "ON") ||
        EQUAL(pszImpl, "1") || EQUAL(pszImpl, "TRUE") )
    {
        return GDALRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace,
                                                 pnLineSpace, papszOptions);
    }

    FlushCache();

    CPLVirtualMem *pVMem = CPLVirtualMemFileMapNew(
        fpRawL, nImgOffset, nSize,
        (eRWFlag == GF_Write) ? VIRTUALMEM_READWRITE : VIRTUALMEM_READONLY,
        NULL, NULL);
    if( pVMem == NULL )
    {
        if( EQUAL(pszImpl, "NO") || EQUAL(pszImpl, "OFF") ||
            EQUAL(pszImpl, "0") || EQUAL(pszImpl, "FALSE") )
        {
            return NULL;
        }
        return GDALRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace,
                                                 pnLineSpace, papszOptions);
    }

    *pnPixelSpace = nPixelOffset;
    *pnLineSpace = nLineOffset;
    return pVMem;
}

/************************************************************************/
/* ==================================================================== */
/*      RawDataset                                                      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            RawDataset()                              */
/************************************************************************/

RawDataset::RawDataset() {}

/************************************************************************/
/*                           ~RawDataset()                              */
/************************************************************************/

// It's pure virtual function but must be defined, even if empty.
RawDataset::~RawDataset() {}

/************************************************************************/
/*                             IRasterIO()                              */
/*                                                                      */
/*      Multi-band raster io handler.                                   */
/************************************************************************/

CPLErr RawDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg )

{
    const char* pszInterleave = NULL;

    // The default GDALDataset::IRasterIO() implementation would go to
    // BlockBasedRasterIO if the dataset is interleaved. However if the
    // access pattern is compatible with DirectIO() we don't want to go
    // BlockBasedRasterIO, but rather used our optimized path in
    // RawRasterBand::IRasterIO().
    if (nXSize == nBufXSize && nYSize == nBufYSize && nBandCount > 1 &&
        (pszInterleave = GetMetadataItem("INTERLEAVE",
                                         "IMAGE_STRUCTURE")) != NULL &&
        EQUAL(pszInterleave, "PIXEL"))
    {
        int iBandIndex = 0;
        for( ; iBandIndex < nBandCount; iBandIndex++ )
        {
            RawRasterBand *poBand = static_cast<RawRasterBand *>(
                GetRasterBand(panBandMap[iBandIndex]));
            if( !poBand->CanUseDirectIO(nXOff, nYOff,
                                        nXSize, nYSize, eBufType) )
            {
                break;
            }
        }
        if( iBandIndex == nBandCount )
        {
            GDALProgressFunc pfnProgressGlobal = psExtraArg->pfnProgress;
            void *pProgressDataGlobal = psExtraArg->pProgressData;

            CPLErr eErr = CE_None;
            for( iBandIndex = 0;
                 iBandIndex < nBandCount && eErr == CE_None;
                 iBandIndex++ )
            {
                GDALRasterBand *poBand = GetRasterBand(panBandMap[iBandIndex]);

                if (poBand == NULL)
                {
                    eErr = CE_Failure;
                    break;
                }

                GByte *pabyBandData =
                    static_cast<GByte *>(pData) + iBandIndex * nBandSpace;

                psExtraArg->pfnProgress = GDALScaledProgress;
                psExtraArg->pProgressData = GDALCreateScaledProgress(
                    1.0 * iBandIndex / nBandCount,
                    1.0 * (iBandIndex + 1) / nBandCount, pfnProgressGlobal,
                    pProgressDataGlobal);

                eErr = poBand->RasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                        static_cast<void *>(pabyBandData),
                                        nBufXSize, nBufYSize, eBufType,
                                        nPixelSpace, nLineSpace, psExtraArg);

                GDALDestroyScaledProgress(psExtraArg->pProgressData);
            }

            psExtraArg->pfnProgress = pfnProgressGlobal;
            psExtraArg->pProgressData = pProgressDataGlobal;

            return eErr;
        }
    }

    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpace, nLineSpace,
                                  nBandSpace, psExtraArg);
}
