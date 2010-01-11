/******************************************************************************
 * $Id$
 *
 * Project:  Generic Raw Binary Driver
 * Purpose:  Implementation of RawDataset and RawRasterBand classes.
 * Author:   Frank Warmerdam, warmerda@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "rawdataset.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( GDALDataset *poDS, int nBand,
                              FILE * fpRaw, vsi_l_offset nImgOffset,
                              int nPixelOffset, int nLineOffset,
                              GDALDataType eDataType, int bNativeOrder,
                              int bIsVSIL, int bOwnsFP )

{
    Initialize();

    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = eDataType;
    this->bIsVSIL = bIsVSIL;
    this->bOwnsFP =bOwnsFP;

    this->fpRaw = fpRaw;
    this->nImgOffset = nImgOffset;
    this->nPixelOffset = nPixelOffset;
    this->nLineOffset = nLineOffset;
    this->bNativeOrder = bNativeOrder;

    CPLDebug( "GDALRaw", 
              "RawRasterBand(%p,%d,%p,\n"
              "              Off=%d,PixOff=%d,LineOff=%d,%s,%d)\n",
              poDS, nBand, fpRaw, 
              (unsigned int) nImgOffset, nPixelOffset, nLineOffset, 
              GDALGetDataTypeName(eDataType), bNativeOrder );

/* -------------------------------------------------------------------- */
/*      Treat one scanline as the block size.                           */
/* -------------------------------------------------------------------- */
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;

/* -------------------------------------------------------------------- */
/*      Allocate working scanline.                                      */
/* -------------------------------------------------------------------- */
    nLoadedScanline = -1;
    if (nPixelOffset <= 0 || nBlockXSize <= 0 || nPixelOffset > INT_MAX / nBlockXSize)
    {
        nLineSize = 0;
        pLineBuffer = NULL;
    }
    else
    {
        nLineSize = nPixelOffset * nBlockXSize;
        pLineBuffer = VSIMalloc2( nPixelOffset, nBlockXSize );
    }
    if (pLineBuffer == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not allocate line buffer : nPixelOffset=%d, nBlockXSize=%d",
                 nPixelOffset, nBlockXSize);
    }
}

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( FILE * fpRaw, vsi_l_offset nImgOffset,
                              int nPixelOffset, int nLineOffset,
                              GDALDataType eDataType, int bNativeOrder,
                              int nXSize, int nYSize, int bIsVSIL, int bOwnsFP )

{
    Initialize();

    this->poDS = NULL;
    this->nBand = 1;
    this->eDataType = eDataType;
    this->bIsVSIL = bIsVSIL;
    this->bOwnsFP =bOwnsFP;

    this->fpRaw = fpRaw;
    this->nImgOffset = nImgOffset;
    this->nPixelOffset = nPixelOffset;
    this->nLineOffset = nLineOffset;
    this->bNativeOrder = bNativeOrder;


    CPLDebug( "GDALRaw", 
              "RawRasterBand(floating,Off=%d,PixOff=%d,LineOff=%d,%s,%d)\n",
              (unsigned int) nImgOffset, nPixelOffset, nLineOffset, 
              GDALGetDataTypeName(eDataType), bNativeOrder );

/* -------------------------------------------------------------------- */
/*      Treat one scanline as the block size.                           */
/* -------------------------------------------------------------------- */
    nBlockXSize = nXSize;
    nBlockYSize = 1;
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    if (!GDALCheckDatasetDimensions(nXSize, nYSize))
    {
        pLineBuffer = NULL;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Allocate working scanline.                                      */
/* -------------------------------------------------------------------- */
    nLoadedScanline = -1;
    if (nPixelOffset <= 0 || nPixelOffset > INT_MAX / nBlockXSize)
    {
        nLineSize = 0;
        pLineBuffer = NULL;
    }
    else
    {
        nLineSize = nPixelOffset * nBlockXSize;
        pLineBuffer = VSIMalloc2( nPixelOffset, nBlockXSize );
    }
    if (pLineBuffer == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not allocate line buffer : nPixelOffset=%d, nBlockXSize=%d",
                 nPixelOffset, nBlockXSize);
    }
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
}


/************************************************************************/
/*                           ~RawRasterBand()                           */
/************************************************************************/

RawRasterBand::~RawRasterBand()

{
    if( poCT )
        delete poCT;

    CSLDestroy( papszCategoryNames );

    FlushCache();
    
    if (bOwnsFP)
    {
        if ( bIsVSIL )
            VSIFCloseL( fpRaw );
        else
            VSIFClose( fpRaw );
    }
    
    CPLFree( pLineBuffer );
}


/************************************************************************/
/*                             SetAccess()                              */
/************************************************************************/

void  RawRasterBand::SetAccess( GDALAccess eAccess )
{
    this->eAccess = eAccess;
}

/************************************************************************/
/*                             FlushCache()                             */
/*                                                                      */
/*      We override this so we have the opportunity to call             */
/*      fflush().  We don't want to do this all the time in the         */
/*      write block function as it is kind of expensive.                */
/************************************************************************/

CPLErr RawRasterBand::FlushCache()

{
    CPLErr eErr;

    eErr = GDALRasterBand::FlushCache();
    if( eErr != CE_None )
        return eErr;

    // If we have unflushed raw, flush it to disk now.
    if ( bDirty )
    {
        if( bIsVSIL )
            VSIFFlushL( fpRaw );
        else
            VSIFFlush( fpRaw );

        bDirty = FALSE;
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

/* -------------------------------------------------------------------- */
/*      Seek to the right line.                                         */
/* -------------------------------------------------------------------- */
    if( Seek(nImgOffset + (vsi_l_offset)iLine * nLineOffset, SEEK_SET) == -1 )
    {
        if (poDS != NULL && poDS->GetAccess() == GA_ReadOnly)
        {
            CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to scanline %d @ %d.\n",
                  iLine, (int) (nImgOffset + (vsi_l_offset)iLine * nLineOffset) );
            return CE_Failure;
        }
        else
        {
            memset( pLineBuffer, 0, nPixelOffset * nBlockXSize );
            nLoadedScanline = iLine;
            return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      Read the line.  Take care not to request any more bytes than    */
/*      are needed, and not to lose a partially successful scanline     */
/*      read.                                                           */
/* -------------------------------------------------------------------- */
    int	nBytesToRead, nBytesActuallyRead;

    nBytesToRead = nPixelOffset * (nBlockXSize - 1) 
        + GDALGetDataTypeSize(GetRasterDataType()) / 8;

    nBytesActuallyRead = Read( pLineBuffer, 1, nBytesToRead );
    if( nBytesActuallyRead < nBlockXSize )
    {
        if (poDS != NULL && poDS->GetAccess() == GA_ReadOnly)
        {
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read scanline %d.\n",
                      iLine);
            return CE_Failure;
        }
        else
        {
            memset( ((GByte *) pLineBuffer) + nBytesActuallyRead, 
                    0, nBytesToRead - nBytesActuallyRead );
        }
    }

/* -------------------------------------------------------------------- */
/*      Byte swap the interesting data, if required.                    */
/* -------------------------------------------------------------------- */
    if( !bNativeOrder && eDataType != GDT_Byte )
    {
        if( GDALDataTypeIsComplex( eDataType ) )
        {
            int nWordSize;

            nWordSize = GDALGetDataTypeSize(eDataType)/16;
            GDALSwapWords( pLineBuffer, nWordSize, nBlockXSize, nPixelOffset );
            GDALSwapWords( ((GByte *) pLineBuffer)+nWordSize, 
                           nWordSize, nBlockXSize, nPixelOffset );
        }
        else
            GDALSwapWords( pLineBuffer, GDALGetDataTypeSize(eDataType)/8,
                           nBlockXSize, nPixelOffset );
    }

    nLoadedScanline = iLine;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RawRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    CPLErr		eErr;

    CPLAssert( nBlockXOff == 0 );

    if (pLineBuffer == NULL)
        return CE_Failure;

    eErr = AccessLine( nBlockYOff );
    
/* -------------------------------------------------------------------- */
/*      Copy data from disk buffer to user block buffer.                */
/* -------------------------------------------------------------------- */
    GDALCopyWords( pLineBuffer, eDataType, nPixelOffset,
                   pImage, eDataType, GDALGetDataTypeSize(eDataType)/8,
                   nBlockXSize );

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr RawRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                   void * pImage )

{
    CPLErr		eErr = CE_None;

    CPLAssert( nBlockXOff == 0 );

    if (pLineBuffer == NULL)
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      If the data for this band is completely contiguous we don't     */
/*      have to worry about pre-reading from disk.                      */
/* -------------------------------------------------------------------- */
    if( nPixelOffset > GDALGetDataTypeSize(eDataType) / 8 )
        eErr = AccessLine( nBlockYOff );

/* -------------------------------------------------------------------- */
/*	Copy data from user buffer into disk buffer.                    */
/* -------------------------------------------------------------------- */
    GDALCopyWords( pImage, eDataType, GDALGetDataTypeSize(eDataType)/8,
                   pLineBuffer, eDataType, nPixelOffset,
                   nBlockXSize );

/* -------------------------------------------------------------------- */
/*      Byte swap (if necessary) back into disk order before writing.   */
/* -------------------------------------------------------------------- */
    if( !bNativeOrder && eDataType != GDT_Byte )
    {
        if( GDALDataTypeIsComplex( eDataType ) )
        {
            int nWordSize;

            nWordSize = GDALGetDataTypeSize(eDataType)/16;
            GDALSwapWords( pLineBuffer, nWordSize, nBlockXSize, nPixelOffset );
            GDALSwapWords( ((GByte *) pLineBuffer)+nWordSize, 
                           nWordSize, nBlockXSize, nPixelOffset );
        }
        else
            GDALSwapWords( pLineBuffer, GDALGetDataTypeSize(eDataType)/8,
                           nBlockXSize, nPixelOffset );
    }

/* -------------------------------------------------------------------- */
/*      Seek to correct location.                                       */
/* -------------------------------------------------------------------- */
    if( Seek( nImgOffset + (vsi_l_offset) nBlockYOff * nLineOffset,
              SEEK_SET ) == -1 ) 
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to scanline %d @ %d to write to file.\n",
                  nBlockYOff, (int) (nImgOffset + nBlockYOff * nLineOffset) );
        
        eErr = CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Write data buffer.                                              */
/* -------------------------------------------------------------------- */
    int	nBytesToWrite;

    nBytesToWrite = nPixelOffset * (nBlockXSize - 1) 
        + GDALGetDataTypeSize(GetRasterDataType()) / 8;

    if( eErr == CE_None 
        && Write( pLineBuffer, 1, nBytesToWrite ) < (size_t) nBytesToWrite )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write scanline %d to file.\n",
                  nBlockYOff );
        
        eErr = CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*      Byte swap (if necessary) back into machine order so the         */
/*      buffer is still usable for reading purposes.                    */
/* -------------------------------------------------------------------- */
    if( !bNativeOrder && eDataType != GDT_Byte )
    {
        if( GDALDataTypeIsComplex( eDataType ) )
        {
            int nWordSize;

            nWordSize = GDALGetDataTypeSize(eDataType)/16;
            GDALSwapWords( pLineBuffer, nWordSize, nBlockXSize, nPixelOffset );
            GDALSwapWords( ((GByte *) pLineBuffer)+nWordSize, 
                           nWordSize, nBlockXSize, nPixelOffset );
        }
        else
            GDALSwapWords( pLineBuffer, GDALGetDataTypeSize(eDataType)/8,
                           nBlockXSize, nPixelOffset );
    }

    bDirty = TRUE;
    return eErr;
}

/************************************************************************/
/*                             AccessBlock()                            */
/************************************************************************/

CPLErr RawRasterBand::AccessBlock( vsi_l_offset nBlockOff, int nBlockSize,
                                   void * pData )
{
    int         nBytesActuallyRead;

/* -------------------------------------------------------------------- */
/*      Seek to the right block.                                        */
/* -------------------------------------------------------------------- */
    if( Seek( nBlockOff, SEEK_SET ) == -1 )
    {
        memset( pData, 0, nBlockSize );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Read the block.                                                 */
/* -------------------------------------------------------------------- */
    nBytesActuallyRead = Read( pData, 1, nBlockSize );
    if( nBytesActuallyRead < nBlockSize )
    {

        memset( ((GByte *) pData) + nBytesActuallyRead, 
                0, nBlockSize - nBytesActuallyRead );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Byte swap the interesting data, if required.                    */
/* -------------------------------------------------------------------- */
    if( !bNativeOrder && eDataType != GDT_Byte )
    {
        if( GDALDataTypeIsComplex( eDataType ) )
        {
            int nWordSize;

            nWordSize = GDALGetDataTypeSize(eDataType)/16;
            GDALSwapWords( pData, nWordSize, nBlockSize / nPixelOffset,
                           nPixelOffset );
            GDALSwapWords( ((GByte *) pData) + nWordSize, 
                           nWordSize, nBlockSize / nPixelOffset, nPixelOffset );
        }
        else
            GDALSwapWords( pData, GDALGetDataTypeSize(eDataType) / 8,
                           nBlockSize / nPixelOffset, nPixelOffset );
    }

    return CE_None;
}

/************************************************************************/
/*                          IsLineLoaded()                              */
/*                                                                      */
/*  Check whether at least one scanline from the specified block of     */
/*  lines is cached.                                                    */
/************************************************************************/

int RawRasterBand::IsLineLoaded( int nLineOff, int nLines )
{
    int         iLine;

    for ( iLine = nLineOff; iLine < nLineOff + nLines; iLine++ )
    {
        GDALRasterBlock *poBlock = TryGetLockedBlockRef( 0, iLine );
        if( poBlock != NULL )
        {
            poBlock->DropLock();
            return TRUE;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr RawRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nPixelSpace, int nLineSpace )

{
    int         nBandDataSize = GDALGetDataTypeSize(eDataType) / 8;
    int         nBufDataSize = GDALGetDataTypeSize( eBufType ) / 8;
    int         nBytesToRW = nPixelOffset * nXSize;

/* -------------------------------------------------------------------- */
/* Use direct IO without caching if:                                    */
/*                                                                      */
/* GDAL_ONE_BIG_READ is enabled                                         */
/*                                                                      */
/* or                                                                   */
/*                                                                      */
/* the length of a scanline on disk is more than 50000 bytes, and the   */
/* width of the requested chunk is less than 40% of the whole scanline  */
/* and none of the requested scanlines are already in the cache.        */
/* -------------------------------------------------------------------- */
    if ( !CSLTestBoolean( CPLGetConfigOption( "GDAL_ONE_BIG_READ", "NO") ) )
         {
        if ( nLineSize < 50000
             || nBytesToRW > nLineSize / 5 * 2
             || IsLineLoaded( nYOff, nYSize ) )
        {

            return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff,
                                              nXSize, nYSize,
                                              pData, nBufXSize, nBufYSize,
                                              eBufType,
                                              nPixelSpace, nLineSpace );
        }
    }

/* ==================================================================== */
/*   Read data.                                                         */
/* ==================================================================== */
    if ( eRWFlag == GF_Read )
    {
/* -------------------------------------------------------------------- */
/*      Do we have overviews that would be appropriate to satisfy       */
/*      this request?                                                   */
/* -------------------------------------------------------------------- */
        if( (nBufXSize < nXSize || nBufYSize < nYSize)
            && GetOverviewCount() > 0 )
        {
            if( OverviewRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                  pData, nBufXSize, nBufYSize, 
                                  eBufType, nPixelSpace, nLineSpace ) == CE_None )
                return CE_None;
        }

/* ==================================================================== */
/*   1. Simplest case when we should get contiguous block               */
/*   of uninterleaved pixels.                                           */
/* ==================================================================== */
        if ( nXSize == GetXSize() 
             && nXSize == nBufXSize
             && nYSize == nBufYSize
             && eBufType == eDataType
             && nPixelOffset == nBandDataSize
             && nPixelSpace == nBufDataSize
             && nLineSpace == nPixelSpace * nXSize )
        {
            if ( AccessBlock( nImgOffset
                              + (vsi_l_offset)nYOff * nLineOffset + nXOff,
                              nXSize * nYSize * nBandDataSize, pData ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to read %d bytes at %lu.",
                          nXSize * nYSize * nBandDataSize,
                          (unsigned long)
                          (nImgOffset + (vsi_l_offset)nYOff * nLineOffset
                           + nXOff) );
            }
        }

/* ==================================================================== */
/*   2. Case when we need deinterleave and/or subsample data.           */
/* ==================================================================== */
        else
        {
            GByte   *pabyData;
            double  dfSrcXInc, dfSrcYInc;
            int     iLine;
            
            dfSrcXInc = (double)nXSize / nBufXSize;
            dfSrcYInc = (double)nYSize / nBufYSize;


            pabyData = (GByte *) CPLMalloc( nBytesToRW );

            for ( iLine = 0; iLine < nBufYSize; iLine++ )
            {
                if ( AccessBlock( nImgOffset
                                  + ((vsi_l_offset)nYOff
                                  + (int)(iLine * dfSrcYInc)) * nLineOffset
                                  + nXOff * nPixelOffset,
                                  nBytesToRW, pabyData ) != CE_None )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Failed to read %d bytes at %lu.",
                              nBytesToRW,
                              (unsigned long)(nImgOffset
                              + ((vsi_l_offset)nYOff
                              + (int)(iLine * dfSrcYInc)) * nLineOffset
                              + nXOff * nPixelOffset) );
                }

/* -------------------------------------------------------------------- */
/*      Copy data from disk buffer to user block buffer and subsample,  */
/*      if needed.                                                      */
/* -------------------------------------------------------------------- */
                if ( nXSize == nBufXSize && nYSize == nBufYSize )
                {
                    GDALCopyWords( pabyData, eDataType, nPixelOffset,
                                   (GByte *)pData + iLine * nLineSpace,
                                   eBufType, nPixelSpace, nXSize );
                }
                else
                {
                    int     iPixel;

                    for ( iPixel = 0; iPixel < nBufXSize; iPixel++ )
                    {
                        GDALCopyWords( pabyData +
                                       (int)(iPixel * dfSrcXInc) * nPixelOffset,
                                       eDataType, 0,
                                       (GByte *)pData + iLine * nLineSpace +
                                       iPixel * nBufDataSize,
                                       eBufType, nPixelSpace, 1 );
                    }
                }
            }

            CPLFree( pabyData );
        }
    }

/* ==================================================================== */
/*   Write data.                                                        */
/* ==================================================================== */
    else
    {
        int nBytesActuallyWritten;

/* ==================================================================== */
/*   1. Simplest case when we should write contiguous block             */
/*   of uninterleaved pixels.                                           */
/* ==================================================================== */
        if ( nXSize == GetXSize() 
             && nXSize == nBufXSize
             && nYSize == nBufYSize
             && eBufType == eDataType
             && nPixelOffset == nBandDataSize
             && nPixelSpace == nBufDataSize
             && nLineSpace == nPixelSpace * nXSize )
        {
/* -------------------------------------------------------------------- */
/*      Byte swap the data buffer, if required.                         */
/* -------------------------------------------------------------------- */
            if( !bNativeOrder && eDataType != GDT_Byte )
            {
                if( GDALDataTypeIsComplex( eDataType ) )
                {
                    int nWordSize;

                    nWordSize = GDALGetDataTypeSize(eDataType)/16;
                    GDALSwapWords( pData, nWordSize, nXSize, nPixelOffset );
                    GDALSwapWords( ((GByte *) pData) + nWordSize, 
                                   nWordSize, nXSize, nPixelOffset );
                }
                else
                    GDALSwapWords( pData, nBandDataSize, nXSize, nPixelOffset );
            }

/* -------------------------------------------------------------------- */
/*      Seek to the right block.                                        */
/* -------------------------------------------------------------------- */
            if( Seek( nImgOffset + (vsi_l_offset)nYOff * nLineOffset + nXOff,
                      SEEK_SET) == -1 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to seek to %lu to write data.\n",
                          (unsigned long)(nImgOffset + (vsi_l_offset)nYOff
                                          * nLineOffset + nXOff) );
        
                return CE_Failure;
            }

/* -------------------------------------------------------------------- */
/*      Write the block.                                                */
/* -------------------------------------------------------------------- */
            nBytesToRW = nXSize * nYSize * nBandDataSize;

            nBytesActuallyWritten = Write( pData, 1, nBytesToRW );
            if( nBytesActuallyWritten < nBytesToRW )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to write %d bytes to file. %d bytes written",
                          nBytesToRW, nBytesActuallyWritten );
        
                return CE_Failure;
            }

/* -------------------------------------------------------------------- */
/*      Byte swap (if necessary) back into machine order so the         */
/*      buffer is still usable for reading purposes.                    */
/* -------------------------------------------------------------------- */
            if( !bNativeOrder  && eDataType != GDT_Byte )
            {
                if( GDALDataTypeIsComplex( eDataType ) )
                {
                    int nWordSize;

                    nWordSize = GDALGetDataTypeSize(eDataType)/16;
                    GDALSwapWords( pData, nWordSize, nXSize, nPixelOffset );
                    GDALSwapWords( ((GByte *) pData) + nWordSize, 
                                   nWordSize, nXSize, nPixelOffset );
                }
                else
                    GDALSwapWords( pData, nBandDataSize, nXSize, nPixelOffset );
            }
        }

/* ==================================================================== */
/*   2. Case when we need deinterleave and/or subsample data.           */
/* ==================================================================== */
        else
        {
            GByte   *pabyData;
            double  dfSrcXInc, dfSrcYInc;
            vsi_l_offset nBlockOff;
            int     iLine;
            
            dfSrcXInc = (double)nXSize / nBufXSize;
            dfSrcYInc = (double)nYSize / nBufYSize;

            pabyData = (GByte *) CPLMalloc( nBytesToRW );

            for ( iLine = 0; iLine < nBufYSize; iLine++ )
            {
                nBlockOff = nImgOffset
                    + ((vsi_l_offset)nYOff + (int)(iLine*dfSrcYInc))*nLineOffset
                    + nXOff * nPixelOffset;

/* -------------------------------------------------------------------- */
/*      If the data for this band is completely contiguous we don't     */
/*      have to worry about pre-reading from disk.                      */
/* -------------------------------------------------------------------- */
                if( nPixelOffset > nBandDataSize )
                    AccessBlock( nBlockOff, nBytesToRW, pabyData );

/* -------------------------------------------------------------------- */
/*      Copy data from user block buffer to disk buffer and subsample,  */
/*      if needed.                                                      */
/* -------------------------------------------------------------------- */
                if ( nXSize == nBufXSize && nYSize == nBufYSize )
                {
                    GDALCopyWords( (GByte *)pData + iLine * nLineSpace,
                                   eBufType, nPixelSpace,
                                   pabyData, eDataType, nPixelOffset, nXSize );
                }
                else
                {
                    int     iPixel;

                    for ( iPixel = 0; iPixel < nBufXSize; iPixel++ )
                    {
                        GDALCopyWords( (GByte *)pData+iLine*nLineSpace +
                                       iPixel * nBufDataSize,
                                       eBufType, nPixelSpace,
                                       pabyData +
                                       (int)(iPixel * dfSrcXInc) * nPixelOffset,
                                       eDataType, 0, 1 );
                    }
                }

/* -------------------------------------------------------------------- */
/*      Byte swap the data buffer, if required.                         */
/* -------------------------------------------------------------------- */
                if( !bNativeOrder && eDataType != GDT_Byte )
                {
                    if( GDALDataTypeIsComplex( eDataType ) )
                    {
                        int nWordSize;

                        nWordSize = GDALGetDataTypeSize(eDataType)/16;
                        GDALSwapWords( pabyData, nWordSize, nXSize, nPixelOffset );
                        GDALSwapWords( ((GByte *) pabyData) + nWordSize, 
                                       nWordSize, nXSize, nPixelOffset );
                    }
                    else
                        GDALSwapWords( pabyData, nBandDataSize, nXSize,
                                       nPixelOffset );
                }

/* -------------------------------------------------------------------- */
/*      Seek to the right line in block.                                */
/* -------------------------------------------------------------------- */
                if( Seek( nBlockOff, SEEK_SET) == -1 )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Failed to seek to %ld to read.\n",
                              (long)nBlockOff );

                    return CE_Failure;
                }

/* -------------------------------------------------------------------- */
/*      Write the line of block.                                        */
/* -------------------------------------------------------------------- */
                nBytesActuallyWritten = Write( pabyData, 1, nBytesToRW );
                if( nBytesActuallyWritten < nBytesToRW )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Failed to write %d bytes to file. %d bytes written",
                              nBytesToRW, nBytesActuallyWritten );
            
                    return CE_Failure;
                }

/* -------------------------------------------------------------------- */
/*      Byte swap (if necessary) back into machine order so the         */
/*      buffer is still usable for reading purposes.                    */
/* -------------------------------------------------------------------- */
                if( !bNativeOrder && eDataType != GDT_Byte )
                {
                    if( GDALDataTypeIsComplex( eDataType ) )
                    {
                        int nWordSize;

                        nWordSize = GDALGetDataTypeSize(eDataType)/16;
                        GDALSwapWords( pabyData, nWordSize, nXSize, nPixelOffset );
                        GDALSwapWords( ((GByte *) pabyData) + nWordSize, 
                                       nWordSize, nXSize, nPixelOffset );
                    }
                    else
                        GDALSwapWords( pabyData, nBandDataSize, nXSize,
                                       nPixelOffset );
                }

            }

            bDirty = TRUE;
            CPLFree( pabyData );
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
        return VSIFSeekL( fpRaw, nOffset, nSeekMode );
    else
        return VSIFSeek( fpRaw, (long) nOffset, nSeekMode );
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t RawRasterBand::Read( void *pBuffer, size_t nSize, size_t nCount )

{
    if( bIsVSIL )
        return VSIFReadL( pBuffer, nSize, nCount, fpRaw );
    else
        return VSIFRead( pBuffer, nSize, nCount, fpRaw );
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t RawRasterBand::Write( void *pBuffer, size_t nSize, size_t nCount )

{
    if( bIsVSIL )
        return VSIFWriteL( pBuffer, nSize, nCount, fpRaw );
    else
        return VSIFWrite( pBuffer, nSize, nCount, fpRaw );
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
    SetNoDataValue( dfValue );
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

char **RawRasterBand::GetCategoryNames()

{
    return papszCategoryNames;
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

CPLErr RawRasterBand::SetCategoryNames( char ** papszNewNames )

{
    CSLDestroy( papszCategoryNames );
    papszCategoryNames = CSLDuplicate( papszNewNames );

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

GDALColorTable *RawRasterBand::GetColorTable()

{
    return poCT;
}

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

GDALColorInterp RawRasterBand::GetColorInterpretation()

{
    return eInterp;
}

/************************************************************************/
/* ==================================================================== */
/*      RawDataset                                                      */
/* ==================================================================== */
/************************************************************************/


/************************************************************************/
/*                            RawDataset()                              */
/************************************************************************/

RawDataset::RawDataset()

{
}

/************************************************************************/
/*                           ~RawDataset()                              */
/************************************************************************/

RawDataset::~RawDataset()

{
    // MUST BE EMPTY
}

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
                              int nPixelSpace, int nLineSpace, int nBandSpace )

{
/*    if( nBandCount > 1 )
        return GDALDataset::BlockBasedRasterIO( 
            eRWFlag, nXOff, nYOff, nXSize, nYSize,
            pData, nBufXSize, nBufYSize, eBufType, 
            nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace );
    else*/
        return 
            GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize, eBufType, 
                                    nBandCount, panBandMap, 
                                    nPixelSpace, nLineSpace, nBandSpace );
}


