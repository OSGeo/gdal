/******************************************************************************
 * $Id$
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

#include "rawdataset.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( GDALDataset *poDS, int nBand,
                              void * fpRaw, vsi_l_offset nImgOffset,
                              int nPixelOffset, int nLineOffset,
                              GDALDataType eDataType, int bNativeOrder,
                              int bIsVSIL, int bOwnsFP )

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->eDataType = eDataType;
    this->bIsVSIL = bIsVSIL;
    this->bOwnsFP =bOwnsFP;

    if (bIsVSIL)
    {
        this->fpRaw = NULL;
        this->fpRawL = (VSILFILE*) fpRaw;
    }
    else
    {
        this->fpRaw = (FILE*) fpRaw;
        this->fpRawL = NULL;
    }
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
/*      Initialize other fields, and setup the line buffer.             */
/* -------------------------------------------------------------------- */
    Initialize();
}

/************************************************************************/
/*                           RawRasterBand()                            */
/************************************************************************/

RawRasterBand::RawRasterBand( void * fpRaw, vsi_l_offset nImgOffset,
                              int nPixelOffset, int nLineOffset,
                              GDALDataType eDataType, int bNativeOrder,
                              int nXSize, int nYSize, int bIsVSIL, int bOwnsFP )

{
    this->poDS = NULL;
    this->nBand = 1;
    this->eDataType = eDataType;
    this->bIsVSIL = bIsVSIL;
    this->bOwnsFP =bOwnsFP;

    if (bIsVSIL)
    {
        this->fpRaw = NULL;
        this->fpRawL = (VSILFILE*) fpRaw;
    }
    else
    {
        this->fpRaw = (FILE*) fpRaw;
        this->fpRawL = NULL;
    }
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
/*      Initialize other fields, and setup the line buffer.             */
/* -------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------- */
/*      Allocate working scanline.                                      */
/* -------------------------------------------------------------------- */
    nLoadedScanline = -1;
    if (nBlockXSize <= 0 || nPixelOffset > INT_MAX / nBlockXSize)
    {
        nLineSize = 0;
        pLineBuffer = NULL;
    }
    else
    {
        nLineSize = ABS(nPixelOffset) * nBlockXSize;
        pLineBuffer = VSIMalloc2( ABS(nPixelOffset), nBlockXSize );
    }
    if (pLineBuffer == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Could not allocate line buffer : nPixelOffset=%d, nBlockXSize=%d",
                 nPixelOffset, nBlockXSize);
    }

    if( nPixelOffset >= 0 )
        pLineStart = pLineBuffer;
    else
        pLineStart = ((char *) pLineBuffer) + ABS(nPixelOffset) * (nBlockXSize-1);
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
            VSIFCloseL( fpRawL );
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
            VSIFFlushL( fpRawL );
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
/*      Figure out where to start reading.                              */
/* -------------------------------------------------------------------- */
    vsi_l_offset nReadStart;
    if( nPixelOffset >= 0 )
        nReadStart = nImgOffset + (vsi_l_offset)iLine * nLineOffset;
    else
    {
        nReadStart = nImgOffset + (vsi_l_offset)iLine * nLineOffset
            - ABS(nPixelOffset) * (nBlockXSize-1);
    }

/* -------------------------------------------------------------------- */
/*      Seek to the right line.                                         */
/* -------------------------------------------------------------------- */
    if( Seek(nReadStart, SEEK_SET) == -1 )
    {
        if (poDS != NULL && poDS->GetAccess() == GA_ReadOnly)
        {
            CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to scanline %d @ " CPL_FRMT_GUIB ".\n",
                  iLine, nImgOffset + (vsi_l_offset)iLine * nLineOffset );
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

    nBytesToRead = ABS(nPixelOffset) * (nBlockXSize - 1) 
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
            GDALSwapWords( pLineBuffer, nWordSize, nBlockXSize, ABS(nPixelOffset) );
            GDALSwapWords( ((GByte *) pLineBuffer)+nWordSize, 
                           nWordSize, nBlockXSize, ABS(nPixelOffset) );
        }
        else
            GDALSwapWords( pLineBuffer, GDALGetDataTypeSize(eDataType)/8,
                           nBlockXSize, ABS(nPixelOffset) );
    }

    nLoadedScanline = iLine;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RawRasterBand::IReadBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
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
    GDALCopyWords( pLineStart, eDataType, nPixelOffset,
                   pImage, eDataType, GDALGetDataTypeSize(eDataType)/8,
                   nBlockXSize );

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr RawRasterBand::IWriteBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff,
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
    if( ABS(nPixelOffset) > GDALGetDataTypeSize(eDataType) / 8 )
        eErr = AccessLine( nBlockYOff );

/* -------------------------------------------------------------------- */
/*	Copy data from user buffer into disk buffer.                    */
/* -------------------------------------------------------------------- */
    GDALCopyWords( pImage, eDataType, GDALGetDataTypeSize(eDataType)/8,
                   pLineStart, eDataType, nPixelOffset,
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
            GDALSwapWords( pLineBuffer, nWordSize, nBlockXSize, 
                           ABS(nPixelOffset) );
            GDALSwapWords( ((GByte *) pLineBuffer)+nWordSize, 
                           nWordSize, nBlockXSize, ABS(nPixelOffset) );
        }
        else
            GDALSwapWords( pLineBuffer, GDALGetDataTypeSize(eDataType)/8,
                           nBlockXSize, ABS(nPixelOffset) );
    }

/* -------------------------------------------------------------------- */
/*      Figure out where to start reading.                              */
/* -------------------------------------------------------------------- */
    vsi_l_offset nWriteStart;
    if( nPixelOffset >= 0 )
        nWriteStart = nImgOffset + (vsi_l_offset)nBlockYOff * nLineOffset;
    else
    {
        nWriteStart = nImgOffset + (vsi_l_offset)nBlockYOff * nLineOffset
            - ABS(nPixelOffset) * (nBlockXSize-1);
    }

/* -------------------------------------------------------------------- */
/*      Seek to correct location.                                       */
/* -------------------------------------------------------------------- */
    if( Seek( nWriteStart, SEEK_SET ) == -1 ) 
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to seek to scanline %d @ " CPL_FRMT_GUIB " to write to file.\n",
                  nBlockYOff, nImgOffset + nBlockYOff * nLineOffset );
        
        eErr = CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Write data buffer.                                              */
/* -------------------------------------------------------------------- */
    int	nBytesToWrite;

    nBytesToWrite = ABS(nPixelOffset) * (nBlockXSize - 1) 
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
            GDALSwapWords( pLineBuffer, nWordSize, nBlockXSize, 
                           ABS(nPixelOffset) );
            GDALSwapWords( ((GByte *) pLineBuffer)+nWordSize, 
                           nWordSize, nBlockXSize, 
                           ABS(nPixelOffset) );
        }
        else
            GDALSwapWords( pLineBuffer, GDALGetDataTypeSize(eDataType)/8,
                           nBlockXSize, ABS(nPixelOffset) );
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
/*               IsSignificantNumberOfLinesLoaded()                     */
/*                                                                      */
/*  Check if there is a significant number of scanlines (>20%) from the */
/*  specified block of lines already cached.                            */
/************************************************************************/

int RawRasterBand::IsSignificantNumberOfLinesLoaded( int nLineOff, int nLines )
{
    int         iLine;
    int         nCountLoaded = 0;

    for ( iLine = nLineOff; iLine < nLineOff + nLines; iLine++ )
    {
        GDALRasterBlock *poBlock = TryGetLockedBlockRef( 0, iLine );
        if( poBlock != NULL )
        {
            poBlock->DropLock();
            nCountLoaded ++;
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

int RawRasterBand::CanUseDirectIO(CPL_UNUSED int nXOff, int nYOff, int nXSize, int nYSize,
                                  CPL_UNUSED GDALDataType eBufType)
{

/* -------------------------------------------------------------------- */
/* Use direct IO without caching if:                                    */
/*                                                                      */
/* GDAL_ONE_BIG_READ is enabled                                         */
/*                                                                      */
/* or                                                                   */
/*                                                                      */
/* the length of a scanline on disk is more than 50000 bytes, and the   */
/* width of the requested chunk is less than 40% of the whole scanline  */
/* and no significant number of requested scanlines are already in the  */
/* cache.                                                               */
/* -------------------------------------------------------------------- */
    if( nPixelOffset < 0 ) 
    {
        return FALSE;
    }

    const char* pszGDAL_ONE_BIG_READ = CPLGetConfigOption( "GDAL_ONE_BIG_READ", NULL);
    if ( pszGDAL_ONE_BIG_READ == NULL )
    {
        int         nBytesToRW = nPixelOffset * nXSize;
        if ( nLineSize < 50000
             || nBytesToRW > nLineSize / 5 * 2
             || IsSignificantNumberOfLinesLoaded( nYOff, nYSize ) )
        {

            return FALSE;
        }
        return TRUE;
    }
    else
        return CSLTestBoolean(pszGDAL_ONE_BIG_READ);
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

    if( !CanUseDirectIO(nXOff, nYOff, nXSize, nYSize, eBufType ) )
    {
        return GDALRasterBand::IRasterIO( eRWFlag, nXOff, nYOff,
                                          nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize,
                                          eBufType,
                                          nPixelSpace, nLineSpace );
    }

    CPLDebug("RAW", "Using direct IO implementation");

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
            vsi_l_offset nOffset = nImgOffset
                              + (vsi_l_offset)nYOff * nLineOffset + nXOff;
            if ( AccessBlock( nOffset,
                              nXSize * nYSize * nBandDataSize, pData ) != CE_None )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to read %d bytes at " CPL_FRMT_GUIB ".",
                          nXSize * nYSize * nBandDataSize, nOffset);
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
                vsi_l_offset nOffset = nImgOffset
                                  + ((vsi_l_offset)nYOff
                                  + (vsi_l_offset)(iLine * dfSrcYInc)) * nLineOffset
                                  + nXOff * nPixelOffset;
                if ( AccessBlock( nOffset,
                                  nBytesToRW, pabyData ) != CE_None )
                {
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Failed to read %d bytes at " CPL_FRMT_GUIB ".",
                              nBytesToRW, nOffset );
                }

/* -------------------------------------------------------------------- */
/*      Copy data from disk buffer to user block buffer and subsample,  */
/*      if needed.                                                      */
/* -------------------------------------------------------------------- */
                if ( nXSize == nBufXSize && nYSize == nBufYSize )
                {
                    GDALCopyWords( pabyData, eDataType, nPixelOffset,
                                   (GByte *)pData + (vsi_l_offset)iLine * nLineSpace,
                                   eBufType, nPixelSpace, nXSize );
                }
                else
                {
                    int     iPixel;

                    for ( iPixel = 0; iPixel < nBufXSize; iPixel++ )
                    {
                        GDALCopyWords( pabyData +
                                       (vsi_l_offset)(iPixel * dfSrcXInc) * nPixelOffset,
                                       eDataType, nPixelOffset,
                                       (GByte *)pData + (vsi_l_offset)iLine * nLineSpace +
                                       (vsi_l_offset)iPixel * nPixelSpace,
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
            vsi_l_offset nOffset = nImgOffset + (vsi_l_offset)nYOff * nLineOffset + nXOff;
            if( Seek( nOffset, SEEK_SET) == -1 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to seek to " CPL_FRMT_GUIB " to write data.\n",
                          nOffset);
        
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
                    + ((vsi_l_offset)nYOff + (vsi_l_offset)(iLine*dfSrcYInc))*nLineOffset
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
                    GDALCopyWords( (GByte *)pData + (vsi_l_offset)iLine * nLineSpace,
                                   eBufType, nPixelSpace,
                                   pabyData, eDataType, nPixelOffset, nXSize );
                }
                else
                {
                    int     iPixel;

                    for ( iPixel = 0; iPixel < nBufXSize; iPixel++ )
                    {
                        GDALCopyWords( (GByte *)pData+(vsi_l_offset)iLine*nLineSpace +
                                       (vsi_l_offset)iPixel * nPixelSpace,
                                       eBufType, nPixelSpace,
                                       pabyData +
                                       (vsi_l_offset)(iPixel * dfSrcXInc) * nPixelOffset,
                                       eDataType, nPixelOffset, 1 );
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
                              "Failed to seek to " CPL_FRMT_GUIB " to read.\n",
                              nBlockOff );

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
        return VSIFSeekL( fpRawL, nOffset, nSeekMode );
    else
        return VSIFSeek( fpRaw, (long) nOffset, nSeekMode );
}

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

size_t RawRasterBand::Read( void *pBuffer, size_t nSize, size_t nCount )

{
    if( bIsVSIL )
        return VSIFReadL( pBuffer, nSize, nCount, fpRawL );
    else
        return VSIFRead( pBuffer, nSize, nCount, fpRaw );
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t RawRasterBand::Write( void *pBuffer, size_t nSize, size_t nCount )

{
    if( bIsVSIL )
        return VSIFWriteL( pBuffer, nSize, nCount, fpRawL );
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
/*                           GetVirtualMemAuto()                        */
/************************************************************************/

CPLVirtualMem  *RawRasterBand::GetVirtualMemAuto( GDALRWFlag eRWFlag,
                                                  int *pnPixelSpace,
                                                  GIntBig *pnLineSpace,
                                                  char **papszOptions )
{
    CPLAssert(pnPixelSpace);
    CPLAssert(pnLineSpace);

    vsi_l_offset nSize =  (vsi_l_offset)(nRasterYSize - 1) * nLineOffset +
        (nRasterXSize - 1) * nPixelOffset + GDALGetDataTypeSize(eDataType) / 8;

    if( !bIsVSIL || VSIFGetNativeFileDescriptorL(fpRawL) == NULL ||
        !CPLIsVirtualMemFileMapAvailable() || (eDataType != GDT_Byte && !bNativeOrder) ||
        (size_t)nSize != nSize || nPixelOffset < 0 || nLineOffset < 0 ||
        CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "USE_DEFAULT_IMPLEMENTATION", "NO")) )
    {
        return GDALRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace,
                                                 pnLineSpace, papszOptions);
    }

    FlushCache();

    CPLVirtualMem* pVMem = CPLVirtualMemFileMapNew(
        fpRawL, nImgOffset, nSize,
        (eRWFlag == GF_Write) ? VIRTUALMEM_READWRITE : VIRTUALMEM_READONLY,
        NULL, NULL);
    if( pVMem == NULL )
    {
        return GDALRasterBand::GetVirtualMemAuto(eRWFlag, pnPixelSpace,
                                                 pnLineSpace, papszOptions);
    }
    else
    {
        *pnPixelSpace = nPixelOffset;
        *pnLineSpace = nLineOffset;
        return pVMem;
    }
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
    /* It's pure virtual function but must be defined, even if empty. */
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
    const char* pszInterleave;

    /* The default GDALDataset::IRasterIO() implementation would go to */
    /* BlockBasedRasterIO if the dataset is interleaved. However if the */
    /* access pattern is compatible with DirectIO() we don't want to go */
    /* BlockBasedRasterIO, but rather used our optimized path in RawRasterBand::IRasterIO() */
    if (nXSize == nBufXSize && nYSize == nBufYSize && nBandCount > 1 &&
        (pszInterleave = GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE")) != NULL &&
        EQUAL(pszInterleave, "PIXEL"))
    {
        int iBandIndex;
        for(iBandIndex = 0; iBandIndex < nBandCount; iBandIndex ++ )
        {
            RawRasterBand* poBand = (RawRasterBand*) GetRasterBand(panBandMap[iBandIndex]);
            if( !poBand->CanUseDirectIO(nXOff, nYOff, nXSize, nYSize, eBufType ) )
            {
                break;
            }
        }
        if( iBandIndex == nBandCount )
        {
            CPLErr eErr = CE_None;
            for( iBandIndex = 0; 
                iBandIndex < nBandCount && eErr == CE_None; 
                iBandIndex++ )
            {
                GDALRasterBand *poBand = GetRasterBand(panBandMap[iBandIndex]);
                GByte *pabyBandData;

                if (poBand == NULL)
                {
                    eErr = CE_Failure;
                    break;
                }

                pabyBandData = ((GByte *) pData) + iBandIndex * nBandSpace;
                
                eErr = poBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                                        (void *) pabyBandData, nBufXSize, nBufYSize,
                                        eBufType, nPixelSpace, nLineSpace );
            }

            return eErr;
        }
    }

    return  GDALDataset::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize, eBufType, 
                                    nBandCount, panBandMap, 
                                    nPixelSpace, nLineSpace, nBandSpace );
}


