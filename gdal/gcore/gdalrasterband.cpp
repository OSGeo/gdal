/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Base class for format specific band class implementation.  This
 *           base class provides default implementation for many methods.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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

#include "gdal_priv.h"
#include "gdal_rat.h"
#include "cpl_string.h"

#define SUBBLOCK_SIZE 64
#define TO_SUBBLOCK(x) ((x) >> 6)
#define WITHIN_SUBBLOCK(x) ((x) & 0x3f)

// Number of data samples that will be used to compute approximate statistics
// (minimum value, maximum value, etc.)
#define GDALSTAT_APPROX_NUMSAMPLES 2500

CPL_CVSID("$Id$");

/************************************************************************/
/*                           GDALRasterBand()                           */
/************************************************************************/

/*! Constructor. Applications should never create GDALRasterBands directly. */

GDALRasterBand::GDALRasterBand()

{
    poDS = NULL;
    nBand = 0;
    nRasterXSize = nRasterYSize = 0;

    eAccess = GA_ReadOnly;
    nBlockXSize = nBlockYSize = -1;
    eDataType = GDT_Byte;

    nSubBlocksPerRow = nBlocksPerRow = 0;
    nSubBlocksPerColumn = nBlocksPerColumn = 0;

    bSubBlockingActive = FALSE;
    papoBlocks = NULL;

    poMask = NULL;
    bOwnMask = false;
    nMaskFlags = 0;

    nBlockReads = 0;
    bForceCachedIO =  CSLTestBoolean( 
        CPLGetConfigOption( "GDAL_FORCE_CACHING", "NO") );

    eFlushBlockErr = CE_None;
}

/************************************************************************/
/*                          ~GDALRasterBand()                           */
/************************************************************************/

/*! Destructor. Applications should never destroy GDALRasterBands directly,
    instead destroy the GDALDataset. */

GDALRasterBand::~GDALRasterBand()

{
    FlushCache();

    CPLFree( papoBlocks );

    if( nBlockReads > nBlocksPerRow * nBlocksPerColumn
        && nBand == 1 && poDS != NULL )
    {
        CPLDebug( "GDAL", "%d block reads on %d block band 1 of %s.",
                  nBlockReads, nBlocksPerRow * nBlocksPerColumn, 
                  poDS->GetDescription() );
    }

    if( bOwnMask )
    {
        delete poMask;
        poMask = NULL;
        nMaskFlags = 0;
        bOwnMask = false;
    }
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

/**
 * \brief Read/write a region of image data for this band.
 *
 * This method allows reading a region of a GDALRasterBand into a buffer,
 * or writing data from a buffer into a region of a GDALRasterBand. It
 * automatically takes care of data type translation if the data type
 * (eBufType) of the buffer is different than that of the GDALRasterBand.
 * The method also takes care of image decimation / replication if the
 * buffer size (nBufXSize x nBufYSize) is different than the size of the
 * region being accessed (nXSize x nYSize).
 *
 * The nPixelSpace and nLineSpace parameters allow reading into or
 * writing from unusually organized buffers. This is primarily used
 * for buffers containing more than one bands raster data in interleaved
 * format. 
 *
 * Some formats may efficiently implement decimation into a buffer by
 * reading from lower resolution overview images.
 *
 * For highest performance full resolution data access, read and write
 * on "block boundaries" as returned by GetBlockSize(), or use the
 * ReadBlock() and WriteBlock() methods.
 *
 * This method is the same as the C GDALRasterIO() function.
 *
 * @param eRWFlag Either GF_Read to read a region of data, or GF_Write to
 * write a region of data.
 *
 * @param nXOff The pixel offset to the top left corner of the region
 * of the band to be accessed. This would be zero to start from the left side.
 *
 * @param nYOff The line offset to the top left corner of the region
 * of the band to be accessed. This would be zero to start from the top.
 *
 * @param nXSize The width of the region of the band to be accessed in pixels.
 *
 * @param nYSize The height of the region of the band to be accessed in lines.
 *
 * @param pData The buffer into which the data should be read, or from which
 * it should be written. This buffer must contain at least nBufXSize *
 * nBufYSize words of type eBufType. It is organized in left to right,
 * top to bottom pixel order. Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nBufXSize the width of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer image into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer. The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline. If defaulted
 * (0) the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next. If defaulted (0) the size of the datatype
 * eBufType * nBufXSize is used.
 *
 * @return CE_Failure if the access fails, otherwise CE_None.
 */

CPLErr GDALRasterBand::RasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 int nPixelSpace,
                                 int nLineSpace )

{

    if( NULL == pData )
    {
        ReportError( CE_Failure, CPLE_AppDefined,
                  "The buffer into which the data should be read is null" );
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Some size values are "noop".  Lets just return to avoid         */
/*      stressing lower level functions.                                */
/* -------------------------------------------------------------------- */
    if( nXSize < 1 || nYSize < 1 || nBufXSize < 1 || nBufYSize < 1 )
    {
        CPLDebug( "GDAL", 
                  "RasterIO() skipped for odd window or buffer size.\n"
                  "  Window = (%d,%d)x%dx%d\n"
                  "  Buffer = %dx%d\n",
                  nXOff, nYOff, nXSize, nYSize, 
                  nBufXSize, nBufYSize );

        return CE_None;
    }

    if( eRWFlag == GF_Write && eFlushBlockErr != CE_None )
    {
        ReportError(eFlushBlockErr, CPLE_AppDefined,
                 "An error occured while writing a dirty block");
        CPLErr eErr = eFlushBlockErr;
        eFlushBlockErr = CE_None;
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      If pixel and line spaceing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize( eBufType ) / 8;
    
    if( nLineSpace == 0 )
    {
        if (nPixelSpace > INT_MAX / nBufXSize)
        {
            ReportError( CE_Failure, CPLE_AppDefined,
                      "Int overflow : %d x %d", nPixelSpace, nBufXSize );
            return CE_Failure;
        }
        nLineSpace = nPixelSpace * nBufXSize;
    }
    
/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */
    if( nXOff < 0 || nXOff > INT_MAX - nXSize || nXOff + nXSize > nRasterXSize
        || nYOff < 0 || nYOff > INT_MAX - nYSize || nYOff + nYSize > nRasterYSize )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Access window out of range in RasterIO().  Requested\n"
                  "(%d,%d) of size %dx%d on raster of %dx%d.",
                  nXOff, nYOff, nXSize, nYSize, nRasterXSize, nRasterYSize );
        return CE_Failure;
    }

    if( eRWFlag != GF_Read && eRWFlag != GF_Write )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "eRWFlag = %d, only GF_Read (0) and GF_Write (1) are legal.",
                  eRWFlag );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Call the format specific function.                              */
/* -------------------------------------------------------------------- */
    if( bForceCachedIO )
        return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nPixelSpace, nLineSpace );
    else
        return IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                          pData, nBufXSize, nBufYSize, eBufType,
                          nPixelSpace, nLineSpace ) ;
}

/************************************************************************/
/*                            GDALRasterIO()                            */
/************************************************************************/

/**
 * \brief Read/write a region of image data for this band.
 *
 * @see GDALRasterBand::RasterIO()
 */

CPLErr CPL_STDCALL 
GDALRasterIO( GDALRasterBandH hBand, GDALRWFlag eRWFlag,
              int nXOff, int nYOff, int nXSize, int nYSize,
              void * pData, int nBufXSize, int nBufYSize,
              GDALDataType eBufType,
              int nPixelSpace, int nLineSpace )
    
{
    VALIDATE_POINTER1( hBand, "GDALRasterIO", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);

    return( poBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                              pData, nBufXSize, nBufYSize, eBufType,
                              nPixelSpace, nLineSpace ) );
}
                     
/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

/**
 * \brief Read a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * This method is the same as the C GDALReadBlock() function.
 *
 * See the GetLockedBlockRef() method for a way of accessing internally cached
 * block oriented data without an extra copy into an application buffer.
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param pImage the buffer into which the data will be read.  The buffer
 * must be large enough to hold GetBlockXSize()*GetBlockYSize() words
 * of type GetRasterDataType().
 *
 * @return CE_None on success or CE_Failure on an error.
 *
 * The following code would efficiently compute a histogram of eight bit
 * raster data.  Note that the final block may be partial ... data beyond
 * the edge of the underlying raster band in these edge blocks is of an
 * undermined value.
 *
<pre>
 CPLErr GetHistogram( GDALRasterBand *poBand, int *panHistogram )

 {
     int        nXBlocks, nYBlocks, nXBlockSize, nYBlockSize;
     int        iXBlock, iYBlock;
     GByte      *pabyData;

     memset( panHistogram, 0, sizeof(int) * 256 );

     CPLAssert( poBand->GetRasterDataType() == GDT_Byte );

     poBand->GetBlockSize( &nXBlockSize, &nYBlockSize );
     nXBlocks = (poBand->GetXSize() + nXBlockSize - 1) / nXBlockSize;
     nYBlocks = (poBand->GetYSize() + nYBlockSize - 1) / nYBlockSize;

     pabyData = (GByte *) CPLMalloc(nXBlockSize * nYBlockSize);

     for( iYBlock = 0; iYBlock < nYBlocks; iYBlock++ )
     {
         for( iXBlock = 0; iXBlock < nXBlocks; iXBlock++ )
         {
             int        nXValid, nYValid;
             
             poBand->ReadBlock( iXBlock, iYBlock, pabyData );

             // Compute the portion of the block that is valid
             // for partial edge blocks.
             if( (iXBlock+1) * nXBlockSize > poBand->GetXSize() )
                 nXValid = poBand->GetXSize() - iXBlock * nXBlockSize;
             else
                 nXValid = nXBlockSize;

             if( (iYBlock+1) * nYBlockSize > poBand->GetYSize() )
                 nYValid = poBand->GetYSize() - iYBlock * nYBlockSize;
             else
                 nYValid = nYBlockSize;

             // Collect the histogram counts.
             for( int iY = 0; iY < nYValid; iY++ )
             {
                 for( int iX = 0; iX < nXValid; iX++ )
                 {
                     panHistogram[pabyData[iX + iY * nXBlockSize]] += 1;
                 }
             }
         }
     }
 }
 
</pre>
 */


CPLErr GDALRasterBand::ReadBlock( int nXBlockOff, int nYBlockOff,
                                   void * pImage )

{
/* -------------------------------------------------------------------- */
/*      Validate arguments.                                             */
/* -------------------------------------------------------------------- */
    CPLAssert( pImage != NULL );
    
    if( !InitBlockInfo() )
        return CE_Failure;
        
    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                        "GDALRasterBand::ReadBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nYBlockOff value (%d) in "
                        "GDALRasterBand::ReadBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */
    return( IReadBlock( nXBlockOff, nYBlockOff, pImage ) );
}

/************************************************************************/
/*                           GDALReadBlock()                            */
/************************************************************************/

/**
 * \brief Read a block of image data efficiently.
 *
 * @see GDALRasterBand::ReadBlock()
 */

CPLErr CPL_STDCALL GDALReadBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                      void * pData )

{
    VALIDATE_POINTER1( hBand, "GDALReadBlock", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return( poBand->ReadBlock( nXOff, nYOff, pData ) );
}

/************************************************************************/
/*                            IWriteBlock()                             */
/*                                                                      */
/*      Default internal implementation ... to be overriden by          */
/*      subclasses that support writing.                                */
/************************************************************************/

CPLErr GDALRasterBand::IWriteBlock( int, int, void * )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "WriteBlock() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

/**
 * \brief Write a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * This method is the same as the C GDALWriteBlock() function.
 *
 * See ReadBlock() for an example of block oriented data access.
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @param pImage the buffer from which the data will be written.  The buffer
 * must be large enough to hold GetBlockXSize()*GetBlockYSize() words
 * of type GetRasterDataType().
 *
 * @return CE_None on success or CE_Failure on an error.
 */

CPLErr GDALRasterBand::WriteBlock( int nXBlockOff, int nYBlockOff,
                                   void * pImage )

{
/* -------------------------------------------------------------------- */
/*      Validate arguments.                                             */
/* -------------------------------------------------------------------- */
    CPLAssert( pImage != NULL );

    if( !InitBlockInfo() )
        return CE_Failure;

    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                        "GDALRasterBand::WriteBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nYBlockOff value (%d) in "
                        "GDALRasterBand::WriteBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }

    if( eAccess == GA_ReadOnly )
    {
        ReportError( CE_Failure, CPLE_NoWriteAccess,
                  "Attempt to write to read only dataset in"
                  "GDALRasterBand::WriteBlock().\n" );

        return( CE_Failure );
    }

    if( eFlushBlockErr != CE_None )
    {
        ReportError(eFlushBlockErr, CPLE_AppDefined,
                 "An error occured while writing a dirty block");
        CPLErr eErr = eFlushBlockErr;
        eFlushBlockErr = CE_None;
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */
    return( IWriteBlock( nXBlockOff, nYBlockOff, pImage ) );
}

/************************************************************************/
/*                           GDALWriteBlock()                           */
/************************************************************************/

/**
 * \brief Write a block of image data efficiently.
 *
 * @see GDALRasterBand::WriteBlock()
 */

CPLErr CPL_STDCALL GDALWriteBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                       void * pData )

{
    VALIDATE_POINTER1( hBand, "GDALWriteBlock", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand *>( hBand );
    return( poBand->WriteBlock( nXOff, nYOff, pData ) );
}


/************************************************************************/
/*                         GetRasterDataType()                          */
/************************************************************************/

/**
 * \brief Fetch the pixel data type for this band.
 *
 * @return the data type of pixels for this band.
 */
  

GDALDataType GDALRasterBand::GetRasterDataType()

{
    return eDataType;
}

/************************************************************************/
/*                       GDALGetRasterDataType()                        */
/************************************************************************/

/**
 * \brief Fetch the pixel data type for this band.
 *
 * @see GDALRasterBand::GetRasterDataType()
 */

GDALDataType CPL_STDCALL GDALGetRasterDataType( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterDataType", GDT_Unknown );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetRasterDataType();
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

/**
 * \brief Fetch the "natural" block size of this band.
 *
 * GDAL contains a concept of the natural block size of rasters so that
 * applications can organized data access efficiently for some file formats.
 * The natural block size is the block size that is most efficient for
 * accessing the format.  For many formats this is simple a whole scanline
 * in which case *pnXSize is set to GetXSize(), and *pnYSize is set to 1.
 *
 * However, for tiled images this will typically be the tile size.
 *
 * Note that the X and Y block sizes don't have to divide the image size
 * evenly, meaning that right and bottom edge blocks may be incomplete.
 * See ReadBlock() for an example of code dealing with these issues.
 *
 * @param pnXSize integer to put the X block size into or NULL.
 *
 * @param pnYSize integer to put the Y block size into or NULL.
 */

void GDALRasterBand::GetBlockSize( int * pnXSize, int *pnYSize )

{
    if( nBlockXSize <= 0 || nBlockYSize <= 0 )
    {
        ReportError( CE_Failure, CPLE_AppDefined, "Invalid block dimension : %d * %d",
                 nBlockXSize, nBlockYSize );
        if( pnXSize != NULL )
            *pnXSize = 0;
        if( pnYSize != NULL )
            *pnYSize = 0;
    }
    else
    {
        if( pnXSize != NULL )
            *pnXSize = nBlockXSize;
        if( pnYSize != NULL )
            *pnYSize = nBlockYSize;
    }
}

/************************************************************************/
/*                          GDALGetBlockSize()                          */
/************************************************************************/

/**
 * \brief Fetch the "natural" block size of this band.
 *
 * @see GDALRasterBand::GetBlockSize()
 */

void CPL_STDCALL 
GDALGetBlockSize( GDALRasterBandH hBand, int * pnXSize, int * pnYSize )

{
    VALIDATE_POINTER0( hBand, "GDALGetBlockSize" );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    poBand->GetBlockSize( pnXSize, pnYSize );
}

/************************************************************************/
/*                           InitBlockInfo()                            */
/************************************************************************/

int GDALRasterBand::InitBlockInfo()

{
    if( papoBlocks != NULL )
        return TRUE;

    /* Do some validation of raster and block dimensions in case the driver */
    /* would have neglected to do it itself */
    if( nBlockXSize <= 0 || nBlockYSize <= 0 )
    {
        ReportError( CE_Failure, CPLE_AppDefined, "Invalid block dimension : %d * %d",
                  nBlockXSize, nBlockYSize );
        return FALSE;
    }

    if( nRasterXSize <= 0 || nRasterYSize <= 0 )
    {
        ReportError( CE_Failure, CPLE_AppDefined, "Invalid raster dimension : %d * %d",
                  nRasterXSize, nRasterYSize );
        return FALSE;
    }

    if (nBlockXSize >= 10000 || nBlockYSize >= 10000)
    {
        /* Check that the block size is not overflowing int capacity as it is */
        /* (reasonnably) assumed in many places (GDALRasterBlock::Internalize(), */
        /* GDALRasterBand::Fill(), many drivers...) */
        /* As 10000 * 10000 * 16 < INT_MAX, we don't need to do the multiplication in other cases */

        int nSizeInBytes = nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType) / 8);

        GIntBig nBigSizeInBytes = (GIntBig)nBlockXSize * nBlockYSize * (GDALGetDataTypeSize(eDataType) / 8);
        if ((GIntBig)nSizeInBytes != nBigSizeInBytes)
        {
            ReportError( CE_Failure, CPLE_NotSupported, "Too big block : %d * %d",
                        nBlockXSize, nBlockYSize );
            return FALSE;
        }
    }

    /* Check for overflows in computation of nBlocksPerRow and nBlocksPerColumn */
    if (nRasterXSize > INT_MAX - (nBlockXSize-1))
    {
        ReportError( CE_Failure, CPLE_NotSupported, "Inappropriate raster width (%d) for block width (%d)",
                    nRasterXSize, nBlockXSize );
        return FALSE;
    }

    if (nRasterYSize > INT_MAX - (nBlockYSize-1))
    {
        ReportError( CE_Failure, CPLE_NotSupported, "Inappropriate raster height (%d) for block height (%d)",
                    nRasterYSize, nBlockYSize );
        return FALSE;
    }

    nBlocksPerRow = (nRasterXSize+nBlockXSize-1) / nBlockXSize;
    nBlocksPerColumn = (nRasterYSize+nBlockYSize-1) / nBlockYSize;
    
    if( nBlocksPerRow < SUBBLOCK_SIZE/2 )
    {
        bSubBlockingActive = FALSE;

        if (nBlocksPerRow < INT_MAX / nBlocksPerColumn)
        {
            papoBlocks = (GDALRasterBlock **)
                VSICalloc( sizeof(void*), nBlocksPerRow * nBlocksPerColumn );
        }
        else
        {
            ReportError( CE_Failure, CPLE_NotSupported, "Too many blocks : %d x %d",
                     nBlocksPerRow, nBlocksPerColumn );
            return FALSE;
        }
    }
    else
    {
        /* Check for overflows in computation of nSubBlocksPerRow and nSubBlocksPerColumn */
        if (nBlocksPerRow > INT_MAX - (SUBBLOCK_SIZE+1))
        {
            ReportError( CE_Failure, CPLE_NotSupported, "Inappropriate raster width (%d) for block width (%d)",
                        nRasterXSize, nBlockXSize );
            return FALSE;
        }

        if (nBlocksPerColumn > INT_MAX - (SUBBLOCK_SIZE+1))
        {
            ReportError( CE_Failure, CPLE_NotSupported, "Inappropriate raster height (%d) for block height (%d)",
                        nRasterYSize, nBlockYSize );
            return FALSE;
        }

        bSubBlockingActive = TRUE;

        nSubBlocksPerRow = (nBlocksPerRow + SUBBLOCK_SIZE + 1)/SUBBLOCK_SIZE;
        nSubBlocksPerColumn = (nBlocksPerColumn + SUBBLOCK_SIZE + 1)/SUBBLOCK_SIZE;

        if (nSubBlocksPerRow < INT_MAX / nSubBlocksPerColumn)
        {
            papoBlocks = (GDALRasterBlock **)
                VSICalloc( sizeof(void*), nSubBlocksPerRow * nSubBlocksPerColumn );
        }
        else
        {
            ReportError( CE_Failure, CPLE_NotSupported, "Too many subblocks : %d x %d",
                      nSubBlocksPerRow, nSubBlocksPerColumn );
            return FALSE;
        }
    }

    if( papoBlocks == NULL )
    {
        ReportError( CE_Failure, CPLE_OutOfMemory,
                  "Out of memory in InitBlockInfo()." );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                             AdoptBlock()                             */
/*                                                                      */
/*      Add a block to the raster band's block matrix.  If this         */
/*      exceeds our maximum blocks for this layer, flush the oldest     */
/*      block out.                                                      */
/*                                                                      */
/*      This method is protected.                                       */
/************************************************************************/

CPLErr GDALRasterBand::AdoptBlock( int nXBlockOff, int nYBlockOff,
                                   GDALRasterBlock * poBlock )

{
    int         nBlockIndex;
    
    if( !InitBlockInfo() )
        return CE_Failure;
    
/* -------------------------------------------------------------------- */
/*      Simple case without subblocking.                                */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * nBlocksPerRow;

        if( papoBlocks[nBlockIndex] == poBlock )
            return( CE_None );

        if( papoBlocks[nBlockIndex] != NULL )
            FlushBlock( nXBlockOff, nYBlockOff );

        papoBlocks[nBlockIndex] = poBlock;
        poBlock->Touch();

        return( CE_None );
    }

/* -------------------------------------------------------------------- */
/*      Identify the subblock in which our target occurs, and create    */
/*      it if necessary.                                                */
/* -------------------------------------------------------------------- */
    int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
        + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

    if( papoBlocks[nSubBlock] == NULL )
    {
        const int nSubGridSize = 
            sizeof(GDALRasterBlock*) * SUBBLOCK_SIZE * SUBBLOCK_SIZE;

        papoBlocks[nSubBlock] = (GDALRasterBlock *) VSICalloc(1, nSubGridSize);
        if( papoBlocks[nSubBlock] == NULL )
        {
            ReportError( CE_Failure, CPLE_OutOfMemory,
                      "Out of memory in AdoptBlock()." );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
    GDALRasterBlock **papoSubBlockGrid = 
        (GDALRasterBlock **) papoBlocks[nSubBlock];

    int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
        + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

    if( papoSubBlockGrid[nBlockInSubBlock] == poBlock )
        return CE_None;

    if( papoSubBlockGrid[nBlockInSubBlock] != NULL )
        FlushBlock( nXBlockOff, nYBlockOff );

    papoSubBlockGrid[nBlockInSubBlock] = poBlock;
    poBlock->Touch();

    return CE_None;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

/**
 * \brief Flush raster data cache.
 *
 * This call will recover memory used to cache data blocks for this raster
 * band, and ensure that new requests are referred to the underlying driver.
 *
 * This method is the same as the C function GDALFlushRasterCache().
 *
 * @return CE_None on success.
 */

CPLErr GDALRasterBand::FlushCache()

{
    CPLErr eGlobalErr = eFlushBlockErr;

    if (eFlushBlockErr != CE_None)
    {
        ReportError(eFlushBlockErr, CPLE_AppDefined,
                 "An error occured while writing a dirty block");
        eFlushBlockErr = CE_None;
    }

    if (papoBlocks == NULL)
        return eGlobalErr;

/* -------------------------------------------------------------------- */
/*      Flush all blocks in memory ... this case is without subblocking.*/
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        for( int iY = 0; iY < nBlocksPerColumn; iY++ )
        {
            for( int iX = 0; iX < nBlocksPerRow; iX++ )
            {
                if( papoBlocks[iX + iY*nBlocksPerRow] != NULL )
                {
                    CPLErr    eErr;

                    eErr = FlushBlock( iX, iY, eGlobalErr == CE_None );

                    if( eErr != CE_None )
                        eGlobalErr = eErr;
                }
            }
        }
        return eGlobalErr;
    }

/* -------------------------------------------------------------------- */
/*      With subblocking.  We can short circuit missing subblocks.      */
/* -------------------------------------------------------------------- */
    int iSBX, iSBY;

    for( iSBY = 0; iSBY < nSubBlocksPerColumn; iSBY++ )
    {
        for( iSBX = 0; iSBX < nSubBlocksPerRow; iSBX++ )
        {
            int nSubBlock = iSBX + iSBY * nSubBlocksPerRow;
        
            GDALRasterBlock **papoSubBlockGrid = 
                (GDALRasterBlock **) papoBlocks[nSubBlock];

            if( papoSubBlockGrid == NULL )
                continue;

            for( int iY = 0; iY < SUBBLOCK_SIZE; iY++ )
            {
                for( int iX = 0; iX < SUBBLOCK_SIZE; iX++ )
                {
                    if( papoSubBlockGrid[iX + iY * SUBBLOCK_SIZE] != NULL )
                    {
                        CPLErr eErr;

                        eErr = FlushBlock( iX + iSBX * SUBBLOCK_SIZE, 
                                           iY + iSBY * SUBBLOCK_SIZE,
                                           eGlobalErr == CE_None );
                        if( eErr != CE_None )
                            eGlobalErr = eErr;
                    }
                }
            }

            // We might as well get rid of this grid chunk since we know 
            // it is now empty.
            papoBlocks[nSubBlock] = NULL;
            CPLFree( papoSubBlockGrid );
        }
    }

    return( eGlobalErr );
}

/************************************************************************/
/*                        GDALFlushRasterCache()                        */
/************************************************************************/

/**
 * \brief Flush raster data cache.
 *
 * @see GDALRasterBand::FlushCache()
 */

CPLErr CPL_STDCALL GDALFlushRasterCache( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALFlushRasterCache", CE_Failure );

    return ((GDALRasterBand *) hBand)->FlushCache();
}

/************************************************************************/
/*                             FlushBlock()                             */
/*                                                                      */
/*      Flush a block out of the block cache.  If it has been           */
/*      modified write it to disk.  If no specific tile is              */
/*      indicated, write the oldest tile.                               */
/*                                                                      */
/*      Protected method.                                               */
/************************************************************************/

CPLErr GDALRasterBand::FlushBlock( int nXBlockOff, int nYBlockOff, int bWriteDirtyBlock )

{
    int             nBlockIndex;
    GDALRasterBlock *poBlock = NULL;

    if( !papoBlocks )
        return CE_None;
    
/* -------------------------------------------------------------------- */
/*      Validate the request                                            */
/* -------------------------------------------------------------------- */
    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockXOff value (%d) in "
                        "GDALRasterBand::FlushBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockYOff value (%d) in "
                        "GDALRasterBand::FlushBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * nBlocksPerRow;

        GDALRasterBlock::SafeLockBlock( papoBlocks + nBlockIndex );

        poBlock = papoBlocks[nBlockIndex];
        papoBlocks[nBlockIndex] = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
            + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;
        
        if( papoBlocks[nSubBlock] == NULL )
            return CE_None;
        
/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
        GDALRasterBlock **papoSubBlockGrid = 
            (GDALRasterBlock **) papoBlocks[nSubBlock];
        
        int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
            + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;
        
        GDALRasterBlock::SafeLockBlock( papoSubBlockGrid + nBlockInSubBlock );

        poBlock = papoSubBlockGrid[nBlockInSubBlock];
        papoSubBlockGrid[nBlockInSubBlock] = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is the target block dirty?  If so we need to write it.          */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    if( poBlock == NULL )
        return CE_None;

    poBlock->Detach();

    if( bWriteDirtyBlock && poBlock->GetDirty() )
        eErr = poBlock->Write();

/* -------------------------------------------------------------------- */
/*      Deallocate the block;                                           */
/* -------------------------------------------------------------------- */
    poBlock->DropLock();
    delete poBlock;

    return eErr;
}

/************************************************************************/
/*                        TryGetLockedBlockRef()                        */
/************************************************************************/

/**
 * \brief Try fetching block ref. 
 *
 * This method will returned the requested block (locked) if it is already
 * in the block cache for the layer.  If not, NULL is returned.  
 * 
 * If a non-NULL value is returned, then a lock for the block will have been
 * acquired on behalf of the caller.  It is absolutely imperative that the
 * caller release this lock (with GDALRasterBlock::DropLock()) or else
 * severe problems may result.
 *
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 * 
 * @return NULL if block not available, or locked block pointer. 
 */

GDALRasterBlock *GDALRasterBand::TryGetLockedBlockRef( int nXBlockOff, 
                                                       int nYBlockOff )

{
    int             nBlockIndex = 0;
    
    if( !InitBlockInfo() )
        return( NULL );
    
/* -------------------------------------------------------------------- */
/*      Validate the request                                            */
/* -------------------------------------------------------------------- */
    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockXOff value (%d) in "
                        "GDALRasterBand::TryGetLockedBlockRef()\n",
                  nXBlockOff );

        return( NULL );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        ReportError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockYOff value (%d) in "
                        "GDALRasterBand::TryGetLockedBlockRef()\n",
                  nYBlockOff );

        return( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Simple case for single level caches.                            */
/* -------------------------------------------------------------------- */
    if( !bSubBlockingActive )
    {
        nBlockIndex = nXBlockOff + nYBlockOff * nBlocksPerRow;
        
        GDALRasterBlock::SafeLockBlock( papoBlocks + nBlockIndex );

        return papoBlocks[nBlockIndex];
    }

/* -------------------------------------------------------------------- */
/*      Identify our subblock.                                          */
/* -------------------------------------------------------------------- */
    int nSubBlock = TO_SUBBLOCK(nXBlockOff) 
        + TO_SUBBLOCK(nYBlockOff) * nSubBlocksPerRow;

    if( papoBlocks[nSubBlock] == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Check within subblock.                                          */
/* -------------------------------------------------------------------- */
    GDALRasterBlock **papoSubBlockGrid = 
        (GDALRasterBlock **) papoBlocks[nSubBlock];

    int nBlockInSubBlock = WITHIN_SUBBLOCK(nXBlockOff)
        + WITHIN_SUBBLOCK(nYBlockOff) * SUBBLOCK_SIZE;

    GDALRasterBlock::SafeLockBlock( papoSubBlockGrid + nBlockInSubBlock );

    return papoSubBlockGrid[nBlockInSubBlock];
}

/************************************************************************/
/*                         GetLockedBlockRef()                          */
/************************************************************************/

/**
 * \brief Fetch a pointer to an internally cached raster block.
 *
 * This method will returned the requested block (locked) if it is already
 * in the block cache for the layer.  If not, the block will be read from 
 * the driver, and placed in the layer block cached, then returned.  If an
 * error occurs reading the block from the driver, a NULL value will be
 * returned.
 * 
 * If a non-NULL value is returned, then a lock for the block will have been
 * acquired on behalf of the caller.  It is absolutely imperative that the
 * caller release this lock (with GDALRasterBlock::DropLock()) or else
 * severe problems may result.
 *
 * Note that calling GetLockedBlockRef() on a previously uncached band will
 * enable caching.
 * 
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the top most block, 1 the next block and so forth.
 * 
 * @param bJustInitialize If TRUE the block will be allocated and initialized,
 * but not actually read from the source.  This is useful when it will just
 * be completely set and written back. 
 *
 * @return pointer to the block object, or NULL on failure.
 */

GDALRasterBlock * GDALRasterBand::GetLockedBlockRef( int nXBlockOff,
                                                     int nYBlockOff,
                                                     int bJustInitialize )

{
    GDALRasterBlock *poBlock = NULL;

/* -------------------------------------------------------------------- */
/*      Try and fetch from cache.                                       */
/* -------------------------------------------------------------------- */
    poBlock = TryGetLockedBlockRef( nXBlockOff, nYBlockOff );

/* -------------------------------------------------------------------- */
/*      If we didn't find it in our memory cache, instantiate a         */
/*      block (potentially load from disk) and "adopt" it into the      */
/*      cache.                                                          */
/* -------------------------------------------------------------------- */
    if( poBlock == NULL )
    {
        if( !InitBlockInfo() )
            return( NULL );

    /* -------------------------------------------------------------------- */
    /*      Validate the request                                            */
    /* -------------------------------------------------------------------- */
        if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
        {
            ReportError( CE_Failure, CPLE_IllegalArg,
                      "Illegal nBlockXOff value (%d) in "
                      "GDALRasterBand::GetLockedBlockRef()\n",
                      nXBlockOff );

            return( NULL );
        }

        if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
        {
            ReportError( CE_Failure, CPLE_IllegalArg,
                      "Illegal nBlockYOff value (%d) in "
                      "GDALRasterBand::GetLockedBlockRef()\n",
                      nYBlockOff );

            return( NULL );
        }

        poBlock = new GDALRasterBlock( this, nXBlockOff, nYBlockOff );

        poBlock->AddLock();

        /* allocate data space */
        if( poBlock->Internalize() != CE_None )
        {
            poBlock->DropLock();
            delete poBlock;
            return( NULL );
        }

        if ( AdoptBlock( nXBlockOff, nYBlockOff, poBlock ) != CE_None )
        {
            poBlock->DropLock();
            delete poBlock;
            return( NULL );
        }

        if( !bJustInitialize
         && IReadBlock(nXBlockOff,nYBlockOff,poBlock->GetDataRef()) != CE_None)
        {
            poBlock->DropLock();
            FlushBlock( nXBlockOff, nYBlockOff );
            ReportError( CE_Failure, CPLE_AppDefined,
                "IReadBlock failed at X offset %d, Y offset %d",
                nXBlockOff, nYBlockOff );
            return( NULL );
        }

        if( !bJustInitialize )
        {
            nBlockReads++;
            if( nBlockReads == nBlocksPerRow * nBlocksPerColumn + 1 
                && nBand == 1 && poDS != NULL )
            {
                CPLDebug( "GDAL", "Potential thrashing on band %d of %s.",
                          nBand, poDS->GetDescription() );
            }
        }
    }

    return poBlock;
}

/************************************************************************/
/*                               Fill()                                 */
/************************************************************************/

/** 
 * \brief Fill this band with a constant value.
 *
 * GDAL makes no guarantees
 * about what values pixels in newly created files are set to, so this
 * method can be used to clear a band to a specified "default" value.
 * The fill value is passed in as a double but this will be converted
 * to the underlying type before writing to the file. An optional
 * second argument allows the imaginary component of a complex
 * constant value to be specified.
 * 
 * @param dfRealValue Real component of fill value
 * @param dfImaginaryValue Imaginary component of fill value, defaults to zero
 * 
 * @return CE_Failure if the write fails, otherwise CE_None
 */
CPLErr GDALRasterBand::Fill(double dfRealValue, double dfImaginaryValue) {

    // General approach is to construct a source block of the file's
    // native type containing the appropriate value and then copy this
    // to each block in the image via the RasterBlock cache. Using
    // the cache means we avoid file I/O if it's not necessary, at the
    // expense of some extra memcpy's (since we write to the
    // RasterBlock cache, which is then at some point written to the
    // underlying file, rather than simply directly to the underlying
    // file.)

    // Check we can write to the file
    if( eAccess == GA_ReadOnly ) {
        ReportError(CE_Failure, CPLE_NoWriteAccess,
                 "Attempt to write to read only dataset in"
                 "GDALRasterBand::Fill().\n" );
        return CE_Failure;
    }

    // Make sure block parameters are set
    if( !InitBlockInfo() )
        return CE_Failure;

    // Allocate the source block
    int blockSize = nBlockXSize * nBlockYSize;
    int elementSize = GDALGetDataTypeSize(eDataType) / 8;
    int blockByteSize = blockSize * elementSize;
    unsigned char* srcBlock = (unsigned char*) VSIMalloc(blockByteSize);
    if (srcBlock == NULL) {
        ReportError(CE_Failure, CPLE_OutOfMemory,
                 "GDALRasterBand::Fill(): Out of memory "
		 "allocating %d bytes.\n", blockByteSize);
        return CE_Failure;
    }
    
    // Initialize the first element of the block, doing type conversion
    double complexSrc[2] = { dfRealValue, dfImaginaryValue };
    GDALCopyWords(complexSrc, GDT_CFloat64, 0, srcBlock, eDataType, 0, 1);

    // Copy first element to the rest of the block
    for (unsigned char* blockPtr = srcBlock + elementSize; 
	 blockPtr < srcBlock + blockByteSize; blockPtr += elementSize) {
	memcpy(blockPtr, srcBlock, elementSize);
    }

    // Write block to block cache
    for (int j = 0; j < nBlocksPerColumn; ++j) {
	for (int i = 0; i < nBlocksPerRow; ++i) {
	    GDALRasterBlock* destBlock = GetLockedBlockRef(i, j, TRUE);
	    if (destBlock == NULL) {
            ReportError(CE_Failure, CPLE_OutOfMemory,
			 "GDALRasterBand::Fill(): Error "
			 "while retrieving cache block.\n");
                VSIFree(srcBlock);
		return CE_Failure;
	    }
            if (destBlock->GetDataRef() == NULL)
            {
                destBlock->DropLock();
                VSIFree(srcBlock);
                return CE_Failure;
            }
	    memcpy(destBlock->GetDataRef(), srcBlock, blockByteSize);
	    destBlock->MarkDirty();
            destBlock->DropLock();
	}
    }

    // Free up the source block
    VSIFree(srcBlock);

    return CE_None;
}


/************************************************************************/
/*                         GDALFillRaster()                             */
/************************************************************************/

/** 
 * \brief Fill this band with a constant value.
 *
 * @see GDALRasterBand::Fill()
 */
CPLErr CPL_STDCALL GDALFillRaster(GDALRasterBandH hBand, double dfRealValue, 
		      double dfImaginaryValue)
{
    VALIDATE_POINTER1( hBand, "GDALFillRaster", CE_Failure );
    
    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->Fill(dfRealValue, dfImaginaryValue);
}

/************************************************************************/
/*                             GetAccess()                              */
/************************************************************************/

/**
 * \brief Find out if we have update permission for this band.
 *
 * This method is the same as the C function GDALGetRasterAccess().
 *
 * @return Either GA_Update or GA_ReadOnly.
 */

GDALAccess GDALRasterBand::GetAccess()

{
    return eAccess;
}

/************************************************************************/
/*                        GDALGetRasterAccess()                         */
/************************************************************************/

/**
 * \brief Find out if we have update permission for this band.
 *
 * @see GDALRasterBand::GetAccess()
 */

GDALAccess CPL_STDCALL GDALGetRasterAccess( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterAccess", GA_ReadOnly );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetAccess();
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

/**
 * \brief Fetch the list of category names for this raster.
 *
 * The return list is a "StringList" in the sense of the CPL functions.
 * That is a NULL terminated array of strings.  Raster values without 
 * associated names will have an empty string in the returned list.  The
 * first entry in the list is for raster values of zero, and so on. 
 *
 * The returned stringlist should not be altered or freed by the application.
 * It may change on the next GDAL call, so please copy it if it is needed
 * for any period of time. 
 * 
 * @return list of names, or NULL if none.
 */

char **GDALRasterBand::GetCategoryNames()

{
    return NULL;
}

/************************************************************************/
/*                     GDALGetRasterCategoryNames()                     */
/************************************************************************/

/**
 * \brief Fetch the list of category names for this raster.
 *
 * @see GDALRasterBand::GetCategoryNames()
 */

char ** CPL_STDCALL GDALGetRasterCategoryNames( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterCategoryNames", NULL );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetCategoryNames();
}

/************************************************************************/
/*                          SetCategoryNames()                          */
/************************************************************************/

/**
 * \brief Set the category names for this band.
 *
 * See the GetCategoryNames() method for more on the interpretation of
 * category names. 
 *
 * This method is the same as the C function GDALSetRasterCategoryNames().
 *
 * @param papszNames the NULL terminated StringList of category names.  May
 * be NULL to just clear the existing list. 
 *
 * @return CE_None on success of CE_Failure on failure.  If unsupported
 * by the driver CE_Failure is returned, but no error message is reported.
 */

CPLErr GDALRasterBand::SetCategoryNames( char ** papszNames )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetCategoryNames() not supported for this dataset." );
    
    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetCategoryNames()                        */
/************************************************************************/

/**
 * \brief Set the category names for this band.
 *
 * @see GDALRasterBand::SetCategoryNames()
 */

CPLErr CPL_STDCALL 
GDALSetRasterCategoryNames( GDALRasterBandH hBand, char ** papszNames )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterCategoryNames", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetCategoryNames( papszNames );
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

/**
 * \brief Fetch the no data value for this band.
 * 
 * If there is no out of data value, an out of range value will generally
 * be returned.  The no data value for a band is generally a special marker
 * value used to mark pixels that are not valid data.  Such pixels should
 * generally not be displayed, nor contribute to analysis operations.
 *
 * This method is the same as the C function GDALGetRasterNoDataValue().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if a value
 * is actually associated with this layer.  May be NULL (default).
 *
 * @return the nodata value for this band.
 */

double GDALRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;
    
    return -1e10;
}

/************************************************************************/
/*                      GDALGetRasterNoDataValue()                      */
/************************************************************************/

/**
 * \brief Fetch the no data value for this band.
 * 
 * @see GDALRasterBand::GetNoDataValue()
 */

double CPL_STDCALL 
GDALGetRasterNoDataValue( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterNoDataValue", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

/**
 * \brief Set the no data value for this band. 
 *
 * To clear the nodata value, just set it with an "out of range" value.
 * Complex band no data values must have an imagery component of zero.
 *
 * This method is the same as the C function GDALSetRasterNoDataValue().
 *
 * @param dfNoData the value to set.
 *
 * @return CE_None on success, or CE_Failure on failure.  If unsupported
 * by the driver, CE_Failure is returned by no error message will have
 * been emitted.
 */

CPLErr GDALRasterBand::SetNoDataValue( double dfNoData )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetNoDataValue() not supported for this dataset." );

    return CE_Failure;
}

/************************************************************************/
/*                         GDALSetRasterNoDataValue()                   */
/************************************************************************/

/**
 * \brief Set the no data value for this band. 
 *
 * @see GDALRasterBand::SetNoDataValue()
 */

CPLErr CPL_STDCALL 
GDALSetRasterNoDataValue( GDALRasterBandH hBand, double dfValue )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterNoDataValue", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetNoDataValue( dfValue );
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

/**
 * \brief Fetch the maximum value for this band.
 * 
 * For file formats that don't know this intrinsically, the maximum supported
 * value for the data type will generally be returned.  
 *
 * This method is the same as the C function GDALGetRasterMaximum().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is a tight maximum or not.  May be NULL (default).
 *
 * @return the maximum raster value (excluding no data pixels)
 */

double GDALRasterBand::GetMaximum( int *pbSuccess )

{
    const char *pszValue = NULL;
    
    if( (pszValue = GetMetadataItem("STATISTICS_MAXIMUM")) != NULL )
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;
        
        return CPLAtofM(pszValue);
    }

    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    switch( eDataType )
    {
      case GDT_Byte:
      {
        const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        if (pszPixelType != NULL && EQUAL(pszPixelType, "SIGNEDBYTE"))
            return 127;
        else
            return 255;
      }

      case GDT_UInt16:
        return 65535;

      case GDT_Int16:
      case GDT_CInt16:
        return 32767;

      case GDT_Int32:
      case GDT_CInt32:
        return 2147483647.0;

      case GDT_UInt32:
        return 4294967295.0;

      case GDT_Float32:
      case GDT_CFloat32:
        return 4294967295.0; /* not actually accurate */

      case GDT_Float64:
      case GDT_CFloat64:
        return 4294967295.0; /* not actually accurate */

      default:
        return 4294967295.0; /* not actually accurate */
    }
}

/************************************************************************/
/*                        GDALGetRasterMaximum()                        */
/************************************************************************/

/**
 * \brief Fetch the maximum value for this band.
 * 
 * @see GDALRasterBand::GetMaximum()
 */

double CPL_STDCALL 
GDALGetRasterMaximum( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterMaximum", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetMaximum( pbSuccess );
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

/**
 * \brief Fetch the minimum value for this band.
 * 
 * For file formats that don't know this intrinsically, the minimum supported
 * value for the data type will generally be returned.  
 *
 * This method is the same as the C function GDALGetRasterMinimum().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is a tight minimum or not.  May be NULL (default).
 *
 * @return the minimum raster value (excluding no data pixels)
 */

double GDALRasterBand::GetMinimum( int *pbSuccess )

{
    const char *pszValue = NULL;
    
    if( (pszValue = GetMetadataItem("STATISTICS_MINIMUM")) != NULL )
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;
        
        return CPLAtofM(pszValue);
    }

    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    switch( eDataType )
    {
      case GDT_Byte:
      {
        const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        if (pszPixelType != NULL && EQUAL(pszPixelType, "SIGNEDBYTE"))
            return -128;
        else
            return 0;
      }

      case GDT_UInt16:
        return 0;

      case GDT_Int16:
        return -32768;

      case GDT_Int32:
        return -2147483648.0;

      case GDT_UInt32:
        return 0;

      case GDT_Float32:
        return -4294967295.0; /* not actually accurate */

      case GDT_Float64:
        return -4294967295.0; /* not actually accurate */

      default:
        return -4294967295.0; /* not actually accurate */
    }
}

/************************************************************************/
/*                        GDALGetRasterMinimum()                        */
/************************************************************************/

/**
 * \brief Fetch the minimum value for this band.
 * 
 * @see GDALRasterBand::GetMinimum()
 */

double CPL_STDCALL 
GDALGetRasterMinimum( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterMinimum", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetMinimum( pbSuccess );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

/**
 * \brief How should this band be interpreted as color?
 *
 * GCI_Undefined is returned when the format doesn't know anything
 * about the color interpretation. 
 *
 * This method is the same as the C function 
 * GDALGetRasterColorInterpretation().
 *
 * @return color interpretation value for band.
 */

GDALColorInterp GDALRasterBand::GetColorInterpretation()

{
    return GCI_Undefined;
}

/************************************************************************/
/*                  GDALGetRasterColorInterpretation()                  */
/************************************************************************/

/**
 * \brief How should this band be interpreted as color?
 *
 * @see GDALRasterBand::GetColorInterpretation()
 */

GDALColorInterp CPL_STDCALL 
GDALGetRasterColorInterpretation( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterColorInterpretation", GCI_Undefined );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetColorInterpretation();
}

/************************************************************************/
/*                       SetColorInterpretation()                       */
/************************************************************************/

/**
 * \brief Set color interpretation of a band.
 *
 * @param eColorInterp the new color interpretation to apply to this band.
 * 
 * @return CE_None on success or CE_Failure if method is unsupported by format.
 */

CPLErr GDALRasterBand::SetColorInterpretation( GDALColorInterp eColorInterp)

{
    (void) eColorInterp;
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetColorInterpretation() not supported for this dataset." );
    return CE_Failure;
}

/************************************************************************/
/*                  GDALSetRasterColorInterpretation()                  */
/************************************************************************/

/**
 * \brief Set color interpretation of a band.
 *
 * @see GDALRasterBand::SetColorInterpretation()
 */

CPLErr CPL_STDCALL 
GDALSetRasterColorInterpretation( GDALRasterBandH hBand,
                                  GDALColorInterp eColorInterp )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterColorInterpretation", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetColorInterpretation(eColorInterp);
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

/**
 * \brief Fetch the color table associated with band.
 *
 * If there is no associated color table, the return result is NULL.  The
 * returned color table remains owned by the GDALRasterBand, and can't
 * be depended on for long, nor should it ever be modified by the caller.
 *
 * This method is the same as the C function GDALGetRasterColorTable().
 *
 * @return internal color table, or NULL.
 */

GDALColorTable *GDALRasterBand::GetColorTable()

{
    return NULL;
}

/************************************************************************/
/*                      GDALGetRasterColorTable()                       */
/************************************************************************/

/**
 * \brief Fetch the color table associated with band.
 *
 * @see GDALRasterBand::GetColorTable()
 */

GDALColorTableH CPL_STDCALL GDALGetRasterColorTable( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterColorTable", NULL );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return (GDALColorTableH)poBand->GetColorTable();
}

/************************************************************************/
/*                           SetColorTable()                            */
/************************************************************************/

/**
 * \brief Set the raster color table. 
 * 
 * The driver will make a copy of all desired data in the colortable.  It
 * remains owned by the caller after the call.
 *
 * This method is the same as the C function GDALSetRasterColorTable().
 *
 * @param poCT the color table to apply.  This may be NULL to clear the color 
 * table (where supported).
 *
 * @return CE_None on success, or CE_Failure on failure.  If the action is
 * unsupported by the driver, a value of CE_Failure is returned, but no
 * error is issued.
 */

CPLErr GDALRasterBand::SetColorTable( GDALColorTable * poCT )

{
    (void) poCT;
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetColorTable() not supported for this dataset." );
    return CE_Failure;
}

/************************************************************************/
/*                      GDALSetRasterColorTable()                       */
/************************************************************************/

/**
 * \brief Set the raster color table. 
 * 
 * @see GDALRasterBand::SetColorTable()
 */

CPLErr CPL_STDCALL 
GDALSetRasterColorTable( GDALRasterBandH hBand, GDALColorTableH hCT )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterColorTable", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetColorTable( static_cast<GDALColorTable*>(hCT) );
}

/************************************************************************/
/*                       HasArbitraryOverviews()                        */
/************************************************************************/

/**
 * \brief Check for arbitrary overviews.
 *
 * This returns TRUE if the underlying datastore can compute arbitrary 
 * overviews efficiently, such as is the case with OGDI over a network. 
 * Datastores with arbitrary overviews don't generally have any fixed
 * overviews, but the RasterIO() method can be used in downsampling mode
 * to get overview data efficiently.
 *
 * This method is the same as the C function GDALHasArbitraryOverviews(),
 *
 * @return TRUE if arbitrary overviews available (efficiently), otherwise
 * FALSE. 
 */

int GDALRasterBand::HasArbitraryOverviews()

{
    return FALSE;
}

/************************************************************************/
/*                     GDALHasArbitraryOverviews()                      */
/************************************************************************/

/**
 * \brief Check for arbitrary overviews.
 *
 * @see GDALRasterBand::HasArbitraryOverviews()
 */

int CPL_STDCALL GDALHasArbitraryOverviews( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALHasArbitraryOverviews", 0 );
    
    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->HasArbitraryOverviews();
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

/**
 * \brief Return the number of overview layers available.
 *
 * This method is the same as the C function GDALGetOverviewCount().
 *
 * @return overview count, zero if none.
 */

int GDALRasterBand::GetOverviewCount()

{
    if( poDS != NULL && poDS->oOvManager.IsInitialized() )
        return poDS->oOvManager.GetOverviewCount( nBand );
    else
        return 0;
}

/************************************************************************/
/*                        GDALGetOverviewCount()                        */
/************************************************************************/

/**
 * \brief Return the number of overview layers available.
 *
 * @see GDALRasterBand::GetOverviewCount()
 */

int CPL_STDCALL GDALGetOverviewCount( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetOverviewCount", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetOverviewCount();
}


/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

/**
 * \brief Fetch overview raster band object.
 *
 * This method is the same as the C function GDALGetOverview().
 * 
 * @param i overview index between 0 and GetOverviewCount()-1.
 * 
 * @return overview GDALRasterBand.
 */

GDALRasterBand * GDALRasterBand::GetOverview( int i )

{
    if( poDS != NULL && poDS->oOvManager.IsInitialized() )
        return poDS->oOvManager.GetOverview( nBand, i );
    else
        return NULL;
}

/************************************************************************/
/*                          GDALGetOverview()                           */
/************************************************************************/

/**
 * \brief Fetch overview raster band object.
 *
 * @see GDALRasterBand::GetOverview()
 */

GDALRasterBandH CPL_STDCALL GDALGetOverview( GDALRasterBandH hBand, int i )

{
    VALIDATE_POINTER1( hBand, "GDALGetOverview", NULL );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return (GDALRasterBandH) poBand->GetOverview(i);
}

/************************************************************************/
/*                      GetRasterSampleOverview()                       */
/************************************************************************/

/**
 * \brief Fetch best sampling overview.
 *
 * Returns the most reduced overview of the given band that still satisfies
 * the desired number of samples.  This function can be used with zero
 * as the number of desired samples to fetch the most reduced overview. 
 * The same band as was passed in will be returned if it has not overviews,
 * or if none of the overviews have enough samples.
 *
 * This method is the same as the C function GDALGetRasterSampleOverview().
 *
 * @param nDesiredSamples the returned band will have at least this many 
 * pixels.
 *
 * @return optimal overview or the band itself. 
 */

GDALRasterBand *GDALRasterBand::GetRasterSampleOverview( int nDesiredSamples )

{
    double dfBestSamples = 0; 
    GDALRasterBand *poBestBand = this;

    dfBestSamples = GetXSize() * (double)GetYSize();

    for( int iOverview = 0; iOverview < GetOverviewCount(); iOverview++ )
    {
        GDALRasterBand  *poOBand = GetOverview( iOverview );
        double          dfOSamples = 0;

        if (poOBand == NULL)
            continue;

        dfOSamples = poOBand->GetXSize() * (double)poOBand->GetYSize();

        if( dfOSamples < dfBestSamples && dfOSamples > nDesiredSamples )
        {
            dfBestSamples = dfOSamples;
            poBestBand = poOBand;
        }
    }

    return poBestBand;
}

/************************************************************************/
/*                    GDALGetRasterSampleOverview()                     */
/************************************************************************/

/**
 * \brief Fetch best sampling overview.
 *
 * @see GDALRasterBand::GetRasterSampleOverview()
 */

GDALRasterBandH CPL_STDCALL 
GDALGetRasterSampleOverview( GDALRasterBandH hBand, int nDesiredSamples )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterSampleOverview", NULL );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return (GDALRasterBandH)
        poBand->GetRasterSampleOverview( nDesiredSamples );
}

/************************************************************************/
/*                           BuildOverviews()                           */
/************************************************************************/

/**
 * \brief Build raster overview(s)
 *
 * If the operation is unsupported for the indicated dataset, then 
 * CE_Failure is returned, and CPLGetLastErrorNo() will return 
 * CPLE_NotSupported.
 *
 * WARNING:  It is not possible to build overviews for a single band in
 * TIFF format, and thus this method does not work for TIFF format, or any
 * formats that use the default overview building in TIFF format.  Instead
 * it is necessary to build overviews on the dataset as a whole using
 * GDALDataset::BuildOverviews().  That makes this method pretty useless
 * from a practical point of view. 
 *
 * @param pszResampling one of "NEAREST", "GAUSS", "CUBIC", "AVERAGE", "MODE",
 * "AVERAGE_MAGPHASE" or "NONE" controlling the downsampling method applied.
 * @param nOverviews number of overviews to build. 
 * @param panOverviewList the list of overview decimation factors to build. 
 * @param pfnProgress a function to call to report progress, or NULL.
 * @param pProgressData application data to pass to the progress function.
 *
 * @return CE_None on success or CE_Failure if the operation doesn't work. 
 */

CPLErr GDALRasterBand::BuildOverviews( const char * pszResampling, 
                                       int nOverviews, 
                                       int * panOverviewList, 
                                       GDALProgressFunc pfnProgress, 
                                       void * pProgressData )

{
    (void) pszResampling;
    (void) nOverviews;
    (void) panOverviewList;
    (void) pfnProgress;
    (void) pProgressData;

    ReportError( CE_Failure, CPLE_NotSupported,
              "BuildOverviews() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

/**
 * \brief Fetch the raster value offset.
 *
 * This value (in combination with the GetScale() value) is used to
 * transform raw pixel values into the units returned by GetUnits().  
 * For example this might be used to store elevations in GUInt16 bands
 * with a precision of 0.1, and starting from -100. 
 * 
 * Units value = (raw value * scale) + offset
 * 
 * For file formats that don't know this intrinsically a value of zero
 * is returned. 
 *
 * This method is the same as the C function GDALGetRasterOffset().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is meaningful or not.  May be NULL (default).
 *
 * @return the raster offset.
 */

double GDALRasterBand::GetOffset( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    return 0.0;
}

/************************************************************************/
/*                        GDALGetRasterOffset()                         */
/************************************************************************/

/**
 * \brief Fetch the raster value offset.
 *
 * @see GDALRasterBand::GetOffset()
 */

double CPL_STDCALL GDALGetRasterOffset( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterOffset", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetOffset( pbSuccess );
}

/************************************************************************/
/*                             SetOffset()                              */
/************************************************************************/

/**
 * \brief Set scaling offset.
 *
 * Very few formats implement this method.   When not implemented it will
 * issue a CPLE_NotSupported error and return CE_Failure. 
 * 
 * @param dfNewOffset the new offset.
 *
 * @return CE_None or success or CE_Failure on failure. 
 */

CPLErr GDALRasterBand::SetOffset( double dfNewOffset )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetOffset() not supported on this raster band." );
    
    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetRasterOffset()                         */
/************************************************************************/

/**
 * \brief Set scaling offset.
 *
 * @see GDALRasterBand::SetOffset()
 */

CPLErr CPL_STDCALL 
GDALSetRasterOffset( GDALRasterBandH hBand, double dfNewOffset )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterOffset", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetOffset( dfNewOffset );
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

/**
 * \brief Fetch the raster value scale.
 *
 * This value (in combination with the GetOffset() value) is used to
 * transform raw pixel values into the units returned by GetUnits().  
 * For example this might be used to store elevations in GUInt16 bands
 * with a precision of 0.1, and starting from -100. 
 * 
 * Units value = (raw value * scale) + offset
 * 
 * For file formats that don't know this intrinsically a value of one
 * is returned. 
 *
 * This method is the same as the C function GDALGetRasterScale().
 *
 * @param pbSuccess pointer to a boolean to use to indicate if the
 * returned value is meaningful or not.  May be NULL (default).
 *
 * @return the raster scale.
 */

double GDALRasterBand::GetScale( int *pbSuccess )

{
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    return 1.0;
}

/************************************************************************/
/*                         GDALGetRasterScale()                         */
/************************************************************************/

/**
 * \brief Fetch the raster value scale.
 *
 * @see GDALRasterBand::GetScale()
 */

double CPL_STDCALL GDALGetRasterScale( GDALRasterBandH hBand, int *pbSuccess )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterScale", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetScale( pbSuccess );
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

/**
 * \brief Set scaling ratio.
 *
 * Very few formats implement this method.   When not implemented it will
 * issue a CPLE_NotSupported error and return CE_Failure. 
 * 
 * @param dfNewScale the new scale.
 *
 * @return CE_None or success or CE_Failure on failure. 
 */

CPLErr GDALRasterBand::SetScale( double dfNewScale )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetScale() not supported on this raster band." );
    
    return CE_Failure;
}

/************************************************************************/
/*                        GDALSetRasterScale()                          */
/************************************************************************/

/**
 * \brief Set scaling ratio.
 *
 * @see GDALRasterBand::SetScale()
 */

CPLErr CPL_STDCALL 
GDALSetRasterScale( GDALRasterBandH hBand, double dfNewOffset )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterScale", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetScale( dfNewOffset );
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

/**
 * \brief Return raster unit type.
 *
 * Return a name for the units of this raster's values.  For instance, it
 * might be "m" for an elevation model in meters, or "ft" for feet.  If no 
 * units are available, a value of "" will be returned.  The returned string 
 * should not be modified, nor freed by the calling application.
 *
 * This method is the same as the C function GDALGetRasterUnitType(). 
 *
 * @return unit name string.
 */

const char *GDALRasterBand::GetUnitType()

{
    return "";
}

/************************************************************************/
/*                       GDALGetRasterUnitType()                        */
/************************************************************************/

/**
 * \brief Return raster unit type.
 *
 * @see GDALRasterBand::GetUnitType()
 */

const char * CPL_STDCALL GDALGetRasterUnitType( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterUnitType", NULL );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetUnitType();
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

/**
 * \brief Set unit type.
 *
 * Set the unit type for a raster band.  Values should be one of
 * "" (the default indicating it is unknown), "m" indicating meters, 
 * or "ft" indicating feet, though other nonstandard values are allowed.
 *
 * This method is the same as the C function GDALSetRasterUnitType(). 
 *
 * @param pszNewValue the new unit type value.
 *
 * @return CE_None on success or CE_Failure if not succuessful, or 
 * unsupported.
 */

CPLErr GDALRasterBand::SetUnitType( const char *pszNewValue )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetUnitType() not supported on this raster band." );
    return CE_Failure;
}

/************************************************************************/
/*                       GDALSetRasterUnitType()                        */
/************************************************************************/

/**
 * \brief Set unit type.
 *
 * @see GDALRasterBand::SetUnitType()
 *
 * @since GDAL 1.8.0
 */

CPLErr CPL_STDCALL GDALSetRasterUnitType( GDALRasterBandH hBand, const char *pszNewValue )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterUnitType", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetUnitType(pszNewValue);
}

/************************************************************************/
/*                              GetXSize()                              */
/************************************************************************/

/**
 * \brief Fetch XSize of raster. 
 *
 * This method is the same as the C function GDALGetRasterBandXSize(). 
 *
 * @return the width in pixels of this band.
 */

int GDALRasterBand::GetXSize()

{
    return nRasterXSize;
}

/************************************************************************/
/*                       GDALGetRasterBandXSize()                       */
/************************************************************************/

/**
 * \brief Fetch XSize of raster. 
 *
 * @see GDALRasterBand::GetXSize()
 */

int CPL_STDCALL GDALGetRasterBandXSize( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterBandXSize", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetXSize();
}

/************************************************************************/
/*                              GetYSize()                              */
/************************************************************************/

/**
 * \brief Fetch YSize of raster. 
 *
 * This method is the same as the C function GDALGetRasterBandYSize(). 
 *
 * @return the height in pixels of this band.
 */

int GDALRasterBand::GetYSize()

{
    return nRasterYSize;
}

/************************************************************************/
/*                       GDALGetRasterBandYSize()                       */
/************************************************************************/

/**
 * \brief Fetch YSize of raster. 
 *
 * @see GDALRasterBand::GetYSize()
 */

int CPL_STDCALL GDALGetRasterBandYSize( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterBandYSize", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetYSize();
}

/************************************************************************/
/*                              GetBand()                               */
/************************************************************************/

/**
 * \brief Fetch the band number.
 *
 * This method returns the band that this GDALRasterBand objects represents
 * within it's dataset.  This method may return a value of 0 to indicate
 * GDALRasterBand objects without an apparently relationship to a dataset,
 * such as GDALRasterBands serving as overviews.
 *
 * This method is the same as the C function GDALGetBandNumber().
 *
 * @return band number (1+) or 0 if the band number isn't known.
 */

int GDALRasterBand::GetBand()

{
    return nBand;
}

/************************************************************************/
/*                         GDALGetBandNumber()                          */
/************************************************************************/

/**
 * \brief Fetch the band number.
 *
 * @see GDALRasterBand::GetBand()
 */

int CPL_STDCALL GDALGetBandNumber( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetBandNumber", 0 );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetBand();
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

/**
 * \brief Fetch the owning dataset handle.
 *
 * Note that some GDALRasterBands are not considered to be a part of a dataset,
 * such as overviews or other "freestanding" bands. 
 *
 * This method is the same as the C function GDALGetBandDataset()
 *
 * @return the pointer to the GDALDataset to which this band belongs, or
 * NULL if this cannot be determined.
 */

GDALDataset *GDALRasterBand::GetDataset()

{
    return poDS;
}

/************************************************************************/
/*                         GDALGetBandDataset()                         */
/************************************************************************/

/**
 * \brief Fetch the owning dataset handle.
 *
 * @see GDALRasterBand::GetDataset()
 */

GDALDatasetH CPL_STDCALL GDALGetBandDataset( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetBandDataset", NULL );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return (GDALDatasetH) poBand->GetDataset();
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

/**
 * \brief Compute raster histogram. 
 *
 * Note that the bucket size is (dfMax-dfMin) / nBuckets.  
 *
 * For example to compute a simple 256 entry histogram of eight bit data, 
 * the following would be suitable.  The unusual bounds are to ensure that
 * bucket boundaries don't fall right on integer values causing possible errors
 * due to rounding after scaling. 
<pre>
    int anHistogram[256];

    poBand->GetHistogram( -0.5, 255.5, 256, anHistogram, FALSE, FALSE, 
                          GDALDummyProgress, NULL );
</pre>
 *
 * Note that setting bApproxOK will generally result in a subsampling of the
 * file, and will utilize overviews if available.  It should generally 
 * produce a representative histogram for the data that is suitable for use
 * in generating histogram based luts for instance.  Generally bApproxOK is
 * much faster than an exactly computed histogram.
 *
 * @param dfMin the lower bound of the histogram.
 * @param dfMax the upper bound of the histogram.
 * @param nBuckets the number of buckets in panHistogram.
 * @param panHistogram array into which the histogram totals are placed.
 * @param bIncludeOutOfRange if TRUE values below the histogram range will
 * mapped into panHistogram[0], and values above will be mapped into 
 * panHistogram[nBuckets-1] otherwise out of range values are discarded.
 * @param bApproxOK TRUE if an approximate, or incomplete histogram OK.
 * @param pfnProgress function to report progress to completion. 
 * @param pProgressData application data to pass to pfnProgress. 
 *
 * @return CE_None on success, or CE_Failure if something goes wrong. 
 */

CPLErr GDALRasterBand::GetHistogram( double dfMin, double dfMax, 
                                     int nBuckets, int *panHistogram, 
                                     int bIncludeOutOfRange, int bApproxOK,
                                     GDALProgressFunc pfnProgress, 
                                     void *pProgressData )

{
    CPLAssert( NULL != panHistogram );

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      If we have overviews, use them for the histogram.               */
/* -------------------------------------------------------------------- */
    if( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        // FIXME: should we use the most reduced overview here or use some
        // minimum number of samples like GDALRasterBand::ComputeStatistics()
        // does?
        GDALRasterBand *poBestOverview = GetRasterSampleOverview( 0 );
        
        if( poBestOverview != this )
        {
            return poBestOverview->GetHistogram( dfMin, dfMax, nBuckets,
                                                 panHistogram,
                                                 bIncludeOutOfRange, bApproxOK,
                                                 pfnProgress, pProgressData );
        }
    }

/* -------------------------------------------------------------------- */
/*      Read actual data and build histogram.                           */
/* -------------------------------------------------------------------- */
    double      dfScale;

    if( !pfnProgress( 0.0, "Compute Histogram", pProgressData ) )
    {
        ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    dfScale = nBuckets / (dfMax - dfMin);
    memset( panHistogram, 0, sizeof(int) * nBuckets );

    const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    int bSignedByte = (pszPixelType != NULL && EQUAL(pszPixelType, "SIGNEDBYTE"));

    if ( bApproxOK && HasArbitraryOverviews() )
    {
/* -------------------------------------------------------------------- */
/*      Figure out how much the image should be reduced to get an       */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        void    *pData;
        int     nXReduced, nYReduced;
        double  dfReduction = sqrt(
            (double)nRasterXSize * nRasterYSize / GDALSTAT_APPROX_NUMSAMPLES );

        if ( dfReduction > 1.0 )
        {
            nXReduced = (int)( nRasterXSize / dfReduction );
            nYReduced = (int)( nRasterYSize / dfReduction );

            // Catch the case of huge resizing ratios here
            if ( nXReduced == 0 )
                nXReduced = 1;
            if ( nYReduced == 0 )
                nYReduced = 1;
        }
        else
        {
            nXReduced = nRasterXSize;
            nYReduced = nRasterYSize;
        }

        pData =
            CPLMalloc(GDALGetDataTypeSize(eDataType)/8 * nXReduced * nYReduced);

        CPLErr eErr = IRasterIO( GF_Read, 0, 0, nRasterXSize, nRasterYSize, pData,
                   nXReduced, nYReduced, eDataType, 0, 0 );
        if ( eErr != CE_None )
        {
            CPLFree(pData);
            return eErr;
        }
        
        /* this isn't the fastest way to do this, but is easier for now */
        for( int iY = 0; iY < nYReduced; iY++ )
        {
            for( int iX = 0; iX < nXReduced; iX++ )
            {
                int    iOffset = iX + iY * nXReduced;
                int    nIndex;
                double dfValue = 0.0;

                switch( eDataType )
                {
                  case GDT_Byte:
                  {
                    if (bSignedByte)
                        dfValue = ((signed char *)pData)[iOffset];
                    else
                        dfValue = ((GByte *)pData)[iOffset];
                    break;
                  }
                  case GDT_UInt16:
                    dfValue = ((GUInt16 *)pData)[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = ((GInt16 *)pData)[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = ((GUInt32 *)pData)[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = ((GInt32 *)pData)[iOffset];
                    break;
                  case GDT_Float32:
                    dfValue = ((float *)pData)[iOffset];
                    if (CPLIsNan(dfValue))
                        continue;
                    break;
                  case GDT_Float64:
                    dfValue = ((double *)pData)[iOffset];
                    if (CPLIsNan(dfValue))
                        continue;
                    break;
                  case GDT_CInt16:
                    {
                        double dfReal = ((GInt16 *)pData)[iOffset*2];
                        double dfImag = ((GInt16 *)pData)[iOffset*2+1];
                        if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                            continue;
                        dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    }
                    break;
                  case GDT_CInt32:
                    {
                        double dfReal = ((GInt32 *)pData)[iOffset*2];
                        double dfImag = ((GInt32 *)pData)[iOffset*2+1];
                        if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                            continue;
                        dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    }
                    break;
                  case GDT_CFloat32:
                    {
                        double dfReal = ((float *)pData)[iOffset*2];
                        double dfImag = ((float *)pData)[iOffset*2+1];
                        if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                            continue;
                        dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    }
                    break;
                  case GDT_CFloat64:
                    {
                        double dfReal = ((double *)pData)[iOffset*2];
                        double dfImag = ((double *)pData)[iOffset*2+1];
                        if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                            continue;
                        dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                    }
                    break;
                  default:
                    CPLAssert( FALSE );
                }

                nIndex = (int) floor((dfValue - dfMin) * dfScale);
                
                if( nIndex < 0 )
                {
                    if( bIncludeOutOfRange )
                        panHistogram[0]++;
                }
                else if( nIndex >= nBuckets )
                {
                    if( bIncludeOutOfRange )
                        panHistogram[nBuckets-1]++;
                }
                else
                {
                    panHistogram[nIndex]++;
                }
            }
        }

        CPLFree( pData );
    }

    else    // No arbitrary overviews
    {
        int         nSampleRate;

        if( !InitBlockInfo() )
            return CE_Failure;
    
/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */

        if ( bApproxOK )
        {
            nSampleRate = 
                (int) MAX(1,sqrt((double) nBlocksPerRow * nBlocksPerColumn));
        }
        else
            nSampleRate = 1;
    
/* -------------------------------------------------------------------- */
/*      Read the blocks, and add to histogram.                          */
/* -------------------------------------------------------------------- */
        for( int iSampleBlock = 0; 
             iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
             iSampleBlock += nSampleRate )
        {
            int  iXBlock, iYBlock, nXCheck, nYCheck;
            GDALRasterBlock *poBlock;
            void* pData;

            if( !pfnProgress( iSampleBlock
                              / ((double)nBlocksPerRow * nBlocksPerColumn),
                              "Compute Histogram", pProgressData ) )
                return CE_Failure;

            iYBlock = iSampleBlock / nBlocksPerRow;
            iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;
            
            poBlock = GetLockedBlockRef( iXBlock, iYBlock );
            if( poBlock == NULL )
                return CE_Failure;
            if( poBlock->GetDataRef() == NULL )
            {
                poBlock->DropLock();
                return CE_Failure;
            }
            
            pData = poBlock->GetDataRef();
            
            if( (iXBlock+1) * nBlockXSize > GetXSize() )
                nXCheck = GetXSize() - iXBlock * nBlockXSize;
            else
                nXCheck = nBlockXSize;

            if( (iYBlock+1) * nBlockYSize > GetYSize() )
                nYCheck = GetYSize() - iYBlock * nBlockYSize;
            else
                nYCheck = nBlockYSize;

            /* this is a special case for a common situation */
            if( eDataType == GDT_Byte && !bSignedByte
                && dfScale == 1.0 && (dfMin >= -0.5 && dfMin <= 0.5)
                && nYCheck == nBlockYSize && nXCheck == nBlockXSize
                && nBuckets == 256 )
            {
                int    nPixels = nXCheck * nYCheck;
                GByte  *pabyData = (GByte *) pData;
                
                for( int i = 0; i < nPixels; i++ )
                    panHistogram[pabyData[i]]++;

                poBlock->DropLock();
                continue; /* to next sample block */
            }

            /* this isn't the fastest way to do this, but is easier for now */
            for( int iY = 0; iY < nYCheck; iY++ )
            {
                for( int iX = 0; iX < nXCheck; iX++ )
                {
                    int    iOffset = iX + iY * nBlockXSize;
                    int    nIndex;
                    double dfValue = 0.0;

                    switch( eDataType )
                    {
                      case GDT_Byte:
                      {
                        if (bSignedByte)
                            dfValue = ((signed char *) pData)[iOffset];
                        else
                            dfValue = ((GByte *) pData)[iOffset];
                        break;
                      }
                      case GDT_UInt16:
                        dfValue = ((GUInt16 *) pData)[iOffset];
                        break;
                      case GDT_Int16:
                        dfValue = ((GInt16 *) pData)[iOffset];
                        break;
                      case GDT_UInt32:
                        dfValue = ((GUInt32 *) pData)[iOffset];
                        break;
                      case GDT_Int32:
                        dfValue = ((GInt32 *) pData)[iOffset];
                        break;
                      case GDT_Float32:
                        dfValue = ((float *) pData)[iOffset];
                        if (CPLIsNan(dfValue))
                            continue;
                        break;
                      case GDT_Float64:
                        dfValue = ((double *) pData)[iOffset];
                        if (CPLIsNan(dfValue))
                            continue;
                        break;
                      case GDT_CInt16:
                        {
                            double  dfReal =
                                ((GInt16 *) pData)[iOffset*2];
                            double  dfImag =
                                ((GInt16 *) pData)[iOffset*2+1];
                            dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                        }
                        break;
                      case GDT_CInt32:
                        {
                            double  dfReal =
                                ((GInt32 *) pData)[iOffset*2];
                            double  dfImag =
                                ((GInt32 *) pData)[iOffset*2+1];
                            dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                        }
                        break;
                      case GDT_CFloat32:
                        {
                            double  dfReal =
                                ((float *) pData)[iOffset*2];
                            double  dfImag =
                                ((float *) pData)[iOffset*2+1];
                            if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                                continue;
                            dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                        }
                        break;
                      case GDT_CFloat64:
                        {
                            double  dfReal =
                                ((double *) pData)[iOffset*2];
                            double  dfImag =
                                ((double *) pData)[iOffset*2+1];
                            if ( CPLIsNan(dfReal) || CPLIsNan(dfImag) )
                                continue;
                            dfValue = sqrt( dfReal * dfReal + dfImag * dfImag );
                        }
                        break;
                      default:
                        CPLAssert( FALSE );
                        return CE_Failure;
                    }
                    
                    nIndex = (int) floor((dfValue - dfMin) * dfScale);

                    if( nIndex < 0 )
                    {
                        if( bIncludeOutOfRange )
                            panHistogram[0]++;
                    }
                    else if( nIndex >= nBuckets )
                    {
                        if( bIncludeOutOfRange )
                            panHistogram[nBuckets-1]++;
                    }
                    else
                    {
                        panHistogram[nIndex]++;
                    }
                }
            }

            poBlock->DropLock();
        }
    }

    pfnProgress( 1.0, "Compute Histogram", pProgressData );

    return CE_None;
}

/************************************************************************/
/*                       GDALGetRasterHistogram()                       */
/************************************************************************/

/**
 * \brief Compute raster histogram. 
 *
 * @see GDALRasterBand::GetHistogram()
 */

CPLErr CPL_STDCALL 
GDALGetRasterHistogram( GDALRasterBandH hBand, 
                        double dfMin, double dfMax, 
                        int nBuckets, int *panHistogram, 
                        int bIncludeOutOfRange, int bApproxOK,
                        GDALProgressFunc pfnProgress, 
                        void *pProgressData )
    
{
    VALIDATE_POINTER1( hBand, "GDALGetRasterHistogram", CE_Failure );
    VALIDATE_POINTER1( panHistogram, "GDALGetRasterHistogram", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);

    return poBand->GetHistogram( dfMin, dfMax, nBuckets, panHistogram, 
                      bIncludeOutOfRange, bApproxOK,
                      pfnProgress, pProgressData );
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

/**
 * \brief Fetch default raster histogram. 
 *
 * The default method in GDALRasterBand will compute a default histogram. This
 * method is overriden by derived classes (such as GDALPamRasterBand, VRTDataset, HFADataset...)
 * that may be able to fetch efficiently an already stored histogram.
 *
 * @param pdfMin pointer to double value that will contain the lower bound of the histogram.
 * @param pdfMax pointer to double value that will contain the upper bound of the histogram.
 * @param pnBuckets pointer to int value that will contain the number of buckets in *ppanHistogram.
 * @param ppanHistogram pointer to array into which the histogram totals are placed. To be freed with VSIFree
 * @param bForce TRUE to force the computation. If FALSE and no default histogram is available, the method will return CE_Warning
 * @param pfnProgress function to report progress to completion. 
 * @param pProgressData application data to pass to pfnProgress. 
 *
 * @return CE_None on success, CE_Failure if something goes wrong, or 
 * CE_Warning if no default histogram is available.
 */

CPLErr 
    GDALRasterBand::GetDefaultHistogram( double *pdfMin, double *pdfMax, 
                                         int *pnBuckets, int **ppanHistogram, 
                                         int bForce,
                                         GDALProgressFunc pfnProgress, 
                                         void *pProgressData )

{
    CPLAssert( NULL != pnBuckets );
    CPLAssert( NULL != ppanHistogram );
    CPLAssert( NULL != pdfMin );
    CPLAssert( NULL != pdfMax );

    *pnBuckets = 0;
    *ppanHistogram = NULL;

    if( !bForce )
        return CE_Warning;

    int nBuckets = 256;
    
    const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    int bSignedByte = (pszPixelType != NULL && EQUAL(pszPixelType, "SIGNEDBYTE"));

    if( GetRasterDataType() == GDT_Byte && !bSignedByte)
    {
        *pdfMin = -0.5;
        *pdfMax = 255.5;
    }
    else
    {
        CPLErr eErr = CE_Failure;
        double dfHalfBucket = 0;

        eErr = GetStatistics( TRUE, TRUE, pdfMin, pdfMax, NULL, NULL );
        dfHalfBucket = (*pdfMax - *pdfMin) / (2 * nBuckets);
        *pdfMin -= dfHalfBucket;
        *pdfMax += dfHalfBucket;

        if( eErr != CE_None )
            return eErr;
    }

    *ppanHistogram = (int *) VSICalloc(sizeof(int), nBuckets);
    if( *ppanHistogram == NULL )
    {
        ReportError( CE_Failure, CPLE_OutOfMemory,
                  "Out of memory in InitBlockInfo()." );
        return CE_Failure;
    }

    *pnBuckets = nBuckets;
    return GetHistogram( *pdfMin, *pdfMax, *pnBuckets, *ppanHistogram, 
                         TRUE, FALSE, pfnProgress, pProgressData );
}

/************************************************************************/
/*                      GDALGetDefaultHistogram()                       */
/************************************************************************/

/**
  * \brief Fetch default raster histogram. 
  *
  * @see GDALRasterBand::GetDefaultHistogram()
  */

CPLErr CPL_STDCALL GDALGetDefaultHistogram( GDALRasterBandH hBand, 
                                double *pdfMin, double *pdfMax, 
                                int *pnBuckets, int **ppanHistogram, 
                                int bForce,
                                GDALProgressFunc pfnProgress, 
                                void *pProgressData )

{
    VALIDATE_POINTER1( hBand, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pdfMin, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pdfMax, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( pnBuckets, "GDALGetDefaultHistogram", CE_Failure );
    VALIDATE_POINTER1( ppanHistogram, "GDALGetDefaultHistogram", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetDefaultHistogram( pdfMin, pdfMax,
        pnBuckets, ppanHistogram, bForce, pfnProgress, pProgressData );
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

/**
 * \brief Advise driver of upcoming read requests.
 *
 * Some GDAL drivers operate more efficiently if they know in advance what 
 * set of upcoming read requests will be made.  The AdviseRead() method allows
 * an application to notify the driver of the region of interest, 
 * and at what resolution the region will be read.  
 *
 * Many drivers just ignore the AdviseRead() call, but it can dramatically
 * accelerate access via some drivers.  
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
 * @param eBufType the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param papszOptions a list of name=value strings with special control 
 * options.  Normally this is NULL.
 *
 * @return CE_Failure if the request is invalid and CE_None if it works or
 * is ignored. 
 */

CPLErr GDALRasterBand::AdviseRead( 
    int nXOff, int nYOff, int nXSize, int nYSize,
    int nBufXSize, int nBufYSize, GDALDataType eBufType, char **papszOptions )

{
    return CE_None;
}

/************************************************************************/
/*                        GDALRasterAdviseRead()                        */
/************************************************************************/


/**
 * \brief Advise driver of upcoming read requests.
 *
 * @see GDALRasterBand::AdviseRead()
 */

CPLErr CPL_STDCALL 
GDALRasterAdviseRead( GDALRasterBandH hBand, 
                      int nXOff, int nYOff, int nXSize, int nYSize,
                      int nBufXSize, int nBufYSize, 
                      GDALDataType eDT, char **papszOptions )
    
{
    VALIDATE_POINTER1( hBand, "GDALRasterAdviseRead", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->AdviseRead( nXOff, nYOff, nXSize, nYSize, 
        nBufXSize, nBufYSize, eDT, papszOptions );
}

/************************************************************************/
/*                           GetStatistics()                            */
/************************************************************************/

/**
 * \brief Fetch image statistics. 
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this band.  If approximate statistics are sufficient,
 * the bApproxOK flag can be set to true in which case overviews, or a
 * subset of image tiles may be used in computing the statistics.  
 *
 * If bForce is FALSE results will only be returned if it can be done 
 * quickly (ie. without scanning the data).  If bForce is FALSE and 
 * results cannot be returned efficiently, the method will return CE_Warning
 * but no warning will have been issued.   This is a non-standard use of
 * the CE_Warning return value to indicate "nothing done". 
 *
 * Note that file formats using PAM (Persistent Auxilary Metadata) services
 * will generally cache statistics in the .pam file allowing fast fetch
 * after the first request. 
 *
 * This method is the same as the C function GDALGetRasterStatistics().
 *
 * @param bApproxOK If TRUE statistics may be computed based on overviews
 * or a subset of all tiles. 
 * 
 * @param bForce If FALSE statistics will only be returned if it can
 * be done without rescanning the image. 
 *
 * @param pdfMin Location into which to load image minimum (may be NULL).
 *
 * @param pdfMax Location into which to load image maximum (may be NULL).-
 *
 * @param pdfMean Location into which to load image mean (may be NULL).
 *
 * @param pdfStdDev Location into which to load image standard deviation 
 * (may be NULL).
 *
 * @return CE_None on success, CE_Warning if no values returned, 
 * CE_Failure if an error occurs.
 */

CPLErr GDALRasterBand::GetStatistics( int bApproxOK, int bForce,
                                      double *pdfMin, double *pdfMax, 
                                      double *pdfMean, double *pdfStdDev )

{
    double       dfMin=0.0, dfMax=0.0;

/* -------------------------------------------------------------------- */
/*      Do we already have metadata items for the requested values?     */
/* -------------------------------------------------------------------- */
    if( (pdfMin == NULL || GetMetadataItem("STATISTICS_MINIMUM") != NULL)
     && (pdfMax == NULL || GetMetadataItem("STATISTICS_MAXIMUM") != NULL)
     && (pdfMean == NULL || GetMetadataItem("STATISTICS_MEAN") != NULL)
     && (pdfStdDev == NULL || GetMetadataItem("STATISTICS_STDDEV") != NULL) )
    {
        if( pdfMin != NULL )
            *pdfMin = CPLAtofM(GetMetadataItem("STATISTICS_MINIMUM"));
        if( pdfMax != NULL )
            *pdfMax = CPLAtofM(GetMetadataItem("STATISTICS_MAXIMUM"));
        if( pdfMean != NULL )
            *pdfMean = CPLAtofM(GetMetadataItem("STATISTICS_MEAN"));
        if( pdfStdDev != NULL )
            *pdfStdDev = CPLAtofM(GetMetadataItem("STATISTICS_STDDEV"));

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Does the driver already know the min/max?                       */
/* -------------------------------------------------------------------- */
    if( bApproxOK && pdfMean == NULL && pdfStdDev == NULL )
    {
        int          bSuccessMin, bSuccessMax;

        dfMin = GetMinimum( &bSuccessMin );
        dfMax = GetMaximum( &bSuccessMax );

        if( bSuccessMin && bSuccessMax )
        {
            if( pdfMin != NULL )
                *pdfMin = dfMin;
            if( pdfMax != NULL )
                *pdfMax = dfMax;
            return CE_None;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Either return without results, or force computation.            */
/* -------------------------------------------------------------------- */
    if( !bForce )
        return CE_Warning;
    else
        return ComputeStatistics( bApproxOK, 
                                  pdfMin, pdfMax, pdfMean, pdfStdDev,
                                  GDALDummyProgress, NULL );
}

/************************************************************************/
/*                      GDALGetRasterStatistics()                       */
/************************************************************************/

/**
 * \brief Fetch image statistics. 
 *
 * @see GDALRasterBand::GetStatistics()
 */

CPLErr CPL_STDCALL GDALGetRasterStatistics( 
        GDALRasterBandH hBand, int bApproxOK, int bForce, 
        double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev )

{
    VALIDATE_POINTER1( hBand, "GDALGetRasterStatistics", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetStatistics( 
        bApproxOK, bForce, pdfMin, pdfMax, pdfMean, pdfStdDev );
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

/**
 * \brief Compute image statistics. 
 *
 * Returns the minimum, maximum, mean and standard deviation of all
 * pixel values in this band.  If approximate statistics are sufficient,
 * the bApproxOK flag can be set to true in which case overviews, or a
 * subset of image tiles may be used in computing the statistics.  
 *
 * Once computed, the statistics will generally be "set" back on the 
 * raster band using SetStatistics(). 
 *
 * This method is the same as the C function GDALComputeRasterStatistics().
 *
 * @param bApproxOK If TRUE statistics may be computed based on overviews
 * or a subset of all tiles. 
 * 
 * @param pdfMin Location into which to load image minimum (may be NULL).
 *
 * @param pdfMax Location into which to load image maximum (may be NULL).-
 *
 * @param pdfMean Location into which to load image mean (may be NULL).
 *
 * @param pdfStdDev Location into which to load image standard deviation 
 * (may be NULL).
 *
 * @param pfnProgress a function to call to report progress, or NULL.
 *
 * @param pProgressData application data to pass to the progress function.
 *
 * @return CE_None on success, or CE_Failure if an error occurs or processing
 * is terminated by the user.
 */

CPLErr 
GDALRasterBand::ComputeStatistics( int bApproxOK,
                                   double *pdfMin, double *pdfMax, 
                                   double *pdfMean, double *pdfStdDev,
                                   GDALProgressFunc pfnProgress, 
                                   void *pProgressData )

{
    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

/* -------------------------------------------------------------------- */
/*      If we have overview bands, use them for statistics.             */
/* -------------------------------------------------------------------- */
    if( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        GDALRasterBand *poBand;

        poBand = GetRasterSampleOverview( GDALSTAT_APPROX_NUMSAMPLES );

        if( poBand != this )
            return poBand->ComputeStatistics( FALSE,  
                                              pdfMin, pdfMax, 
                                              pdfMean, pdfStdDev,
                                              pfnProgress, pProgressData );
    }

/* -------------------------------------------------------------------- */
/*      Read actual data and compute statistics.                        */
/* -------------------------------------------------------------------- */
    double      dfMin = 0.0, dfMax = 0.0;
    int         bGotNoDataValue, bFirstValue = TRUE;
    double      dfNoDataValue, dfSum = 0.0, dfSum2 = 0.0;
    GIntBig     nSampleCount = 0;

    if( !pfnProgress( 0.0, "Compute Statistics", pProgressData ) )
    {
        ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    dfNoDataValue = GetNoDataValue( &bGotNoDataValue );
    bGotNoDataValue = bGotNoDataValue && !CPLIsNan(dfNoDataValue);

    const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    int bSignedByte = (pszPixelType != NULL && EQUAL(pszPixelType, "SIGNEDBYTE"));
    
    if ( bApproxOK && HasArbitraryOverviews() )
    {
/* -------------------------------------------------------------------- */
/*      Figure out how much the image should be reduced to get an       */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        void    *pData;
        int     nXReduced, nYReduced;
        double  dfReduction = sqrt(
            (double)nRasterXSize * nRasterYSize / GDALSTAT_APPROX_NUMSAMPLES );

        if ( dfReduction > 1.0 )
        {
            nXReduced = (int)( nRasterXSize / dfReduction );
            nYReduced = (int)( nRasterYSize / dfReduction );

            // Catch the case of huge resizing ratios here
            if ( nXReduced == 0 )
                nXReduced = 1;
            if ( nYReduced == 0 )
                nYReduced = 1;
        }
        else
        {
            nXReduced = nRasterXSize;
            nYReduced = nRasterYSize;
        }

        pData =
            CPLMalloc(GDALGetDataTypeSize(eDataType)/8 * nXReduced * nYReduced);

        CPLErr eErr = IRasterIO( GF_Read, 0, 0, nRasterXSize, nRasterYSize, pData,
                   nXReduced, nYReduced, eDataType, 0, 0 );
        if ( eErr != CE_None )
        {
            CPLFree(pData);
            return eErr;
        }

        /* this isn't the fastest way to do this, but is easier for now */
        for( int iY = 0; iY < nYReduced; iY++ )
        {
            for( int iX = 0; iX < nXReduced; iX++ )
            {
                int    iOffset = iX + iY * nXReduced;
                double dfValue = 0.0;

                switch( eDataType )
                {
                  case GDT_Byte:
                  {
                    if (bSignedByte)
                        dfValue = ((signed char *)pData)[iOffset];
                    else
                        dfValue = ((GByte *)pData)[iOffset];
                    break;
                  }
                  case GDT_UInt16:
                    dfValue = ((GUInt16 *)pData)[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = ((GInt16 *)pData)[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = ((GUInt32 *)pData)[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = ((GInt32 *)pData)[iOffset];
                    break;
                  case GDT_Float32:
                    dfValue = ((float *)pData)[iOffset];
                    if (CPLIsNan(dfValue))
                        continue;
                    break;
                  case GDT_Float64:
                    dfValue = ((double *)pData)[iOffset];
                    if (CPLIsNan(dfValue))
                        continue;
                    break;
                  case GDT_CInt16:
                    dfValue = ((GInt16 *)pData)[iOffset*2];
                    break;
                  case GDT_CInt32:
                    dfValue = ((GInt32 *)pData)[iOffset*2];
                    break;
                  case GDT_CFloat32:
                    dfValue = ((float *)pData)[iOffset*2];
                    if( CPLIsNan(dfValue) )
                        continue;
                    break;
                  case GDT_CFloat64:
                    dfValue = ((double *)pData)[iOffset*2];
                    if( CPLIsNan(dfValue) )
                        continue;
                    break;
                  default:
                    CPLAssert( FALSE );
                }
                
                if( bGotNoDataValue && ARE_REAL_EQUAL(dfValue, dfNoDataValue) )
                    continue;

                if( bFirstValue )
                {
                    dfMin = dfMax = dfValue;
                    bFirstValue = FALSE;
                }
                else
                {
                    dfMin = MIN(dfMin,dfValue);
                    dfMax = MAX(dfMax,dfValue);
                }

                dfSum += dfValue;
                dfSum2 += dfValue * dfValue;

                nSampleCount++;
            }
        }

        CPLFree( pData );
    }

    else    // No arbitrary overviews
    {
        int     nSampleRate;
        
        if( !InitBlockInfo() )
            return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        if ( bApproxOK )
        {
            nSampleRate = 
                (int)MAX( 1, sqrt((double)nBlocksPerRow * nBlocksPerColumn) );
        }
        else
            nSampleRate = 1;

        for( int iSampleBlock = 0; 
             iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
             iSampleBlock += nSampleRate )
        {
            int  iXBlock, iYBlock, nXCheck, nYCheck;
            GDALRasterBlock *poBlock;
            void* pData;

            iYBlock = iSampleBlock / nBlocksPerRow;
            iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;
            
            poBlock = GetLockedBlockRef( iXBlock, iYBlock );
            if( poBlock == NULL )
                continue;
            if( poBlock->GetDataRef() == NULL )
            {
                poBlock->DropLock();
                continue;
            }
            
            pData = poBlock->GetDataRef();
            
            if( (iXBlock+1) * nBlockXSize > GetXSize() )
                nXCheck = GetXSize() - iXBlock * nBlockXSize;
            else
                nXCheck = nBlockXSize;

            if( (iYBlock+1) * nBlockYSize > GetYSize() )
                nYCheck = GetYSize() - iYBlock * nBlockYSize;
            else
                nYCheck = nBlockYSize;

            /* this isn't the fastest way to do this, but is easier for now */
            for( int iY = 0; iY < nYCheck; iY++ )
            {
                for( int iX = 0; iX < nXCheck; iX++ )
                {
                    int    iOffset = iX + iY * nBlockXSize;
                    double dfValue = 0.0;

                    switch( eDataType )
                    {
                      case GDT_Byte:
                      {
                        if (bSignedByte)
                            dfValue = ((signed char *)pData)[iOffset];
                        else
                            dfValue = ((GByte *)pData)[iOffset];
                        break;
                      }
                      case GDT_UInt16:
                        dfValue = ((GUInt16 *)pData)[iOffset];
                        break;
                      case GDT_Int16:
                        dfValue = ((GInt16 *)pData)[iOffset];
                        break;
                      case GDT_UInt32:
                        dfValue = ((GUInt32 *)pData)[iOffset];
                        break;
                      case GDT_Int32:
                        dfValue = ((GInt32 *)pData)[iOffset];
                        break;
                      case GDT_Float32:
                        dfValue = ((float *)pData)[iOffset];
                        if (CPLIsNan(dfValue))
                            continue;
                        break;
                      case GDT_Float64:
                        dfValue = ((double *)pData)[iOffset];
                        if (CPLIsNan(dfValue))
                            continue;
                        break;
                      case GDT_CInt16:
                        dfValue = ((GInt16 *)pData)[iOffset*2];
                        break;
                      case GDT_CInt32:
                        dfValue = ((GInt32 *)pData)[iOffset*2];
                        break;
                      case GDT_CFloat32:
                        dfValue = ((float *)pData)[iOffset*2];
                        if( CPLIsNan(dfValue) )
                            continue;
                        break;
                      case GDT_CFloat64:
                        dfValue = ((double *)pData)[iOffset*2];
                        if( CPLIsNan(dfValue) )
                            continue;
                        break;
                      default:
                        CPLAssert( FALSE );
                    }

                    if( bGotNoDataValue && ARE_REAL_EQUAL(dfValue, dfNoDataValue) )
                        continue;

                    if( bFirstValue )
                    {
                        dfMin = dfMax = dfValue;
                        bFirstValue = FALSE;
                    }
                    else
                    {
                        dfMin = MIN(dfMin,dfValue);
                        dfMax = MAX(dfMax,dfValue);
                    }

                    dfSum += dfValue;
                    dfSum2 += dfValue * dfValue;

                    nSampleCount++;
                }
            }

            poBlock->DropLock();

            if ( !pfnProgress(iSampleBlock
                              / ((double)(nBlocksPerRow*nBlocksPerColumn)),
                              "Compute Statistics", pProgressData) )
            {
                ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                return CE_Failure;
            }
        }
    }

    if( !pfnProgress( 1.0, "Compute Statistics", pProgressData ) )
    {
        ReportError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Save computed information.                                      */
/* -------------------------------------------------------------------- */
    double dfMean = dfSum / nSampleCount;
    double dfStdDev = sqrt((dfSum2 / nSampleCount) - (dfMean * dfMean));

    if( nSampleCount > 0 )
        SetStatistics( dfMin, dfMax, dfMean, dfStdDev );

/* -------------------------------------------------------------------- */
/*      Record results.                                                 */
/* -------------------------------------------------------------------- */
    if( pdfMin != NULL )
        *pdfMin = dfMin;
    if( pdfMax != NULL )
        *pdfMax = dfMax;

    if( pdfMean != NULL )
        *pdfMean = dfMean;

    if( pdfStdDev != NULL )
        *pdfStdDev = dfStdDev;

    if( nSampleCount > 0 )
        return CE_None;
    else
    {
        ReportError( CE_Failure, CPLE_AppDefined,
        "Failed to compute statistics, no valid pixels found in sampling." );
        return CE_Failure;
    }
}

/************************************************************************/
/*                    GDALComputeRasterStatistics()                     */
/************************************************************************/

/**
  * \brief Compute image statistics. 
  *
  * @see GDALRasterBand::ComputeStatistics()
  */

CPLErr CPL_STDCALL GDALComputeRasterStatistics( 
        GDALRasterBandH hBand, int bApproxOK, 
        double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev,
        GDALProgressFunc pfnProgress, void *pProgressData )

{
    VALIDATE_POINTER1( hBand, "GDALComputeRasterStatistics", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);

    return poBand->ComputeStatistics( 
        bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev,
        pfnProgress, pProgressData );
}

/************************************************************************/
/*                           SetStatistics()                            */
/************************************************************************/

/**
 * \brief Set statistics on band.
 *
 * This method can be used to store min/max/mean/standard deviation
 * statistics on a raster band.  
 *
 * The default implementation stores them as metadata, and will only work 
 * on formats that can save arbitrary metadata.  This method cannot detect
 * whether metadata will be properly saved and so may return CE_None even
 * if the statistics will never be saved.
 *
 * This method is the same as the C function GDALSetRasterStatistics().
 * 
 * @param dfMin minimum pixel value.
 * 
 * @param dfMax maximum pixel value.
 *
 * @param dfMean mean (average) of all pixel values.		
 *
 * @param dfStdDev Standard deviation of all pixel values.
 *
 * @return CE_None on success or CE_Failure on failure. 
 */

CPLErr GDALRasterBand::SetStatistics( double dfMin, double dfMax, 
                                      double dfMean, double dfStdDev )

{
    char szValue[128] = { 0 };

    sprintf( szValue, "%.14g", dfMin );
    SetMetadataItem( "STATISTICS_MINIMUM", szValue );

    sprintf( szValue, "%.14g", dfMax );
    SetMetadataItem( "STATISTICS_MAXIMUM", szValue );

    sprintf( szValue, "%.14g", dfMean );
    SetMetadataItem( "STATISTICS_MEAN", szValue );

    sprintf( szValue, "%.14g", dfStdDev );
    SetMetadataItem( "STATISTICS_STDDEV", szValue );

    return CE_None;
}

/************************************************************************/
/*                      GDALSetRasterStatistics()                       */
/************************************************************************/

/**
 * \brief Set statistics on band.
 *
 * @see GDALRasterBand::SetStatistics()
 */

CPLErr CPL_STDCALL GDALSetRasterStatistics( 
        GDALRasterBandH hBand,  
        double dfMin, double dfMax, double dfMean, double dfStdDev )

{
    VALIDATE_POINTER1( hBand, "GDALSetRasterStatistics", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetStatistics( dfMin, dfMax, dfMean, dfStdDev );
}

/************************************************************************/
/*                        ComputeRasterMinMax()                         */
/************************************************************************/

/**
 * \brief Compute the min/max values for a band.
 * 
 * If approximate is OK, then the band's GetMinimum()/GetMaximum() will
 * be trusted.  If it doesn't work, a subsample of blocks will be read to
 * get an approximate min/max.  If the band has a nodata value it will
 * be excluded from the minimum and maximum.
 *
 * If bApprox is FALSE, then all pixels will be read and used to compute
 * an exact range.
 *
 * This method is the same as the C function GDALComputeRasterMinMax().
 * 
 * @param bApproxOK TRUE if an approximate (faster) answer is OK, otherwise
 * FALSE.
 * @param adfMinMax the array in which the minimum (adfMinMax[0]) and the
 * maximum (adfMinMax[1]) are returned.
 *
 * @return CE_None on success or CE_Failure on failure.
 */


CPLErr GDALRasterBand::ComputeRasterMinMax( int bApproxOK,
                                            double adfMinMax[2] )
{
    double  dfMin = 0.0;
    double  dfMax = 0.0;

/* -------------------------------------------------------------------- */
/*      Does the driver already know the min/max?                       */
/* -------------------------------------------------------------------- */
    if( bApproxOK )
    {
        int          bSuccessMin, bSuccessMax;

        dfMin = GetMinimum( &bSuccessMin );
        dfMax = GetMaximum( &bSuccessMax );

        if( bSuccessMin && bSuccessMax )
        {
            adfMinMax[0] = dfMin;
            adfMinMax[1] = dfMax;
            return CE_None;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      If we have overview bands, use them for min/max.                */
/* -------------------------------------------------------------------- */
    if ( bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews() )
    {
        GDALRasterBand *poBand;

        poBand = GetRasterSampleOverview( GDALSTAT_APPROX_NUMSAMPLES );

        if ( poBand != this )
            return poBand->ComputeRasterMinMax( FALSE, adfMinMax );
    }
    
/* -------------------------------------------------------------------- */
/*      Read actual data and compute minimum and maximum.               */
/* -------------------------------------------------------------------- */
    int     bGotNoDataValue, bFirstValue = TRUE;
    double  dfNoDataValue;

    dfNoDataValue = GetNoDataValue( &bGotNoDataValue );
    bGotNoDataValue = bGotNoDataValue && !CPLIsNan(dfNoDataValue);

    const char* pszPixelType = GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    int bSignedByte = (pszPixelType != NULL && EQUAL(pszPixelType, "SIGNEDBYTE"));
    
    if ( bApproxOK && HasArbitraryOverviews() )
    {
/* -------------------------------------------------------------------- */
/*      Figure out how much the image should be reduced to get an       */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        void    *pData;
        int     nXReduced, nYReduced;
        double  dfReduction = sqrt(
            (double)nRasterXSize * nRasterYSize / GDALSTAT_APPROX_NUMSAMPLES );

        if ( dfReduction > 1.0 )
        {
            nXReduced = (int)( nRasterXSize / dfReduction );
            nYReduced = (int)( nRasterYSize / dfReduction );

            // Catch the case of huge resizing ratios here
            if ( nXReduced == 0 )
                nXReduced = 1;
            if ( nYReduced == 0 )
                nYReduced = 1;
        }
        else
        {
            nXReduced = nRasterXSize;
            nYReduced = nRasterYSize;
        }

        pData =
            CPLMalloc(GDALGetDataTypeSize(eDataType)/8 * nXReduced * nYReduced);

        CPLErr eErr = IRasterIO( GF_Read, 0, 0, nRasterXSize, nRasterYSize, pData,
                   nXReduced, nYReduced, eDataType, 0, 0 );
        if ( eErr != CE_None )
        {
            CPLFree(pData);
            return eErr;
        }
        
        /* this isn't the fastest way to do this, but is easier for now */
        for( int iY = 0; iY < nYReduced; iY++ )
        {
            for( int iX = 0; iX < nXReduced; iX++ )
            {
                int    iOffset = iX + iY * nXReduced;
                double dfValue = 0.0;

                switch( eDataType )
                {
                  case GDT_Byte:
                  {
                    if (bSignedByte)
                        dfValue = ((signed char *)pData)[iOffset];
                    else
                        dfValue = ((GByte *)pData)[iOffset];
                    break;
                  }
                  case GDT_UInt16:
                    dfValue = ((GUInt16 *)pData)[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = ((GInt16 *)pData)[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = ((GUInt32 *)pData)[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = ((GInt32 *)pData)[iOffset];
                    break;
                  case GDT_Float32:
                    dfValue = ((float *)pData)[iOffset];
                    if (CPLIsNan(dfValue))
                        continue;
                    break;
                  case GDT_Float64:
                    dfValue = ((double *)pData)[iOffset];
                    if (CPLIsNan(dfValue))
                        continue;
                    break;
                  case GDT_CInt16:
                    dfValue = ((GInt16 *)pData)[iOffset*2];
                    break;
                  case GDT_CInt32:
                    dfValue = ((GInt32 *)pData)[iOffset*2];
                    break;
                  case GDT_CFloat32:
                    dfValue = ((float *)pData)[iOffset*2];
                    if( CPLIsNan(dfValue) )
                        continue;
                    break;
                  case GDT_CFloat64:
                    dfValue = ((double *)pData)[iOffset*2];
                    if( CPLIsNan(dfValue) )
                        continue;
                    break;
                  default:
                    CPLAssert( FALSE );
                }
                
                if( bGotNoDataValue && ARE_REAL_EQUAL(dfValue, dfNoDataValue) )
                    continue;

                if( bFirstValue )
                {
                    dfMin = dfMax = dfValue;
                    bFirstValue = FALSE;
                }
                else
                {
                    dfMin = MIN(dfMin,dfValue);
                    dfMax = MAX(dfMax,dfValue);
                }
            }
        }

        CPLFree( pData );
    }

    else    // No arbitrary overviews
    {
        int     nSampleRate;

        if( !InitBlockInfo() )
            return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
        if ( bApproxOK )
        {
            nSampleRate = 
                (int) MAX(1,sqrt((double) nBlocksPerRow * nBlocksPerColumn));
        }
        else
            nSampleRate = 1;
        
        for( int iSampleBlock = 0; 
             iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
             iSampleBlock += nSampleRate )
        {
            int  iXBlock, iYBlock, nXCheck, nYCheck;
            GDALRasterBlock *poBlock;
            void* pData;

            iYBlock = iSampleBlock / nBlocksPerRow;
            iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;
            
            poBlock = GetLockedBlockRef( iXBlock, iYBlock );
            if( poBlock == NULL )
                continue;
            if( poBlock->GetDataRef() == NULL )
            {
                poBlock->DropLock();
                continue;
            }
            
            pData = poBlock->GetDataRef();
            
            if( (iXBlock+1) * nBlockXSize > GetXSize() )
                nXCheck = GetXSize() - iXBlock * nBlockXSize;
            else
                nXCheck = nBlockXSize;

            if( (iYBlock+1) * nBlockYSize > GetYSize() )
                nYCheck = GetYSize() - iYBlock * nBlockYSize;
            else
                nYCheck = nBlockYSize;
                
            /* this isn't the fastest way to do this, but is easier for now */
            for( int iY = 0; iY < nYCheck; iY++ )
            {
                for( int iX = 0; iX < nXCheck; iX++ )
                {
                    int    iOffset = iX + iY * nBlockXSize;
                    double dfValue = 0.0;

                    switch( eDataType )
                    {
                      case GDT_Byte:
                      {
                        if (bSignedByte)
                            dfValue = ((signed char *) pData)[iOffset];
                        else
                            dfValue = ((GByte *) pData)[iOffset];
                        break;
                      }
                      case GDT_UInt16:
                        dfValue = ((GUInt16 *) pData)[iOffset];
                        break;
                      case GDT_Int16:
                        dfValue = ((GInt16 *) pData)[iOffset];
                        break;
                      case GDT_UInt32:
                        dfValue = ((GUInt32 *) pData)[iOffset];
                        break;
                      case GDT_Int32:
                        dfValue = ((GInt32 *) pData)[iOffset];
                        break;
                      case GDT_Float32:
                        dfValue = ((float *) pData)[iOffset];
                        if( CPLIsNan(dfValue) )
                            continue;
                        break;
                      case GDT_Float64:
                        dfValue = ((double *) pData)[iOffset];
                        if( CPLIsNan(dfValue) )
                            continue;
                        break;
                      case GDT_CInt16:
                        dfValue = ((GInt16 *) pData)[iOffset*2];
                        break;
                      case GDT_CInt32:
                        dfValue = ((GInt32 *) pData)[iOffset*2];
                        break;
                      case GDT_CFloat32:
                        dfValue = ((float *) pData)[iOffset*2];
                        if( CPLIsNan(dfValue) )
                            continue;
                        break;
                      case GDT_CFloat64:
                        dfValue = ((double *) pData)[iOffset*2];
                        if( CPLIsNan(dfValue) )
                            continue;
                        break;
                      default:
                        CPLAssert( FALSE );
                    }
                    
                    if( bGotNoDataValue && ARE_REAL_EQUAL(dfValue, dfNoDataValue) )
                        continue;

                    if( bFirstValue )
                    {
                        dfMin = dfMax = dfValue;
                        bFirstValue = FALSE;
                    }
                    else
                    {
                        dfMin = MIN(dfMin,dfValue);
                        dfMax = MAX(dfMax,dfValue);
                    }
                }
            }

            poBlock->DropLock();
        }
    }

    adfMinMax[0] = dfMin;
    adfMinMax[1] = dfMax;

    if (bFirstValue)
    {
        ReportError( CE_Failure, CPLE_AppDefined,
            "Failed to compute min/max, no valid pixels found in sampling." );
        return CE_Failure;
    }
    else
    {
        return CE_None;
    }
}

/************************************************************************/
/*                      GDALComputeRasterMinMax()                       */
/************************************************************************/

/**
 * \brief Compute the min/max values for a band.
 * 
 * @see GDALRasterBand::ComputeRasterMinMax()
 */

void CPL_STDCALL 
GDALComputeRasterMinMax( GDALRasterBandH hBand, int bApproxOK, 
                         double adfMinMax[2] )

{
    VALIDATE_POINTER0( hBand, "GDALComputeRasterMinMax" );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    poBand->ComputeRasterMinMax( bApproxOK, adfMinMax );
}

/************************************************************************/
/*                        SetDefaultHistogram()                         */
/************************************************************************/

/* FIXME : add proper documentation */
/**
 * \brief Set default histogram.
 */
CPLErr GDALRasterBand::SetDefaultHistogram( double dfMin, double dfMax, 
                                            int nBuckets, int *panHistogram )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetDefaultHistogram() not implemented for this format." );

    return CE_Failure;
}

/************************************************************************/
/*                      GDALSetDefaultHistogram()                       */
/************************************************************************/

/**
 * \brief Set default histogram.
 *
 * @see GDALRasterBand::SetDefaultHistogram()
 */

CPLErr CPL_STDCALL GDALSetDefaultHistogram( GDALRasterBandH hBand, 
                                            double dfMin, double dfMax, 
                                            int nBuckets, int *panHistogram )

{
    VALIDATE_POINTER1( hBand, "GDALSetDefaultHistogram", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->SetDefaultHistogram( dfMin, dfMax, nBuckets, panHistogram );
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

/**
 * \brief Fetch default Raster Attribute Table.
 *
 * A RAT will be returned if there is a default one associated with the
 * band, otherwise NULL is returned.  The returned RAT is owned by the
 * band and should not be deleted, or altered by the application. 
 * 
 * @return NULL, or a pointer to an internal RAT owned by the band.
 */

const GDALRasterAttributeTable *GDALRasterBand::GetDefaultRAT()

{
    return NULL;
}

/************************************************************************/
/*                         GDALGetDefaultRAT()                          */
/************************************************************************/

/**
 * \brief Fetch default Raster Attribute Table.
 *
 * @see GDALRasterBand::GetDefaultRAT()
 */

GDALRasterAttributeTableH CPL_STDCALL GDALGetDefaultRAT( GDALRasterBandH hBand)

{
    VALIDATE_POINTER1( hBand, "GDALGetDefaultRAT", NULL );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return (GDALRasterAttributeTableH) poBand->GetDefaultRAT();
}

/************************************************************************/
/*                           SetDefaultRAT()                            */
/************************************************************************/

/**
 * \brief Set default Raster Attribute Table.
 *
 * Associates a default RAT with the band.  If not implemented for the
 * format a CPLE_NotSupported error will be issued.  If successful a copy
 * of the RAT is made, the original remains owned by the caller.
 *
 * @param poRAT the RAT to assign to the band.
 *
 * @return CE_None on success or CE_Failure if unsupported or otherwise 
 * failing.
 */

CPLErr GDALRasterBand::SetDefaultRAT( const GDALRasterAttributeTable *poRAT )

{
    if( !(GetMOFlags() & GMO_IGNORE_UNIMPLEMENTED) )
        ReportError( CE_Failure, CPLE_NotSupported,
                  "SetDefaultRAT() not implemented for this format." );

    return CE_Failure;
}

/************************************************************************/
/*                         GDALSetDefaultRAT()                          */
/************************************************************************/

/**
 * \brief Set default Raster Attribute Table.
 *
 * @see GDALRasterBand::GDALSetDefaultRAT()
 */

CPLErr CPL_STDCALL GDALSetDefaultRAT( GDALRasterBandH hBand,
                                      GDALRasterAttributeTableH hRAT )

{
    VALIDATE_POINTER1( hBand, "GDALSetDefaultRAT", CE_Failure );
    VALIDATE_POINTER1( hRAT, "GDALSetDefaultRAT", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);

    return poBand->SetDefaultRAT( 
        static_cast<GDALRasterAttributeTable *>(hRAT) );
}

/************************************************************************/
/*                            GetMaskBand()                             */
/************************************************************************/

/**
 * \brief Return the mask band associated with the band.
 *
 * The GDALRasterBand class includes a default implementation of GetMaskBand() that
 * returns one of four default implementations :
 * <ul>
 * <li>If a corresponding .msk file exists it will be used for the mask band.</li>
 * <li>If the dataset has a NODATA_VALUES metadata item, an instance of the
 *     new GDALNoDataValuesMaskBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA | GMF_PER_DATASET. @since GDAL 1.6.0</li>
 * <li>If the band has a nodata value set, an instance of the new
 *     GDALNodataMaskRasterBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA.</li>
 * <li>If there is no nodata value, but the dataset has an alpha band that seems
 *     to apply to this band (specific rules yet to be determined) and that is
 *     of type GDT_Byte then that alpha band will be returned, and the flags
 *     GMF_PER_DATASET and GMF_ALPHA will be returned in the flags.</li>
 * <li>If neither of the above apply, an instance of the new GDALAllValidRasterBand
 *     class will be returned that has 255 values for all pixels.
 *     The null flags will return GMF_ALL_VALID.</li>
 * </ul>
 *
 * Note that the GetMaskBand() should always return a GDALRasterBand mask, even if it is only
 * an all 255 mask with the flags indicating GMF_ALL_VALID. 
 *
 * @return a valid mask band.
 *
 * @since GDAL 1.5.0
 *
 * @see http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask
 *
 */
GDALRasterBand *GDALRasterBand::GetMaskBand()

{
    if( poMask != NULL )
        return poMask;

/* -------------------------------------------------------------------- */
/*      Check for a mask in a .msk file.                                */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS = GetDataset();

    if( poDS != NULL && poDS->oOvManager.HaveMaskFile() )
    {
        poMask = poDS->oOvManager.GetMaskBand( nBand );
        if( poMask != NULL )
        {
            nMaskFlags = poDS->oOvManager.GetMaskFlags( nBand );
            return poMask;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for NODATA_VALUES metadata.                               */
/* -------------------------------------------------------------------- */
    if (poDS != NULL)
    {
        const char* pszNoDataValues = poDS->GetMetadataItem("NODATA_VALUES");
        if (pszNoDataValues != NULL)
        {
            char** papszNoDataValues = CSLTokenizeStringComplex(pszNoDataValues, " ", FALSE, FALSE);

            /* Make sure we have as many values as bands */
            if (CSLCount(papszNoDataValues) == poDS->GetRasterCount() && poDS->GetRasterCount() != 0)
            {
                /* Make sure that all bands have the same data type */
                /* This is cleraly not a fundamental condition, just a condition to make implementation */
                /* easier. */
                int i;
                GDALDataType eDT = GDT_Unknown;
                for(i=0;i<poDS->GetRasterCount();i++)
                {
                    if (i == 0)
                        eDT = poDS->GetRasterBand(1)->GetRasterDataType();
                    else if (eDT != poDS->GetRasterBand(i + 1)->GetRasterDataType())
                    {
                        break;
                    }
                }
                if (i == poDS->GetRasterCount())
                {
                    nMaskFlags = GMF_NODATA | GMF_PER_DATASET;
                    poMask = new GDALNoDataValuesMaskBand ( poDS );
                    bOwnMask = true;
                    CSLDestroy(papszNoDataValues);
                    return poMask;
                }
                else
                {
                    ReportError(CE_Warning, CPLE_AppDefined,
                            "All bands should have the same type in order the NODATA_VALUES metadata item to be used as a mask.");
                }
            }
            else
            {
                ReportError(CE_Warning, CPLE_AppDefined,
                        "NODATA_VALUES metadata item doesn't have the same number of values as the number of bands.\n"
                        "Ignoring it for mask.");
            }

            CSLDestroy(papszNoDataValues);
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for nodata case.                                          */
/* -------------------------------------------------------------------- */
    int bHaveNoData;

    GetNoDataValue( &bHaveNoData );
    
    if( bHaveNoData )
    {
        nMaskFlags = GMF_NODATA;
        poMask = new GDALNoDataMaskBand( this );
        bOwnMask = true;
        return poMask;
    }

/* -------------------------------------------------------------------- */
/*      Check for alpha case.                                           */
/* -------------------------------------------------------------------- */
    if( poDS != NULL 
        && poDS->GetRasterCount() == 2
        && this == poDS->GetRasterBand(1)
        && poDS->GetRasterBand(2)->GetColorInterpretation() == GCI_AlphaBand
        && poDS->GetRasterBand(2)->GetRasterDataType() == GDT_Byte )
    {
        nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
        poMask = poDS->GetRasterBand(2);
        return poMask;
    }

    if( poDS != NULL 
        && poDS->GetRasterCount() == 4
        && (this == poDS->GetRasterBand(1)
            || this == poDS->GetRasterBand(2)
            || this == poDS->GetRasterBand(3))
        && poDS->GetRasterBand(4)->GetColorInterpretation() == GCI_AlphaBand
        && poDS->GetRasterBand(4)->GetRasterDataType() == GDT_Byte )
    {
        nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
        poMask = poDS->GetRasterBand(4);
        return poMask;
    }

/* -------------------------------------------------------------------- */
/*      Fallback to all valid case.                                     */
/* -------------------------------------------------------------------- */
    nMaskFlags = GMF_ALL_VALID;
    poMask = new GDALAllValidMaskBand( this );
    bOwnMask = true;

    return poMask;
}

/************************************************************************/
/*                          GDALGetMaskBand()                           */
/************************************************************************/

/**
 * \brief Return the mask band associated with the band.
 *
 * @see GDALRasterBand::GetMaskBand()
 */

GDALRasterBandH CPL_STDCALL GDALGetMaskBand( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetMaskBand", NULL );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetMaskBand();
}

/************************************************************************/
/*                            GetMaskFlags()                            */
/************************************************************************/

/**
 * \brief Return the status flags of the mask band associated with the band.
 *
 * The GetMaskFlags() method returns an bitwise OR-ed set of status flags with
 * the following available definitions that may be extended in the future:
 * <ul>
 * <li>GMF_ALL_VALID(0x01): There are no invalid pixels, all mask values will be 255.
 *     When used this will normally be the only flag set.</li>
 * <li>GMF_PER_DATASET(0x02): The mask band is shared between all bands on the dataset.</li>
 * <li>GMF_ALPHA(0x04): The mask band is actually an alpha band and may have values
 *     other than 0 and 255.</li>
 * <li>GMF_NODATA(0x08): Indicates the mask is actually being generated from nodata values.
 *     (mutually exclusive of GMF_ALPHA)</li>
 * </ul>
 *
 * The GDALRasterBand class includes a default implementation of GetMaskBand() that
 * returns one of four default implementations :
 * <ul>
 * <li>If a corresponding .msk file exists it will be used for the mask band.</li>
 * <li>If the dataset has a NODATA_VALUES metadata item, an instance of the
 *     new GDALNoDataValuesMaskBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA | GMF_PER_DATASET. @since GDAL 1.6.0</li>
 * <li>If the band has a nodata value set, an instance of the new
 *     GDALNodataMaskRasterBand class will be returned.
 *     GetMaskFlags() will return GMF_NODATA.</li>
 * <li>If there is no nodata value, but the dataset has an alpha band that seems
 *     to apply to this band (specific rules yet to be determined) and that is
 *     of type GDT_Byte then that alpha band will be returned, and the flags
 *     GMF_PER_DATASET and GMF_ALPHA will be returned in the flags.</li>
 * <li>If neither of the above apply, an instance of the new GDALAllValidRasterBand
 *     class will be returned that has 255 values for all pixels.
 *     The null flags will return GMF_ALL_VALID.</li>
 * </ul>
 *
 * @since GDAL 1.5.0
 *
 * @return a valid mask band.
 *
 * @see http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask
 *
 */
int GDALRasterBand::GetMaskFlags()

{
    // If we don't have a band yet, force this now so that the masks value
    // will be initialized.

    if( poMask == NULL )
        GetMaskBand();

    return nMaskFlags;
}

/************************************************************************/
/*                          GDALGetMaskFlags()                          */
/************************************************************************/

/**
 * \brief Return the status flags of the mask band associated with the band.
 *
 * @see GDALRasterBand::GetMaskFlags()
 */

int CPL_STDCALL GDALGetMaskFlags( GDALRasterBandH hBand )

{
    VALIDATE_POINTER1( hBand, "GDALGetMaskFlags", GMF_ALL_VALID );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->GetMaskFlags();
}

/************************************************************************/
/*                           CreateMaskBand()                           */
/************************************************************************/

/**
 * \brief Adds a mask band to the current band
 *
 * The default implementation of the CreateMaskBand() method is implemented
 * based on similar rules to the .ovr handling implemented using the
 * GDALDefaultOverviews object. A TIFF file with the extension .msk will
 * be created with the same basename as the original file, and it will have
 * as many bands as the original image (or just one for GMF_PER_DATASET).
 * The mask images will be deflate compressed tiled images with the same
 * block size as the original image if possible.
 *
 * Note that if you got a mask band with a previous call to GetMaskBand(),
 * it might be invalidated by CreateMaskBand(). So you have to call GetMaskBand()
 * again.
 *
 * @since GDAL 1.5.0
 *
 * @return CE_None on success or CE_Failure on an error.
 *
 * @see http://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask
 *
 */

CPLErr GDALRasterBand::CreateMaskBand( int nFlags )

{
    if( poDS != NULL && poDS->oOvManager.IsInitialized() )
    {
        CPLErr eErr = poDS->oOvManager.CreateMaskBand( nFlags, nBand );
        if (eErr != CE_None)
            return eErr;

        /* Invalidate existing raster band mask */
        if (bOwnMask)
            delete poMask;
        bOwnMask = false;
        poMask = NULL;

        return CE_None;
    }

    ReportError( CE_Failure, CPLE_NotSupported,
              "CreateMaskBand() not supported for this band." );
    
    return CE_Failure;
}

/************************************************************************/
/*                         GDALCreateMaskBand()                         */
/************************************************************************/

/**
 * \brief Adds a mask band to the current band
 *
 * @see GDALRasterBand::CreateMaskBand()
 */

CPLErr CPL_STDCALL GDALCreateMaskBand( GDALRasterBandH hBand, int nFlags )

{
    VALIDATE_POINTER1( hBand, "GDALCreateMaskBand", CE_Failure );

    GDALRasterBand *poBand = static_cast<GDALRasterBand*>(hBand);
    return poBand->CreateMaskBand( nFlags );
}

/************************************************************************/
/*                    GetIndexColorTranslationTo()                      */
/************************************************************************/

/**
 * \brief Compute translation table for color tables.
 *
 * When the raster band has a palette index, it may be usefull to compute
 * the "translation" of this palette to the palette of another band.
 * The translation tries to do exact matching first, and then approximate
 * matching if no exact matching is possible.
 * This method returns a table such that table[i] = j where i is an index
 * of the 'this' rasterband and j the corresponding index for the reference
 * rasterband.
 *
 * This method is thought as internal to GDAL and is used for drivers
 * like RPFTOC.
 *
 * The implementation only supports 1-byte palette rasterbands.
 *
 * @param poReferenceBand the raster band
 * @param pTranslationTable an already allocated translation table (at least 256 bytes),
 *                          or NULL to let the method allocate it
 * @param pApproximateMatching a pointer to a flag that is set if the matching
 *                              is approximate. May be NULL.
 *
 * @return a translation table if the two bands are palette index and that they do
 *         not match or NULL in other cases.
 *         The table must be freed with CPLFree if NULL was passed for pTranslationTable.
 */

unsigned char* GDALRasterBand::GetIndexColorTranslationTo(GDALRasterBand* poReferenceBand,
                                                          unsigned char* pTranslationTable,
                                                          int* pApproximateMatching )
{
    if (poReferenceBand == NULL)
        return NULL;

    if (poReferenceBand->GetColorInterpretation() == GCI_PaletteIndex &&
        GetColorInterpretation() == GCI_PaletteIndex &&
        poReferenceBand->GetRasterDataType() == GDT_Byte &&
        GetRasterDataType() == GDT_Byte)
    {
        GDALColorTable* srcColorTable = GetColorTable();
        GDALColorTable* destColorTable = poReferenceBand->GetColorTable();
        if (srcColorTable != NULL && destColorTable != NULL)
        {
            int nEntries = srcColorTable->GetColorEntryCount();
            int nRefEntries = destColorTable->GetColorEntryCount();
            int bHasNoDataValueSrc;
            int noDataValueSrc = (int)GetNoDataValue(&bHasNoDataValueSrc);
            int bHasNoDataValueRef;
            int noDataValueRef = (int)poReferenceBand->GetNoDataValue(&bHasNoDataValueRef);
            int samePalette;
            int i, j;

            if (pApproximateMatching)
                *pApproximateMatching = FALSE;

            if (nEntries == nRefEntries && bHasNoDataValueSrc == bHasNoDataValueRef &&
                (bHasNoDataValueSrc == FALSE || noDataValueSrc == noDataValueRef))
            {
                samePalette = TRUE;
                for(i=0;i<nEntries;i++)
                {
                    if (noDataValueSrc == i)
                        continue;
                    const GDALColorEntry* entry = srcColorTable->GetColorEntry(i);
                    const GDALColorEntry* entryRef = destColorTable->GetColorEntry(i);
                    if (entry->c1 != entryRef->c1 ||
                        entry->c2 != entryRef->c2 ||
                        entry->c3 != entryRef->c3)
                    {
                        samePalette = FALSE;
                    }
                }
            }
            else
            {
                samePalette = FALSE;
            }
            if (samePalette == FALSE)
            {
                if (pTranslationTable == NULL)
                    pTranslationTable = (unsigned char*)CPLMalloc(256);

                /* Trying to remap the product palette on the subdataset palette */
                for(i=0;i<nEntries;i++)
                {
                    if (bHasNoDataValueSrc && bHasNoDataValueRef && noDataValueSrc == i)
                        continue;
                    const GDALColorEntry* entry = srcColorTable->GetColorEntry(i);
                    for(j=0;j<nRefEntries;j++)
                    {
                        if (bHasNoDataValueRef && noDataValueRef == j)
                            continue;
                        const GDALColorEntry* entryRef = destColorTable->GetColorEntry(j);
                        if (entry->c1 == entryRef->c1 &&
                            entry->c2 == entryRef->c2 &&
                            entry->c3 == entryRef->c3)
                        {
                            pTranslationTable[i] = (unsigned char) j;
                            break;
                        }
                    }
                    if (j == nEntries)
                    {
                        /* No exact match. Looking for closest color now... */
                        int best_j = 0;
                        int best_distance = 0;
                        if (pApproximateMatching)
                            *pApproximateMatching = TRUE;
                        for(j=0;j<nRefEntries;j++)
                        {
                            const GDALColorEntry* entryRef = destColorTable->GetColorEntry(j);
                            int distance = (entry->c1 - entryRef->c1) * (entry->c1 - entryRef->c1) +
                                           (entry->c2 - entryRef->c2) * (entry->c2 - entryRef->c2) +
                                           (entry->c3 - entryRef->c3) * (entry->c3 - entryRef->c3);
                            if (j == 0 || distance < best_distance)
                            {
                                best_j = j;
                                best_distance = distance;
                            }
                        }
                        pTranslationTable[i] = (unsigned char) best_j;
                    }
                }
                if (bHasNoDataValueRef && bHasNoDataValueSrc)
                    pTranslationTable[noDataValueSrc] = (unsigned char) noDataValueRef;

                return pTranslationTable;
            }
        }
    }
    return NULL;
}

/************************************************************************/
/*                         SetFlushBlockErr()                           */
/************************************************************************/

/**
 * \brief Store that an error occured while writing a dirty block.
 *
 * This function stores the fact that an error occured while writing a dirty
 * block from GDALRasterBlock::FlushCacheBlock(). Indeed when dirty blocks are
 * flushed when the block cache get full, it is not convenient/possible to
 * report that a dirty block could not be written correctly. This function
 * remembers the error and re-issue it from GDALRasterBand::FlushCache(),
 * GDALRasterBand::WriteBlock() and GDALRasterBand::RasterIO(), which are
 * places where the user can easily match the error with the relevant dataset.
 */

void GDALRasterBand::SetFlushBlockErr( CPLErr eErr )
{
    eFlushBlockErr = eErr;
}

/************************************************************************/
/*                            ReportError()                             */
/************************************************************************/

/**
 * \brief Emits an error related to a raster band.
 *
 * This function is a wrapper for regular CPLError(). The only difference
 * with CPLError() is that it prepends the error message with the dataset
 * name and the band number.
 *
 * @param eErrClass one of CE_Warning, CE_Failure or CE_Fatal.
 * @param err_no the error number (CPLE_*) from cpl_error.h.
 * @param fmt a printf() style format string.  Any additional arguments
 * will be treated as arguments to fill in this format in a manner
 * similar to printf().
 *
 * @since GDAL 1.9.0
 */

void GDALRasterBand::ReportError(CPLErr eErrClass, int err_no, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);

    char szNewFmt[256];
    const char* pszDSName = poDS ? poDS->GetDescription() : "";
    if (strlen(fmt) + strlen(pszDSName) + 20 >= sizeof(szNewFmt) - 1)
        pszDSName = CPLGetFilename(pszDSName);
    if (pszDSName[0] != '\0' &&
        strlen(fmt) + strlen(pszDSName) + 20 < sizeof(szNewFmt) - 1)
    {
        snprintf(szNewFmt, sizeof(szNewFmt), "%s, band %d: %s",
                 pszDSName, GetBand(), fmt);
        CPLErrorV( eErrClass, err_no, szNewFmt, args );
    }
    else
    {
        CPLErrorV( eErrClass, err_no, fmt, args );
    }
    va_end(args);
}
