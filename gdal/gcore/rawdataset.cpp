/******************************************************************************
 *
 * Project:  Generic Raw Binary Driver
 * Purpose:  Implementation of RawDataset and RawRasterBand classes.
 * Author:   Frank Warmerdam, warmerda@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_vax.h"
#include "rawdataset.h"

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>
#include <limits>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_virtualmem.h"
#include "cpl_vsi.h"
#include "cpl_safemaths.hpp"
#include "gdal.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( GDALDataset *poDSIn, int nBandIn,
                              VSILFILE *fpRawLIn, vsi_l_offset nImgOffsetIn,
                              int nPixelOffsetIn, int nLineOffsetIn,
                              GDALDataType eDataTypeIn,
                              int bNativeOrderIn,
                              OwnFP bOwnsFPIn ) :
    RawRasterBand(poDSIn, nBandIn, fpRawLIn,
                  nImgOffsetIn, nPixelOffsetIn, nLineOffsetIn,
                  eDataTypeIn,
#ifdef CPL_LSB
                  bNativeOrderIn ? ByteOrder::ORDER_LITTLE_ENDIAN : ByteOrder::ORDER_BIG_ENDIAN,
#else
                  bNativeOrderIn ? ByteOrder::ORDER_BIG_ENDIAN : ByteOrder::ORDER_LITTLE_ENDIAN,
#endif
                  bOwnsFPIn)
{
}

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( GDALDataset *poDSIn, int nBandIn,
                              VSILFILE *fpRawLIn, vsi_l_offset nImgOffsetIn,
                              int nPixelOffsetIn, int nLineOffsetIn,
                              GDALDataType eDataTypeIn,
                              ByteOrder eByteOrderIn,
                              OwnFP bOwnsFPIn ) :
    fpRawL(fpRawLIn),
    nImgOffset(nImgOffsetIn),
    nPixelOffset(nPixelOffsetIn),
    nLineOffset(nLineOffsetIn),
    eByteOrder(eByteOrderIn),
    bOwnsFP(bOwnsFPIn == OwnFP::YES)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDataTypeIn;
    nRasterXSize = poDSIn->GetRasterXSize();
    nRasterYSize = poDSIn->GetRasterYSize();

    CPLDebug("GDALRaw",
             "RawRasterBand(%p,%d,%p,\n"
             "              Off=%d,PixOff=%d,LineOff=%d,%s,%d)",
             poDS, nBand, fpRawL,
             static_cast<unsigned int>(nImgOffset), nPixelOffset, nLineOffset,
             GDALGetDataTypeName(eDataType), static_cast<int>(eByteOrder));

    // Treat one scanline as the block size.
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

    // Initialize other fields, and setup the line buffer.
    Initialize();
}

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( VSILFILE *fpRawLIn, vsi_l_offset nImgOffsetIn,
                              int nPixelOffsetIn, int nLineOffsetIn,
                              GDALDataType eDataTypeIn, int bNativeOrderIn,
                              int nXSize, int nYSize,
                              OwnFP bOwnsFPIn ) :
    RawRasterBand(fpRawLIn, nImgOffsetIn, nPixelOffsetIn, nLineOffsetIn,
                  eDataTypeIn,
#ifdef CPL_LSB
                  bNativeOrderIn ? ByteOrder::ORDER_LITTLE_ENDIAN : ByteOrder::ORDER_BIG_ENDIAN,
#else
                  bNativeOrderIn ? ByteOrder::ORDER_BIG_ENDIAN : ByteOrder::ORDER_LITTLE_ENDIAN,
#endif
                  nXSize, nYSize,
                  bOwnsFPIn)
{
}

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( VSILFILE *fpRawLIn, vsi_l_offset nImgOffsetIn,
                              int nPixelOffsetIn, int nLineOffsetIn,
                              GDALDataType eDataTypeIn,
                              ByteOrder eByteOrderIn,
                              int nXSize, int nYSize,
                              OwnFP bOwnsFPIn ) :
    fpRawL(fpRawLIn),
    nImgOffset(nImgOffsetIn),
    nPixelOffset(nPixelOffsetIn),
    nLineOffset(nLineOffsetIn),
    eByteOrder(eByteOrderIn),
    bOwnsFP(bOwnsFPIn == OwnFP::YES)
{
    poDS = nullptr;
    nBand = 1;
    eDataType = eDataTypeIn;

    CPLDebug("GDALRaw",
             "RawRasterBand(floating,Off=%d,PixOff=%d,LineOff=%d,%s,%d)",
             static_cast<unsigned int>(nImgOffset),
             nPixelOffset, nLineOffset,
             GDALGetDataTypeName(eDataType), static_cast<int>(eByteOrder));

    // Treat one scanline as the block size.
    nBlockXSize = nXSize;
    nBlockYSize = 1;
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    if (!GDALCheckDatasetDimensions(nXSize, nYSize))
    {
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
    vsi_l_offset nSmallestOffset = nImgOffset;
    vsi_l_offset nLargestOffset = nImgOffset;
    if( nLineOffset < 0 )
    {
        const auto nDelta = static_cast<vsi_l_offset>(
            -static_cast<GIntBig>(nLineOffset)) * (nRasterYSize - 1);
        if( nDelta > nImgOffset )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Inconsistent nLineOffset, nRasterYSize and nImgOffset");
            return;
        }
        nSmallestOffset -= nDelta;
    }
    else
    {
        if( nImgOffset > std::numeric_limits<vsi_l_offset>::max() -
                    static_cast<vsi_l_offset>(nLineOffset) * (nRasterYSize - 1) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Inconsistent nLineOffset, nRasterYSize and nImgOffset");
            return;
        }
        nLargestOffset += static_cast<vsi_l_offset>(nLineOffset) * (nRasterYSize - 1);
    }
    if( nPixelOffset < 0 )
    {
        if( static_cast<vsi_l_offset>(-static_cast<GIntBig>(nPixelOffset)) *
                                        (nRasterXSize - 1) > nSmallestOffset )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Inconsistent nPixelOffset, nRasterXSize and nImgOffset");
            return;
        }
    }
    else
    {
        if( nLargestOffset > std::numeric_limits<vsi_l_offset>::max() -
                    static_cast<vsi_l_offset>(nPixelOffset) * (nRasterXSize - 1) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Inconsistent nPixelOffset, nRasterXSize and nImgOffset");
            return;
        }
        nLargestOffset += static_cast<vsi_l_offset>(nPixelOffset) * (nRasterXSize - 1);
    }
    if( nLargestOffset > static_cast<vsi_l_offset>(GINTBIG_MAX) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too big largest offset");
        return;
    }


    const int nDTSize = GDALGetDataTypeSizeBytes(GetRasterDataType());

    // Allocate working scanline.
    const bool bIsBIP = IsBIP();
    if( bIsBIP )
    {
        if( nBand == 1 )
        {
            nLineSize = nPixelOffset * nBlockXSize;
            pLineBuffer = VSIMalloc(nLineSize);
        }
        else
        {
            // Band > 1 : share the same buffer as band 1
            pLineBuffer = nullptr;
            const auto poFirstBand = cpl::down_cast<RawRasterBand*>(poDS->GetRasterBand(1));
            if( poFirstBand->pLineBuffer != nullptr )
                pLineStart = static_cast<char *>(poFirstBand->pLineBuffer) + (nBand - 1) * nDTSize;
            return;
        }
    }
    else if (nBlockXSize <= 0 ||
        (nBlockXSize > 1 && std::abs(nPixelOffset) >
            std::numeric_limits<int>::max() / (nBlockXSize - 1)) ||
        std::abs(nPixelOffset) * (nBlockXSize - 1) >
            std::numeric_limits<int>::max() - nDTSize)
    {
        nLineSize = 0;
        pLineBuffer = nullptr;
    }
    else
    {
        nLineSize = std::abs(nPixelOffset) * (nBlockXSize - 1) + nDTSize;
        pLineBuffer = VSIMalloc(nLineSize);
    }

    if (pLineBuffer == nullptr)
    {
        nLineSize = 0;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not allocate line buffer: "
                 "nPixelOffset=%d, nBlockXSize=%d",
                 nPixelOffset, nBlockXSize);
        return;
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

    RawRasterBand::FlushCache(true);

    if (bOwnsFP)
    {
        if( VSIFCloseL(fpRawL) != 0 )
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
    }

    CPLFree(pLineBuffer);
}

/************************************************************************/
/*                              IsBIP()                                 */
/************************************************************************/

bool RawRasterBand::IsBIP() const
{
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    const bool bIsRawDataset = dynamic_cast<RawDataset*>(poDS) != nullptr;
    if( bIsRawDataset && nPixelOffset > nDTSize &&
        nLineOffset == static_cast<int64_t>(nPixelOffset) * nRasterXSize )
    {
        if( nBand == 1 )
        {
            return true;
        }
        const auto poFirstBand = dynamic_cast<RawRasterBand*>(poDS->GetRasterBand(1));
        if( poFirstBand &&
            eDataType == poFirstBand->eDataType &&
            eByteOrder == poFirstBand->eByteOrder &&
            nPixelOffset == poFirstBand->nPixelOffset &&
            nLineOffset == poFirstBand->nLineOffset &&
            nImgOffset == poFirstBand->nImgOffset + (nBand - 1) * nDTSize )
        {
            return true;
        }
    }
    return false;
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

CPLErr RawRasterBand::FlushCache(bool bAtClosing)

{
    CPLErr eErr = GDALRasterBand::FlushCache(bAtClosing);
    if( eErr != CE_None )
    {
        bNeedFileFlush = false;
        return eErr;
    }

    RawRasterBand* masterBand = this;
    if( nBand > 1 && poDS != nullptr && poDS->GetRasterCount() > 1 && IsBIP() )
    {
        // can't be null as IsBIP() checks that the first band is not null,
        // which could happen during dataset destruction.
        masterBand = cpl::down_cast<RawRasterBand *>(poDS->GetRasterBand(1));
    }

    if( !masterBand->FlushCurrentLine(false) )
    {
        masterBand->bNeedFileFlush = false;
        return CE_Failure;
    }

    // If we have unflushed raw, flush it to disk now.
    if ( masterBand->bNeedFileFlush )
    {
        int nRet = VSIFFlushL(fpRawL);

        masterBand->bNeedFileFlush = false;
        if( nRet < 0 )
            return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                      NeedsByteOrderChange()                          */
/************************************************************************/

bool RawRasterBand::NeedsByteOrderChange() const
{
#ifdef CPL_LSB
    return eDataType != GDT_Byte &&
           eByteOrder != RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
#else
    return eDataType != GDT_Byte &&
           eByteOrder != RawRasterBand::ByteOrder::ORDER_BIG_ENDIAN;
#endif
}

/************************************************************************/
/*                          DoByteSwap()                                */
/************************************************************************/

void RawRasterBand::DoByteSwap(void* pBuffer, size_t nValues, int nByteSkip, bool bDiskToCPU) const
{
    if( eByteOrder != RawRasterBand::ByteOrder::ORDER_VAX )
    {
        if( GDALDataTypeIsComplex(eDataType) )
        {
            const int nWordSize = GDALGetDataTypeSize(eDataType) / 16;
            GDALSwapWordsEx(pBuffer, nWordSize, nValues, nByteSkip);
            GDALSwapWordsEx(
                static_cast<GByte *>(pBuffer) + nWordSize,
                nWordSize, nValues, nByteSkip);
        }
        else
        {
            GDALSwapWordsEx(pBuffer, GDALGetDataTypeSizeBytes(eDataType),
                          nValues, nByteSkip);
        }
    }
    else if( eDataType == GDT_Float32 ||  eDataType == GDT_CFloat32 )
    {
        GByte* pPtr = static_cast<GByte *>(pBuffer);
        for( int k = 0; k < 2; k++ )
        {
            if( bDiskToCPU )
            {
                for( size_t i = 0; i < nValues; i++, pPtr += nByteSkip )
                {
                    CPLVaxToIEEEFloat(pPtr);
                }
            }
            else
            {
                for( size_t i = 0; i < nValues; i++, pPtr += nByteSkip )
                {
                    CPLIEEEToVaxFloat(pPtr);
                }
            }
            if( k == 0 && eDataType == GDT_CFloat32 )
                pPtr = static_cast<GByte *>(pBuffer) + sizeof(float);
            else
                break;
        }
    }
    else if( eDataType == GDT_Float64 || eDataType == GDT_CFloat64 )
    {
        GByte* pPtr = static_cast<GByte *>(pBuffer);
        for( int k = 0; k < 2; k++ )
        {
            if( bDiskToCPU )
            {
                for( size_t i = 0; i < nValues; i++, pPtr += nByteSkip )
                {
                    CPLVaxToIEEEDouble(pPtr);
                }
            }
            else
            {
                for( size_t i = 0; i < nValues; i++, pPtr += nByteSkip )
                {
                    CPLIEEEToVaxDouble(pPtr);
                }
            }
            if( k == 0 && eDataType == GDT_CFloat64 )
                pPtr = static_cast<GByte *>(pBuffer) + sizeof(double);
            else
                break;
        }
    }
}

/************************************************************************/
/*                         ComputeFileOffset()                          */
/************************************************************************/

vsi_l_offset RawRasterBand::ComputeFileOffset(int iLine) const
{
    // Write formulas such that unsigned int overflow doesn't occur
    vsi_l_offset nOffset = nImgOffset;
    if( nLineOffset >= 0 )
    {
        nOffset += static_cast<GUIntBig>(nLineOffset) * iLine;
    }
    else
    {
        nOffset -= static_cast<GUIntBig>(-static_cast<GIntBig>(nLineOffset)) * iLine;
    }
    if( nPixelOffset < 0 )
    {
        const GUIntBig nPixelOffsetToSubtract =
            static_cast<GUIntBig>(-static_cast<GIntBig>(nPixelOffset)) * (nBlockXSize - 1);
        nOffset -= nPixelOffsetToSubtract;
    }
    return nOffset;
}

/************************************************************************/
/*                             AccessLine()                             */
/************************************************************************/

CPLErr RawRasterBand::AccessLine( int iLine )

{
    if (pLineBuffer == nullptr)
    {
        if( nBand > 1 && pLineStart != nullptr )
        {
            // BIP interleaved
            auto poFirstBand = cpl::down_cast<RawRasterBand*>(poDS->GetRasterBand(1));
            CPLAssert(poFirstBand);
            return poFirstBand->AccessLine(iLine);
        }
        return CE_Failure;
    }

    if( nLoadedScanline == iLine )
    {
        return CE_None;
    }

    if( !FlushCurrentLine(false) )
    {
        return CE_Failure;
    }

    // Figure out where to start reading.
    const vsi_l_offset nReadStart = ComputeFileOffset(iLine);

    // Seek to the correct line.
    if( Seek(nReadStart, SEEK_SET) == -1 )
    {
        if (poDS != nullptr && poDS->GetAccess() == GA_ReadOnly)
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
    const size_t nBytesToRead = nLineSize;
    const size_t nBytesActuallyRead = Read(pLineBuffer, 1, nBytesToRead);
    if( nBytesActuallyRead < nBytesToRead )
    {
        if (poDS != nullptr && poDS->GetAccess() == GA_ReadOnly &&
            // ENVI datasets might be sparse (see #915)
            poDS->GetMetadata("ENVI") == nullptr)
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
    if( NeedsByteOrderChange() )
    {
        if( poDS != nullptr && poDS->GetRasterCount() > 1 && IsBIP() )
        {
            const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
            DoByteSwap(pLineBuffer, nBlockXSize * poDS->GetRasterCount(), nDTSize, true);
        }
        else
            DoByteSwap(pLineBuffer, nBlockXSize, std::abs(nPixelOffset), true);
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

    const CPLErr eErr = AccessLine(nBlockYOff);
    if( eErr == CE_Failure )
        return eErr;

    // Copy data from disk buffer to user block buffer.
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    GDALCopyWords(pLineStart, eDataType, nPixelOffset,
                  pImage, eDataType, nDTSize,
                  nBlockXSize);

    // Pre-cache block cache of other bands
    if( poDS != nullptr && poDS->GetRasterCount() > 1 && IsBIP() )
    {
        for( int iBand = 1; iBand <= poDS->GetRasterCount(); iBand++ )
        {
            if( iBand != nBand )
            {
                auto poOtherBand = cpl::down_cast<RawRasterBand*>(poDS->GetRasterBand(iBand));
                GDALRasterBlock *poBlock = poOtherBand->TryGetLockedBlockRef(0, nBlockYOff);
                if( poBlock != nullptr )
                {
                    poBlock->DropLock();
                    continue;
                }
                poBlock = poOtherBand->GetLockedBlockRef(0, nBlockYOff, true);
                if( poBlock != nullptr )
                {
                    GDALCopyWords(poOtherBand->pLineStart, eDataType, nPixelOffset,
                                  poBlock->GetDataRef(), eDataType, nDTSize,
                                  nBlockXSize);
                    poBlock->DropLock();
                }
            }
        }
    }

    return eErr;
}

/************************************************************************/
/*                           BIPWriteBlock()                            */
/************************************************************************/

CPLErr RawRasterBand::BIPWriteBlock( int nBlockYOff,
                                     int nCallingBand,
                                     const void* pImage )
{
    if( nLoadedScanline != nBlockYOff )
    {
        if( !FlushCurrentLine(false) )
            return CE_Failure;
    }

    const int nBands = poDS->GetRasterCount();
    std::vector<GDALRasterBlock*> apoBlocks(nBands);
    bool bAllBlocksDirty = true;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);

/* -------------------------------------------------------------------- */
/*     If all blocks are cached and dirty then we do not need to reload */
/*     the scanline from disk                                           */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; ++iBand )
    {
        if( iBand + 1 != nCallingBand )
        {
            apoBlocks[iBand] =
                cpl::down_cast<RawRasterBand *>(
                    poDS->GetRasterBand( iBand + 1 ))
                        ->TryGetLockedBlockRef( 0, nBlockYOff );

            if( apoBlocks[iBand] == nullptr )
            {
                bAllBlocksDirty = false;
            }
            else if( !apoBlocks[iBand]->GetDirty() )
            {
                apoBlocks[iBand]->DropLock();
                apoBlocks[iBand] = nullptr;
                bAllBlocksDirty = false;
            }
        }
        else
            apoBlocks[iBand] = nullptr;
    }

    if( !bAllBlocksDirty )
    {
        // We only to read the scanline if we don't have data for all bands.
        if( AccessLine(nBlockYOff) != CE_None )
        {
            for( int iBand = 0; iBand < nBands; ++iBand )
            {
                if( apoBlocks[iBand] != nullptr )
                    apoBlocks[iBand]->DropLock();
            }
            return CE_Failure;
        }
    }

    for( int iBand = 0; iBand < nBands; ++iBand )
    {
        const GByte *pabyThisImage = nullptr;
        GDALRasterBlock *poBlock = nullptr;

        if( iBand + 1 == nCallingBand )
        {
            pabyThisImage = static_cast<const GByte *>( pImage );
        }
        else
        {
            poBlock = apoBlocks[iBand];
            if( poBlock == nullptr )
                continue;

            if( !poBlock->GetDirty() )
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = static_cast<const GByte *>( poBlock->GetDataRef() );
        }

        GByte *pabyOut = static_cast<GByte *>(pLineStart) + iBand * nDTSize;

        GDALCopyWords(pabyThisImage, eDataType, nDTSize,
                      pabyOut, eDataType, nPixelOffset, nBlockXSize);

        if( poBlock != nullptr )
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    nLoadedScanline = nBlockYOff;
    bLoadedScanlineDirty = true;

    if( bAllBlocksDirty )
    {
        return FlushCurrentLine(true) ? CE_None : CE_Failure;
    }

    bNeedFileFlush = true;
    return CE_None;
}


/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr RawRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff,
                                   int nBlockYOff,
                                   void *pImage )
{
    CPLAssert(nBlockXOff == 0);

    if (pLineBuffer == nullptr)
    {
        if( poDS != nullptr && poDS->GetRasterCount() > 1 && IsBIP() )
        {
            auto poFirstBand = (nBand == 1) ? this :
                cpl::down_cast<RawRasterBand *>(poDS->GetRasterBand(1));
            CPLAssert(poFirstBand);
            return poFirstBand->BIPWriteBlock(nBlockYOff, nBand, pImage);
        }

        return CE_Failure;
    }

    if( nLoadedScanline != nBlockYOff )
    {
        if( !FlushCurrentLine(false) )
            return CE_Failure;
    }

    // If the data for this band is completely contiguous, we don't
    // have to worry about pre-reading from disk.
    CPLErr eErr = CE_None;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    if( std::abs(nPixelOffset) > nDTSize )
        eErr = AccessLine(nBlockYOff);

    // Copy data from user buffer into disk buffer.
    GDALCopyWords(pImage, eDataType, nDTSize,
                  pLineStart, eDataType, nPixelOffset, nBlockXSize);

    nLoadedScanline = nBlockYOff;
    bLoadedScanlineDirty = true;

    return eErr == CE_None && FlushCurrentLine(true) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                         FlushCurrentLine()                           */
/************************************************************************/

bool RawRasterBand::FlushCurrentLine(bool bNeedUsableBufferAfter)
{
    if( !bLoadedScanlineDirty )
        return true;

    bLoadedScanlineDirty = false;

    bool ok = true;

    // Byte swap (if necessary) back into disk order before writing.
    if( NeedsByteOrderChange() )
    {
        if( poDS != nullptr && poDS->GetRasterCount() > 1 && IsBIP() )
        {
            const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
            DoByteSwap(pLineBuffer, nBlockXSize * poDS->GetRasterCount(), nDTSize, false);
        }
        else
            DoByteSwap(pLineBuffer, nBlockXSize, std::abs(nPixelOffset), false);
    }

    // Figure out where to start reading.
    const vsi_l_offset nWriteStart = ComputeFileOffset(nLoadedScanline);

    // Seek to correct location.
    if( Seek(nWriteStart, SEEK_SET) == -1 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to seek to scanline %d @ " CPL_FRMT_GUIB
                 " to write to file.",
                 nLoadedScanline, nWriteStart);

        ok = false;
    }

    // Write data buffer.
    const int nBytesToWrite = nLineSize;
    if( ok && Write(pLineBuffer, 1, nBytesToWrite) <
                                        static_cast<size_t>(nBytesToWrite) )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to write scanline %d to file.",
                 nLoadedScanline);

        ok = false;
    }

    // Byte swap (if necessary) back into machine order so the
    // buffer is still usable for reading purposes, unless this is not needed.
    if( bNeedUsableBufferAfter && NeedsByteOrderChange() )
    {
        if( poDS != nullptr && poDS->GetRasterCount() > 1 && IsBIP() )
        {
            const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
            DoByteSwap(pLineBuffer, nBlockXSize * poDS->GetRasterCount(), nDTSize, true);
        }
        else
            DoByteSwap(pLineBuffer, nBlockXSize, std::abs(nPixelOffset), true);
    }

    bNeedFileFlush = true;

    return ok;
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
    if( NeedsByteOrderChange() )
    {
        DoByteSwap(pData, nBlockSize / nPixelOffset, std::abs(nPixelOffset), true);
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
        if( poBlock != nullptr )
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
                                  GDALDataType /* eBufType*/,
                                  GDALRasterIOExtraArg* psExtraArg)
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

    if( nPixelOffset < 0 ||
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        return FALSE;
    }

    const char *pszGDAL_ONE_BIG_READ =
        CPLGetConfigOption("GDAL_ONE_BIG_READ", nullptr);
    if ( pszGDAL_ONE_BIG_READ == nullptr )
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

    if( !CanUseDirectIO(nXOff, nYOff, nXSize, nYSize, eBufType, psExtraArg) )
    {
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff,
                                         nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize,
                                         eBufType,
                                         nPixelSpace, nLineSpace, psExtraArg);
    }

    CPLDebug("RAW", "Using direct IO implementation");

    if (pLineBuffer == nullptr)
    {
        if( poDS != nullptr && poDS->GetRasterCount() > 1 && IsBIP() )
        {
            auto poFirstBand = (nBand == 1) ? this :
                cpl::down_cast<RawRasterBand *>(poDS->GetRasterBand(1));
            CPLAssert(poFirstBand);
            if( poFirstBand->bNeedFileFlush )
                RawRasterBand::FlushCache(false);
        }
    }
    if( bNeedFileFlush )
        RawRasterBand::FlushCache(false);

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
            vsi_l_offset nOffset = nImgOffset;
            if( nLineOffset >= 0 )
                nOffset += nYOff * nLineOffset;
            else
                nOffset -= nYOff * static_cast<vsi_l_offset>(-nLineOffset);

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
            if( pabyData == nullptr )
                return CE_Failure;

            for ( int iLine = 0; iLine < nBufYSize; iLine++ )
            {
                const vsi_l_offset nLine =
                    static_cast<vsi_l_offset>(nYOff) +
                      static_cast<vsi_l_offset>(iLine * dfSrcYInc);
                vsi_l_offset nOffset = nImgOffset;
                if( nLineOffset >= 0 )
                    nOffset += nLine * nLineOffset;
                else
                    nOffset -= nLine * static_cast<vsi_l_offset>(-nLineOffset);
                if( nPixelOffset >= 0 )
                    nOffset += nXOff * nPixelOffset;
                else
                    nOffset -= nXOff * static_cast<vsi_l_offset>(-nPixelOffset);
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

                if( psExtraArg->pfnProgress != nullptr &&
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
            if( NeedsByteOrderChange() )
            {
                DoByteSwap(pData, nXSize, std::abs(nPixelOffset), false);
            }

            // Seek to the correct block.
            vsi_l_offset nOffset = nImgOffset;
            if( nLineOffset >= 0 )
                nOffset += nYOff * nLineOffset;
            else
                nOffset -= nYOff * static_cast<vsi_l_offset>(-nLineOffset);

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
            if( NeedsByteOrderChange() )
            {
                DoByteSwap(pData, nXSize, std::abs(nPixelOffset), true);
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
            if( pabyData == nullptr )
                return CE_Failure;

            for ( int iLine = 0; iLine < nBufYSize; iLine++ )
            {
                const vsi_l_offset nLine =
                    static_cast<vsi_l_offset>(nYOff) +
                      static_cast<vsi_l_offset>(iLine * dfSrcYInc);
                vsi_l_offset nOffset = nImgOffset;
                if( nLineOffset >= 0 )
                    nOffset += nLine * nLineOffset;
                else
                    nOffset -= nLine * static_cast<vsi_l_offset>(-nLineOffset);
                if( nPixelOffset >= 0 )
                    nOffset += nXOff * nPixelOffset;
                else
                    nOffset -= nXOff * static_cast<vsi_l_offset>(-nPixelOffset);

                // If the data for this band is completely contiguous we don't
                // have to worry about pre-reading from disk.
                if( nPixelOffset > nBandDataSize )
                    AccessBlock(nOffset, nBytesToRW, pabyData);

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
                if( NeedsByteOrderChange() )
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
                if( Seek(nOffset, SEEK_SET) == -1 )
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Failed to seek to " CPL_FRMT_GUIB " to read.",
                             nOffset);
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
                if( NeedsByteOrderChange() )
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

            bNeedFileFlush = TRUE;
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
    return VSIFSeekL(fpRawL, nOffset, nSeekMode);
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t RawRasterBand::Read( void *pBuffer, size_t nSize, size_t nCount )

{
    return VSIFReadL(pBuffer, nSize, nCount, fpRawL);
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t RawRasterBand::Write( void *pBuffer, size_t nSize, size_t nCount )

{
    return VSIFWriteL(pBuffer, nSize, nCount, fpRawL);
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
    if( poNewCT == nullptr )
        poCT = nullptr;
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
    if( VSIFGetNativeFileDescriptorL(fpRawL) == nullptr ||
        !CPLIsVirtualMemFileMapAvailable() ||
        NeedsByteOrderChange() ||
        static_cast<size_t>(nSize) != nSize ||
        nPixelOffset < 0 ||
        nLineOffset < 0 ||
        EQUAL(pszImpl, "YES") || EQUAL(pszImpl, "ON") ||
        EQUAL(pszImpl, "1") || EQUAL(pszImpl, "TRUE") )
    {
        return GDALRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace,
                                                 pnLineSpace, papszOptions);
    }

    FlushCache(false);

    CPLVirtualMem *pVMem = CPLVirtualMemFileMapNew(
        fpRawL, nImgOffset, nSize,
        (eRWFlag == GF_Write) ? VIRTUALMEM_READWRITE : VIRTUALMEM_READONLY,
        nullptr, nullptr);
    if( pVMem == nullptr )
    {
        if( EQUAL(pszImpl, "NO") || EQUAL(pszImpl, "OFF") ||
            EQUAL(pszImpl, "0") || EQUAL(pszImpl, "FALSE") )
        {
            return nullptr;
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
    const char* pszInterleave = nullptr;

    // The default GDALDataset::IRasterIO() implementation would go to
    // BlockBasedRasterIO if the dataset is interleaved. However if the
    // access pattern is compatible with DirectIO() we don't want to go
    // BlockBasedRasterIO, but rather used our optimized path in
    // RawRasterBand::IRasterIO().
    if (nXSize == nBufXSize && nYSize == nBufYSize && nBandCount > 1 &&
        (pszInterleave = GetMetadataItem("INTERLEAVE",
                                         "IMAGE_STRUCTURE")) != nullptr &&
        EQUAL(pszInterleave, "PIXEL"))
    {
        int iBandIndex = 0;
        for( ; iBandIndex < nBandCount; iBandIndex++ )
        {
            RawRasterBand *poBand = dynamic_cast<RawRasterBand *>(
                GetRasterBand(panBandMap[iBandIndex]));
            if( poBand == nullptr ||
                !poBand->CanUseDirectIO(nXOff, nYOff,
                                        nXSize, nYSize, eBufType, psExtraArg) )
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

                if (poBand == nullptr)
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


/************************************************************************/
/*                  RAWDatasetCheckMemoryUsage()                        */
/************************************************************************/

bool RAWDatasetCheckMemoryUsage(int nXSize, int nYSize, int nBands,
                                int nDTSize,
                                int nPixelOffset,
                                int nLineOffset,
                                vsi_l_offset nHeaderSize,
                                vsi_l_offset nBandOffset,
                                VSILFILE* fp)
{
    const GIntBig nTotalBufferSize =
        nPixelOffset == static_cast<GIntBig>(nDTSize) * nBands ? // BIP ?
            static_cast<GIntBig>(nPixelOffset) * nXSize :
            static_cast<GIntBig>(std::abs(nPixelOffset)) * nXSize * nBands;

    // Currently each RawRasterBand allocates nPixelOffset * nRasterXSize bytes
    // so for a pixel interleaved scheme, this will allocate lots of memory!
    // Actually this is quadratic in the number of bands!
    // Do a few sanity checks to avoid excessive memory allocation on
    // small files.
    // But ultimately we should fix RawRasterBand to have a shared buffer
    // among bands.
    const char* pszCheck = CPLGetConfigOption("RAW_CHECK_FILE_SIZE", nullptr);
    if( (nBands > 10 || nTotalBufferSize > 20000 ||
         (pszCheck && CPLTestBool(pszCheck))) &&
        !(pszCheck && !CPLTestBool(pszCheck)) )
    {
        vsi_l_offset nExpectedFileSize;
        try
        {
            nExpectedFileSize =
                (CPLSM(static_cast<GUInt64>(nHeaderSize)) +
                CPLSM(static_cast<GUInt64>(nBandOffset)) * CPLSM(static_cast<GUInt64>(nBands - 1)) +
                (nLineOffset >= 0 ? CPLSM(static_cast<GUInt64>(nYSize-1)) * CPLSM(static_cast<GUInt64>(nLineOffset)) : CPLSM(static_cast<GUInt64>(0))) +
                (nPixelOffset >= 0 ? CPLSM(static_cast<GUInt64>(nXSize-1)) * CPLSM(static_cast<GUInt64>(nPixelOffset)) : CPLSM(static_cast<GUInt64>(0)))).v();
        }
        catch( ... )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Image file is too small");
            return false;
        }
        CPL_IGNORE_RET_VAL( VSIFSeekL(fp, 0, SEEK_END) );
        vsi_l_offset nFileSize = VSIFTellL(fp);
        // Do not strictly compare against nExpectedFileSize, but use an arbitrary
        // 50% margin, since some raw formats such as ENVI
        // allow for sparse files (see https://github.com/OSGeo/gdal/issues/915)
        if( nFileSize < nExpectedFileSize / 2 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Image file is too small");
            return false;
        }
    }

#if SIZEOF_VOIDP == 8
    const char* pszDefault = "1024";
#else
    const char* pszDefault = "512";
#endif
    constexpr int MB_IN_BYTES = 1024 * 1024;
    const GIntBig nMAX_BUFFER_MEM =
        static_cast<GIntBig>(atoi(CPLGetConfigOption("RAW_MEM_ALLOC_LIMIT_MB", pszDefault))) * MB_IN_BYTES;
    if( nTotalBufferSize > nMAX_BUFFER_MEM  )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 CPL_FRMT_GIB " MB of RAM would be needed to open the dataset. If you are "
                 "comfortable with this, you can set the RAW_MEM_ALLOC_LIMIT_MB "
                 "configuration option to that value or above",
                 (nTotalBufferSize + MB_IN_BYTES - 1) / MB_IN_BYTES);
        return false;
    }

    return true;
}

/************************************************************************/
/*                        GetRawBinaryLayout()                          */
/************************************************************************/

bool RawDataset::GetRawBinaryLayout(GDALDataset::RawBinaryLayout& sLayout)
{
    vsi_l_offset nImgOffset = 0;
    GIntBig nBandOffset = 0;
    int nPixelOffset = 0;
    int nLineOffset = 0;
    RawRasterBand::ByteOrder eByteOrder = RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
    GDALDataType eDT = GDT_Unknown;
    for( int i = 1; i <= nBands; i++ )
    {
        auto poBand = dynamic_cast<RawRasterBand*>(GetRasterBand(i));
        if( poBand == nullptr )
            return false;
        if( i == 1 )
        {
            nImgOffset = poBand->nImgOffset;
            nPixelOffset = poBand->nPixelOffset;
            nLineOffset = poBand->nLineOffset;
            eByteOrder = poBand->eByteOrder;
            if( eByteOrder == RawRasterBand::ByteOrder::ORDER_VAX )
                return false;
            eDT = poBand->GetRasterDataType();
        }
        else if( nPixelOffset != poBand->nPixelOffset ||
                 nLineOffset != poBand->nLineOffset ||
                 eByteOrder != poBand->eByteOrder ||
                 eDT != poBand->GetRasterDataType() )
        {
            return false;
        }
        else if( i == 2 )
        {
            nBandOffset = static_cast<GIntBig>(poBand->nImgOffset) -
                                static_cast<GIntBig>(nImgOffset);
        }
        else if( nBandOffset * (i - 1) !=
                    static_cast<GIntBig>(poBand->nImgOffset) -
                        static_cast<GIntBig>(nImgOffset) )
        {
            return false;
        }
    }

    sLayout.eInterleaving = RawBinaryLayout::Interleaving::UNKNOWN;
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    if( nBands > 1 )
    {
        if( nPixelOffset == nBands * nDTSize &&
            nLineOffset == nPixelOffset * nRasterXSize &&
            nBandOffset == nDTSize )
        {
            sLayout.eInterleaving = RawBinaryLayout::Interleaving::BIP;
        }
        else if( nPixelOffset == nDTSize &&
                 nLineOffset == nDTSize * nBands * nRasterXSize &&
                 nBandOffset == static_cast<GIntBig>(nDTSize) * nRasterXSize )
        {
            sLayout.eInterleaving = RawBinaryLayout::Interleaving::BIL;
        }
        else if( nPixelOffset == nDTSize &&
                 nLineOffset == nDTSize * nRasterXSize &&
                 nBandOffset == static_cast<GIntBig>(nLineOffset) * nRasterYSize )
        {
            sLayout.eInterleaving = RawBinaryLayout::Interleaving::BSQ;
        }
    }

    sLayout.eDataType = eDT;
    sLayout.bLittleEndianOrder = eByteOrder == RawRasterBand::ByteOrder::ORDER_LITTLE_ENDIAN;
    sLayout.nImageOffset = nImgOffset;
    sLayout.nPixelOffset = nPixelOffset;
    sLayout.nLineOffset = nLineOffset;
    sLayout.nBandOffset = nBandOffset;

    return true;
}
