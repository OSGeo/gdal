/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Base class for format specific band class implementation.  This
 *           base class provides default implementation for many methods.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 **********************************************************************
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
 ******************************************************************************
 * $Log$
 * Revision 1.11  2000/03/08 19:59:16  warmerda
 * added GDALFlushRasterCache
 *
 * Revision 1.10  2000/03/06 21:50:37  warmerda
 * added min/max support
 *
 * Revision 1.9  2000/03/06 02:22:01  warmerda
 * added overviews, colour tables, and many other methods
 *
 * Revision 1.8  2000/02/28 16:34:28  warmerda
 * added arg window check in RasterIO()
 *
 * Revision 1.7  1999/11/17 16:18:10  warmerda
 * fixed example code
 *
 * Revision 1.6  1999/10/21 13:24:37  warmerda
 * Fixed some build breaking variable name differences.
 *
 * Revision 1.5  1999/10/01 14:44:02  warmerda
 * added documentation
 *
 * Revision 1.4  1998/12/31 18:54:25  warmerda
 * Implement initial GDALRasterBlock support, and block cache
 *
 * Revision 1.3  1998/12/06 22:17:09  warmerda
 * Fill out rasterio support.
 *
 * Revision 1.2  1998/12/06 02:52:08  warmerda
 * Added new methods, and C cover functions.
 *
 * Revision 1.1  1998/12/03 18:32:01  warmerda
 * New
 */

#include "gdal_priv.h"

/************************************************************************/
/*                           GDALRasterBand()                           */
/************************************************************************/

/*! Constructor. Applications should never create GDALRasterBands directly. */

GDALRasterBand::GDALRasterBand()

{
    poDS = NULL;
    nBand = 0;

    eAccess = GA_ReadOnly;
    nBlockXSize = nBlockYSize = -1;
    eDataType = GDT_Byte;

    nBlocksPerRow = 0;
    nBlocksPerColumn = 0;

    nLoadedBlocks = 0;
    nMaxLoadableBlocks = 0;

    papoBlocks = NULL;
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
}

/************************************************************************/
/*                              RasterIO()                              */
/************************************************************************/

/**
 * Read/write a region of image data for this band.
 *
 * This method allows reading a region of a GDALRasterBand into a buffer,
 * or writing data from a buffer into a region of a GDALRasterBand.  It
 * automatically takes care of data type translation if the data type
 * (eBufType) of the buffer is different than that of the GDALRasterBand.
 * The method also takes care of image decimation / replication if the
 * buffer size (nBufXSize x nBufYSize) is different than the size of the
 * region being accessed (nXSize x nYSize).
 *
 * The nPixelSpace and nLineSpace parameters allow reading into or
 * writing from unusually organized buffers.  This is primarily used
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
 * @param eRWFlag Either GF_Read to read a region of data, or GT_Write to
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
 * @param pData The buffer into which the data should be read, or from which
 * it should be written.  This buffer must contain at least nBufXSize *
 * nBufYSize words of type eBufType.  It is organized in left to right,
 * top to bottom pixel order.  Spacing is controlled by the nPixelSpace,
 * and nLineSpace parameters.
 *
 * @param nBufXSize the width of the buffer into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param nBufYSize the height of the buffer into which the desired region is
 * to be read, or from which it is to be written.
 *
 * @param eBufType the type of the pixel values in the pData data buffer.  The
 * pixel values will automatically be translated to/from the GDALRasterBand
 * data type as needed.
 *
 * @param nPixelSpace The byte offset from the start of one pixel value in
 * pData to the start of the next pixel value within a scanline.  If defaulted
 * the size of the datatype eBufType is used.
 *
 * @param nLineSpace The byte offset from the start of one scanline in
 * pData to the start of the next.  If defaulted the size of the datatype
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
/* -------------------------------------------------------------------- */
/*      If pixel and line spaceing are defaulted assign reasonable      */
/*      value assuming a packed buffer.                                 */
/* -------------------------------------------------------------------- */
    if( nPixelSpace == 0 )
        nPixelSpace = GDALGetDataTypeSize( eBufType ) / 8;
    
    if( nLineSpace == 0 )
        nLineSpace = nPixelSpace * nBufXSize;
    
/* -------------------------------------------------------------------- */
/*      Do some validation of parameters.                               */
/* -------------------------------------------------------------------- */
    if( nXOff < 0 || nXOff + nXSize > nRasterXSize
        || nYOff < 0 || nYOff + nYSize > nRasterYSize )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Access window out of range in RasterIO().  Requested\n"
                  "(%d,%d) of size %dx%d on raster of %dx%d.\n",
                  nXOff, nYOff, nXSize, nYSize, nRasterXSize, nRasterYSize );
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
    
/* -------------------------------------------------------------------- */
/*      Call the format specific function.                              */
/* -------------------------------------------------------------------- */
    return( IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                       pData, nBufXSize, nBufYSize, eBufType,
                       nPixelSpace, nLineSpace ) );
}

/************************************************************************/
/*                            GDALRasterIO()                            */
/************************************************************************/

CPLErr GDALRasterIO( GDALRasterBandH hBand, GDALRWFlag eRWFlag,
                     int nXOff, int nYOff,
                     int nXSize, int nYSize,
                     void * pData,
                     int nBufXSize, int nBufYSize,
                     GDALDataType eBufType,
                     int nPixelSpace,
                     int nLineSpace )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

    return( poBand->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                              pData, nBufXSize, nBufYSize, eBufType,
                              nPixelSpace, nLineSpace ) );
}
                     
/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

/**
 * Read a block of image data efficiently.
 *
 * This method accesses a "natural" block from the raster band without
 * resampling, or data type conversion.  For a more generalized, but
 * potentially less efficient access use RasterIO().
 *
 * This method is the same as the C GDALReadBlock() function.
 *
 * See the GetBlockRef() method for a way of accessing internally cached
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
     int	nXBlocks, nYBlocks, nXBlockSize, nYBlockSize;
     int	iXBlock, iYBlock;

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
             int	nXValid, nYValid;
             
             poBand->ReadBlock( iXBlock, iYBlock, pabyData );

             // Compute the portion of the block that is valid
             // for partial edge blocks.
             if( iXBlock * nXBlockSize > poBand->GetXSize() )
                 nXValid = poBand->GetXSize() - iXBlock * nXBlockSize;
             else
                 nXValid = nXBlockSize;

             if( iYBlock * nYBlockSize > poBand->GetYSize() )
                 nYValid = poBand->GetXSize() - iYBlock * nYBlockSize;
             else
                 nYValid = nYBlockSize;

             // Collect the histogram counts.
             for( int iY = 0; iY < nXValid; iY++ )
             {
                 for( int iX = 0; iX < nXValid; iX++ )
                 {
                     pabyHistogram[pabyData[iX + iY * nXBlockSize]] += 1;
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
    
    if( nXBlockOff < 0
        || nXBlockOff*nBlockXSize >= poDS->GetRasterXSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                  	"GDALRasterBand::ReadBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0
        || nYBlockOff*nBlockYSize >= poDS->GetRasterYSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
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

CPLErr GDALReadBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                      void * pData )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

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
    CPLError( CE_Failure, CPLE_NotSupported,
              "WriteBlock() not supported for this dataset." );
    
    return( CE_Failure );
}

/************************************************************************/
/*                             WriteBlock()                             */
/************************************************************************/

/**
 * Write a block of image data efficiently.
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
 *
 * The following code would efficiently compute a histogram of eight bit
 * raster data.  Note that the final block may be partial ... data beyond
 * the edge of the underlying raster band in these edge blocks is of an
 * undermined value.
 *
 */

CPLErr GDALRasterBand::WriteBlock( int nXBlockOff, int nYBlockOff,
                                   void * pImage )

{
/* -------------------------------------------------------------------- */
/*      Validate arguments.                                             */
/* -------------------------------------------------------------------- */
    CPLAssert( pImage != NULL );
    
    if( nXBlockOff < 0
        || nXBlockOff*nBlockXSize >= poDS->GetRasterXSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nXBlockOff value (%d) in "
                  	"GDALRasterBand::WriteBlock()\n",
                  nXBlockOff );

        return( CE_Failure );
    }

    if( nYBlockOff < 0
        || nYBlockOff*nBlockYSize >= poDS->GetRasterYSize() )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nYBlockOff value (%d) in "
                  	"GDALRasterBand::WriteBlock()\n",
                  nYBlockOff );

        return( CE_Failure );
    }

    if( eAccess == GA_ReadOnly )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Attempt to write to read only dataset in"
                  "GDALRasterBand::WriteBlock().\n" );

        return( CE_Failure );
    }
    
/* -------------------------------------------------------------------- */
/*      Invoke underlying implementation method.                        */
/* -------------------------------------------------------------------- */
    return( IWriteBlock( nXBlockOff, nYBlockOff, pImage ) );
}

/************************************************************************/
/*                           GDALWriteBlock()                           */
/************************************************************************/

CPLErr GDALWriteBlock( GDALRasterBandH hBand, int nXOff, int nYOff,
                       void * pData )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

    return( poBand->WriteBlock( nXOff, nYOff, pData ) );
}


/************************************************************************/
/*                         GetRasterDataType()                          */
/************************************************************************/

/**
 * Fetch the pixel data type for this band.
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

GDALDataType GDALGetRasterDataType( GDALRasterBandH hBand )

{
    return( ((GDALRasterBand *) hBand)->GetRasterDataType() );
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

/**
 * Fetch the "natural" block size of this band.
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
    CPLAssert( nBlockXSize > 0 && nBlockYSize > 0 );
    
    if( pnXSize != NULL )
        *pnXSize = nBlockXSize;
    if( pnYSize != NULL )
        *pnYSize = nBlockYSize;
}

/************************************************************************/
/*                          GDALGetBlockSize()                          */
/************************************************************************/

void GDALGetBlockSize( GDALRasterBandH hBand, int * pnXSize, int * pnYSize )

{
    GDALRasterBand	*poBand = (GDALRasterBand *) hBand;

    poBand->GetBlockSize( pnXSize, pnYSize );
}

/************************************************************************/
/*                           InitBlockInfo()                            */
/************************************************************************/

void GDALRasterBand::InitBlockInfo()

{
    if( papoBlocks != NULL )
        return;

    CPLAssert( nBlockXSize > 0 && nBlockYSize > 0 );
    
    nBlocksPerRow = (poDS->GetRasterXSize()+nBlockXSize-1) / nBlockXSize;
    nBlocksPerColumn = (poDS->GetRasterYSize()+nBlockYSize-1) / nBlockYSize;
    
    papoBlocks = (GDALRasterBlock **)
        CPLCalloc( sizeof(void*), nBlocksPerRow * nBlocksPerColumn );

/* -------------------------------------------------------------------- */
/*      Don't override caching info if the subclass has already set     */
/*      it.  Eventually I imagine the application should be mostly      */
/*      in control of this.                                             */
/* -------------------------------------------------------------------- */
    if( nMaxLoadableBlocks < 1 )
    {
        nMaxLoadableBlocks = nBlocksPerRow;
    }
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

CPLErr GDALRasterBand::AdoptBlock( int nBlockXOff, int nBlockYOff,
                                   GDALRasterBlock * poBlock )

{
    int		nBlockIndex;
    
    InitBlockInfo();
    
    CPLAssert( nBlockXOff >= 0 && nBlockXOff < nBlocksPerRow );
    CPLAssert( nBlockYOff >= 0 && nBlockYOff < nBlocksPerColumn );

    nBlockIndex = nBlockXOff + nBlockYOff * nBlocksPerRow;
    if( papoBlocks[nBlockIndex] == poBlock )
        return( CE_None );

    if( papoBlocks[nBlockIndex] != NULL )
        FlushBlock( nBlockXOff, nBlockYOff );

    papoBlocks[nBlockIndex] = poBlock;
    poBlock->Touch();

    nLoadedBlocks++;
    if( nLoadedBlocks > nMaxLoadableBlocks )
        FlushBlock();

    return( CE_None );
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

/**
 * Flush raster data cache.
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
    CPLErr	eErr = CE_None;

    while( nLoadedBlocks > 0 && eErr == CE_None )
    {
        eErr = FlushBlock();
    }
    
    return( eErr );
}

/************************************************************************/
/*                        GDALFlushRasterCache()                        */
/************************************************************************/

CPLErr GDALFlushRasterCache( GDALRasterBandH hBand )

{
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

CPLErr GDALRasterBand::FlushBlock( int nBlockXOff, int nBlockYOff )

{
    int		nBlockIndex;
    GDALRasterBlock *poBlock;
    CPLErr	eErr = CE_None;
        
    InitBlockInfo();
    
/* -------------------------------------------------------------------- */
/*      Select a block if none indicated.                               */
/*                                                                      */
/*      Currently we scan all possible blocks, but for efficiency in    */
/*      cases with many blocks we should eventually modify the          */
/*      GDALRasterBand to keep a linked list of blocks by age.          */
/* -------------------------------------------------------------------- */
    if( nBlockXOff == -1 || nBlockYOff == -1 )
    {
        int		i, nBlocks;
        int		nOldestAge = 0x7fffffff;
        int		nOldestBlock = -1;

        nBlocks = nBlocksPerRow * nBlocksPerColumn;

        for( i = 0; i < nBlocks; i++ )
        {
            if( papoBlocks[i] != NULL
                && papoBlocks[i]->GetAge() < nOldestAge )
            {
                nOldestAge = papoBlocks[i]->GetAge();
                nOldestBlock = i;
            }
        }

        if( nOldestBlock == -1 )
            return( CE_None );

        nBlockXOff = nOldestBlock % nBlocksPerRow;
        nBlockYOff = (nOldestBlock - nBlockXOff) / nBlocksPerRow;
    }

/* -------------------------------------------------------------------- */
/*      Validate                                                        */
/* -------------------------------------------------------------------- */
    CPLAssert( nBlockXOff >= 0 && nBlockXOff < nBlocksPerRow );
    CPLAssert( nBlockYOff >= 0 && nBlockYOff < nBlocksPerColumn );

    nBlockIndex = nBlockXOff + nBlockYOff * nBlocksPerRow;
    poBlock = papoBlocks[nBlockIndex];
    if( poBlock == NULL )
        return( CE_None );

/* -------------------------------------------------------------------- */
/*      Remove, and update count.                                       */
/* -------------------------------------------------------------------- */
    papoBlocks[nBlockIndex] = NULL;
    nLoadedBlocks--;

    CPLAssert( nLoadedBlocks >= 0 );
    
/* -------------------------------------------------------------------- */
/*      Is the target block dirty?  If so we need to write it.          */
/* -------------------------------------------------------------------- */
    if( poBlock->GetDirty() )
    {
        eErr = IWriteBlock( nBlockXOff, nBlockYOff, poBlock->GetDataRef() );
        poBlock->MarkClean();
    }

/* -------------------------------------------------------------------- */
/*      Deallocate the block;                                           */
/* -------------------------------------------------------------------- */
    delete poBlock;

    return( eErr );
}


/************************************************************************/
/*                            GetBlockRef()                             */
/************************************************************************/

/**
 * Fetch a pointer to an internally cached raster block.
 *
 * Note that calling GetBlockRef() on a previously uncached band will
 * enable caching.
 * 
 * @param nXBlockOff the horizontal block offset, with zero indicating
 * the left most block, 1 the next block and so forth. 
 *
 * @param nYBlockOff the vertical block offset, with zero indicating
 * the left most block, 1 the next block and so forth.
 *
 * @return pointer to the block object, or NULL on failure.
 */

GDALRasterBlock * GDALRasterBand::GetBlockRef( int nXBlockOff,
                                               int nYBlockOff )

{
    int		nBlockIndex;

    InitBlockInfo();
    
/* -------------------------------------------------------------------- */
/*      Validate the request                                            */
/* -------------------------------------------------------------------- */
    if( nXBlockOff < 0 || nXBlockOff >= nBlocksPerRow )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockXOff value (%d) in "
                  	"GDALRasterBand::GetBlockRef()\n",
                  nXBlockOff );

        return( NULL );
    }

    if( nYBlockOff < 0 || nYBlockOff >= nBlocksPerColumn )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Illegal nBlockYOff value (%d) in "
                  	"GDALRasterBand::GetBlockRef()\n",
                  nYBlockOff );

        return( NULL );
    }

/* -------------------------------------------------------------------- */
/*      If the block isn't already in the cache, we will need to        */
/*      create it, read into it, and adopt it.  Adopting it may         */
/*      flush an old tile from the cache.                               */
/* -------------------------------------------------------------------- */
    nBlockIndex = nXBlockOff + nYBlockOff * nBlocksPerRow;
    
    if( papoBlocks[nBlockIndex] == NULL )
    {
        GDALRasterBlock	*poBlock;
        
        poBlock = new GDALRasterBlock( nBlockXSize, nBlockYSize,
                                       eDataType, NULL );

        /* allocate data space */
        if( poBlock->Internalize() != CE_None )
        {
            delete poBlock;

            return( NULL );
        }

        if( IReadBlock(nXBlockOff,nYBlockOff,poBlock->GetDataRef()) != CE_None)
        {
            delete poBlock;
            return( NULL );
        }

        AdoptBlock( nXBlockOff, nYBlockOff, poBlock );
    }

/* -------------------------------------------------------------------- */
/*      Every read access updates the last touched time.                */
/* -------------------------------------------------------------------- */
    if( papoBlocks[nBlockIndex] != NULL )
        papoBlocks[nBlockIndex]->Touch();

    return( papoBlocks[nBlockIndex] );
}

/************************************************************************/
/*                             GetAccess()                              */
/************************************************************************/

/**
 * Find out if we have update permission for this band.
 *
 * @return Either GA_Update or GA_ReadOnly.
 */

GDALAccess GDALRasterBand::GetAccess()

{
    return eAccess;
}

/************************************************************************/
/*                          GetCategoryNames()                          */
/************************************************************************/

/**
 * Fetch the list of category names for this raster.
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
/*                           GetNoDataValue()                           */
/************************************************************************/

/**
 * Fetch the no data value for this band.
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

double GDALGetRasterNoDataValue( GDALRasterBandH hBand, int *pbSuccess )

{
    return ((GDALRasterBand *) hBand)->GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

/**
 * Fetch the maximum value for this band.
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
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    switch( eDataType )
    {
      case GDT_Byte:
        return 255;

      case GDT_UInt16:
        return 65535;

      case GDT_Int16:
        return 32767;

      case GDT_Int32:
        return 2147483647.0;

      case GDT_UInt32:
        return 4294967295.0;

      case GDT_Float32:
        return 4294967295.0; /* not actually accurate */

      case GDT_Float64:
        return 4294967295.0; /* not actually accurate */

      default:
        return 4294967295.0; /* not actually accurate */
    }
}

/************************************************************************/
/*                        GDALGetRasterMaximum()                        */
/************************************************************************/

double GDALGetRasterMaximum( GDALRasterBandH hBand, int *pbSuccess )

{
    return ((GDALRasterBand *) hBand)->GetMaximum( pbSuccess );
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

/**
 * Fetch the minimum value for this band.
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
    if( pbSuccess != NULL )
        *pbSuccess = FALSE;

    switch( eDataType )
    {
      case GDT_Byte:
        return 0;

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

double GDALGetRasterMinimum( GDALRasterBandH hBand, int *pbSuccess )

{
    return ((GDALRasterBand *) hBand)->GetMinimum( pbSuccess );
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

/**
 * How should this band be interpreted as color?
 *
 * CV_Undefined is returned when the format doesn't know anything
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

GDALColorInterp GDALGetRasterColorInterpretation( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetColorInterpretation();
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

/**
 * Fetch the color table associated with band.
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

GDALColorTableH GDALGetRasterColorTable( GDALRasterBandH hBand )

{
    return (GDALColorTableH) ((GDALRasterBand *) hBand)->GetColorTable();
}

/************************************************************************/
/*                       HasArbitraryOverviews()                        */
/************************************************************************/

/**
 * Check for arbitrary overviews.
 *
 * This returns TRUE if the underlying datastore can compute arbitrary 
 * overviews efficiently, such as is the case with OGDI over a network. 
 * Datastores with arbitrary overviews don't generally have any fixed
 * overviews, but the RasterIO() method can be used in downsampling mode
 * to get overview data efficiently.
 *
 * @return TRUE if arbitrary overviews available (efficiently), otherwise
 * FALSE. 
 */

int GDALRasterBand::HasArbitraryOverviews()

{
    return FALSE;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

/**
 * Return the number of overview layers available.
 *
 * This method is the same as the C function GDALGetOverviewCount();
 *
 * @return overview count, zero if none.
 */

int GDALRasterBand::GetOverviewCount()

{
    return 0;
}

/************************************************************************/
/*                        GDALGetOverviewCount()                        */
/************************************************************************/

int GDALGetOverviewCount( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetOverviewCount();
}


/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

/**
 * Fetch overview raster band object.
 *
 * This function is the same as the C function GDALGetOverview().
 * 
 * @param i overview index between 0 and GetOverviewCount()-1.
 * 
 * @return overview GDALRasterBand.
 */

GDALRasterBand * GDALRasterBand::GetOverview( int i )

{
    (void) i;

    return NULL;
}

/************************************************************************/
/*                          GDALGetOverview()                           */
/************************************************************************/

GDALRasterBandH GDALGetOverview( GDALRasterBandH hBand, int i )

{
    return (GDALRasterBandH) ((GDALRasterBand *) hBand)->GetOverview(i);
}


/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

/**
 * Fetch the raster value offset.
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
/*                              GetScale()                              */
/************************************************************************/

/**
 * Fetch the raster value scale.
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
/*                           GetDescription()                           */
/************************************************************************/

/**
 * Return a description of this band.
 *
 * @return internal description string, or "" if none is available.
 */

const char *GDALRasterBand::GetDescription()

{
    return "";
}


/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

/**
 * Return raster unit type.
 *
 * Return a name for the units of this raster's values.  For instance, it
 * might be "m" for an elevation model in meters.  If no units are available,
 * a value of "" will be returned.  The returned string should not be
 * modified, nor freed by the calling application.
 *
 * @return unit name string.
 */

const char *GDALRasterBand::GetUnitType()

{
    return "";
}

/************************************************************************/
/*                              GetXSize()                              */
/************************************************************************/

/**
 * Fetch XSize of raster. 
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

int GDALGetRasterBandXSize( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetXSize();
}

/************************************************************************/
/*                              GetYSize()                              */
/************************************************************************/

/**
 * Fetch YSize of raster. 
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

int GDALGetRasterBandYSize( GDALRasterBandH hBand )

{
    return ((GDALRasterBand *) hBand)->GetYSize();
}
